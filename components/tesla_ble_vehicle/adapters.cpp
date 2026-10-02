#include "ble_adapter_impl.h"
#include "storage_adapter_impl.h"
#include "tesla_ble_vehicle.h"
#include <esphome/core/log.h>
#include <tb_utils.h>
#include <algorithm>

namespace esphome {
namespace tesla_ble_vehicle {

static const char *ADAPTER_TAG = "tesla_ble_adapters";

// --- BleAdapterImpl ---

BleAdapterImpl::BleAdapterImpl(TeslaBLEVehicle* parent) : parent_(parent) {}

void BleAdapterImpl::connect(const std::string& address) {
    // ESPHome handles connection
}

void BleAdapterImpl::disconnect() {
    if (parent_) {
        if (parent_->ble_client() != nullptr) parent_->ble_client()->disconnect();
    }
}

bool BleAdapterImpl::write(const std::vector<uint8_t>& data) {
    if (!parent_->is_connected()) return false;
    
    ESP_LOGV(ADAPTER_TAG, "BLE TX: %s", TeslaBLE::format_hex(data.data(), data.size()).c_str());
    
    // Fragment message
    for (size_t i = 0; i < data.size(); i += BLOCK_LENGTH) {
        size_t chunk_len = std::min(BLOCK_LENGTH, data.size() - i);
        std::vector<uint8_t> chunk(data.begin() + i, data.begin() + i + chunk_len);
        
        write_queue_.emplace(chunk, ESP_GATT_WRITE_TYPE_NO_RSP, ESP_GATT_AUTH_REQ_NONE);
    }
    
    return true;
}

void BleAdapterImpl::process_write_queue() {
    if (write_queue_.empty()) return;
    if (!parent_->is_connected()) return;

    // Back off a failing chunk instead of retrying every loop() iteration,
    // and drop it after repeated failures so it cannot block newer traffic.
    switch (write_retry_policy_.next_action(millis())) {
        case WriteAttemptDecision::WAIT:
            return;
        case WriteAttemptDecision::DROP:
            ESP_LOGE(ADAPTER_TAG, "Dropping TX chunk after %u consecutive failures (%u bytes)",
                     (unsigned) WriteRetryPolicy::MAX_CONSECUTIVE_FAILURES,
                     (unsigned) write_queue_.front().data.size());
            write_queue_.pop();
            write_retry_policy_.on_drop();
            return;
        case WriteAttemptDecision::ATTEMPT:
            break;
    }

    BLETXChunk& chunk = write_queue_.front();

    auto* client = parent_->ble_client();
    int gattc_if = client->get_gattc_if();
    uint16_t conn_id = client->get_conn_id();
    uint16_t handle = parent_->get_write_handle(); // Need public getter on Vehicle

    if (handle == 0) {
        // Not ready
        return;
    }

    esp_err_t err = esp_ble_gattc_write_char(
        gattc_if, conn_id, handle,
        chunk.data.size(), chunk.data.data(),
        chunk.write_type, chunk.auth_req
    );

    if (err == ESP_OK) {
        write_retry_policy_.on_success(millis());
        write_queue_.pop();
    } else {
        write_retry_policy_.on_failure(millis());
        ESP_LOGW(ADAPTER_TAG, "BLE write failed: %s", esp_err_to_name(err));
    }
}

void BleAdapterImpl::clear_queues() {
    std::queue<BLETXChunk> empty;
    write_queue_.swap(empty);
    write_retry_policy_.reset();
}

// --- StorageAdapterImpl ---

StorageAdapterImpl::StorageAdapterImpl(const std::string& storage_namespace)
    : global_handle_(0), session_handle_(0), initialized_(false),
      storage_namespace_(storage_namespace) {}

StorageAdapterImpl::~StorageAdapterImpl() {
    if (global_handle_ != 0) {
        nvs_close(global_handle_);
    }
    if (session_handle_ != 0) {
        nvs_close(session_handle_);
    }
}

bool StorageAdapterImpl::initialize() {
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        // Standard recovery for a changed/full NVS layout. Must not abort: a
        // failed erase would boot-loop the device. Degrading to no
        // persistence keeps the vehicle connection working; the component
        // already handles initialize() == false.
        ESP_LOGW(ADAPTER_TAG, "NVS needs erase (0x%x): erasing NVS partition", (int) err);
        esp_err_t erase_err = nvs_flash_erase();
        if (erase_err != ESP_OK) {
            ESP_LOGE(ADAPTER_TAG, "nvs_flash_erase failed: %s - continuing without persistence",
                     esp_err_to_name(erase_err));
            return false;
        }
        err = nvs_flash_init();
        if (err != ESP_OK) {
            ESP_LOGE(ADAPTER_TAG, "nvs_flash_init after erase failed: %s - continuing without persistence",
                     esp_err_to_name(err));
            return false;
        }
    }

    if (err != ESP_OK) {
        ESP_LOGE(ADAPTER_TAG, "nvs_flash_init failed: %s - continuing without persistence", esp_err_to_name(err));
        return false;
    }
    
    // Keep the Tesla private key in the legacy global namespace so one ESP
    // identity can be paired with multiple vehicles and existing keys migrate
    // without regeneration.
    err = nvs_open("storage", NVS_READWRITE, &global_handle_);
    if (err != ESP_OK) return false;

    // Session blobs are vehicle-specific and must never be shared across VINs.
    err = nvs_open(storage_namespace_.c_str(), NVS_READWRITE, &session_handle_);
    if (err != ESP_OK) {
        nvs_close(global_handle_);
        global_handle_ = 0;
        return false;
    }

    initialized_ = true;
    return true;
}

const char* StorageAdapterImpl::map_key(const std::string& key) {
    // The private key stays global so one ESP identity can be paired to
    // multiple vehicles. Session state is per vehicle via the NVS namespace.
    if (key == "session_vcsec") return "tk_vcsec";
    if (key == "session_infotainment") return "tk_info";
    if (key == "private_key") return "private_key";
    return nullptr;
}

bool StorageAdapterImpl::load(const std::string& key, std::vector<uint8_t>& buffer) {
    if (!initialized_) return false;
    
    const char* nvs_key = map_key(key);
    if (!nvs_key) return false;
    
    nvs_handle_t handle = key == "private_key" ? global_handle_ : session_handle_;
    size_t required_size = 0;
    esp_err_t err = nvs_get_blob(handle, nvs_key, nullptr, &required_size);
    if (err != ESP_OK || required_size == 0) return false;
    
    buffer.resize(required_size);
    err = nvs_get_blob(handle, nvs_key, buffer.data(), &required_size);
    return err == ESP_OK;
}

bool StorageAdapterImpl::save(const std::string& key, const std::vector<uint8_t>& buffer) {
    if (!initialized_) return false;
    
    const char* nvs_key = map_key(key);
    if (!nvs_key) return false;
    
    nvs_handle_t handle = key == "private_key" ? global_handle_ : session_handle_;
    esp_err_t err = nvs_set_blob(handle, nvs_key, buffer.data(), buffer.size());
    if (err != ESP_OK) return false;

    return nvs_commit(handle) == ESP_OK;
}

bool StorageAdapterImpl::remove(const std::string& key) {
    if (!initialized_) return false;
    
    const char* nvs_key = map_key(key);
    if (!nvs_key) return false;
    
    nvs_handle_t handle = key == "private_key" ? global_handle_ : session_handle_;
    esp_err_t err = nvs_erase_key(handle, nvs_key);
    return (err == ESP_OK) && (nvs_commit(handle) == ESP_OK);
}

} // namespace tesla_ble_vehicle
} // namespace esphome
