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
    
    ESP_LOGV(ADAPTER_TAG, "[%s] BLE TX: %s", parent_->log_name(),
             TeslaBLE::format_hex(data.data(), data.size()).c_str());
    
    // Fragment message
    size_t fragments = 0;
    for (size_t i = 0; i < data.size(); i += BLOCK_LENGTH) {
        size_t chunk_len = std::min(BLOCK_LENGTH, data.size() - i);
        std::vector<uint8_t> chunk(data.begin() + i, data.begin() + i + chunk_len);
        
        write_queue_.emplace(chunk, ESP_GATT_WRITE_TYPE_NO_RSP, ESP_GATT_AUTH_REQ_NONE);
        ++fragments;
    }

    parent_->note_link_activity();
    const auto queued = tx_tracker_.on_message_queued(millis(), data.size(), fragments);
    if (queued.messages_ahead > 0) {
        // A new message while an earlier one is still being transmitted is
        // usually the library resending a command it thinks timed out.
        ESP_LOGW(ADAPTER_TAG,
                 "[%s] TX msg #%u queued: %u bytes, %u fragments, behind %u unsent fragments of %u earlier message(s)",
                 parent_->log_name(), (unsigned) queued.seq, (unsigned) data.size(), (unsigned) fragments,
                 (unsigned) queued.fragments_ahead, (unsigned) queued.messages_ahead);
    } else {
        ESP_LOGD(ADAPTER_TAG, "[%s] TX msg #%u queued: %u bytes, %u fragments", parent_->log_name(),
                 (unsigned) queued.seq, (unsigned) data.size(), (unsigned) fragments);
    }
    
    return true;
}

void BleAdapterImpl::process_write_queue() {
    if (write_queue_.empty()) return;
    if (!parent_->is_connected()) return;
    if (write_in_flight_) return;
    const uint32_t now = millis();
    // Congestion is per link: only this car waits, the other keeps sending.
    if (congestion_gate_.blocked(now)) return;
    if (congestion_gate_.expired(now)) {
        ESP_LOGW(ADAPTER_TAG, "[%s] No congestion-cleared event after %u ms - resuming writes",
                 parent_->log_name(), (unsigned) CongestionGate::MAX_WAIT_MS);
        congestion_gate_.reset();
    }
    if (static_cast<int32_t>(now - next_write_ms_) < 0) return;

    // Back off a failing chunk instead of retrying every loop() iteration,
    // and drop it after repeated failures so it cannot block newer traffic.
    switch (write_retry_policy_.next_action(millis())) {
        case WriteAttemptDecision::WAIT:
            return;
        case WriteAttemptDecision::DROP:
            ESP_LOGE(ADAPTER_TAG, "[%s] Dropping TX chunk after %u consecutive failures (%u bytes)",
                     parent_->log_name(), (unsigned) WriteRetryPolicy::MAX_CONSECUTIVE_FAILURES,
                     (unsigned) write_queue_.front().data.size());
            finish_head_fragment_(false, false);
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
        // The call only queues the write in ESP-IDF. Do not discard this
        // fragment until ESP_GATTC_WRITE_CHAR_EVT confirms completion.
        write_in_flight_ = true;
    } else {
        write_retry_policy_.on_failure(millis());
        next_write_ms_ = millis() + std::max<uint32_t>(CONGESTION_GAP_MS, parent_->ble_write_gap_ms());
        ESP_LOGW(ADAPTER_TAG, "[%s] BLE write submit failed: %s", parent_->log_name(), esp_err_to_name(err));
    }
}

void BleAdapterImpl::on_write_complete(esp_gatt_status_t status) {
    if (!write_in_flight_) return;
    write_in_flight_ = false;

    switch (classify_write_status(status)) {
        case WriteOutcome::SENT:
            finish_head_fragment_(true, false);
            write_retry_policy_.on_success(millis());
            next_write_ms_ = millis() + parent_->ble_write_gap_ms();
            return;

        case WriteOutcome::SENT_CONGESTED:
            // 143 means the stack accepted the fragment but this link is now
            // congested. Never resend it (that duplicates bytes inside the
            // Tesla frame). Pause this link only, until ESP_GATTC_CONGEST_EVT
            // clears it; the other car is not held back.
            if (!congestion_gate_.congested()) {
                ESP_LOGD(ADAPTER_TAG, "[%s] Link congested (143) - fragment accepted, pausing this link",
                         parent_->log_name());
            }
            congestion_gate_.on_congested(millis());
            finish_head_fragment_(true, true);
            write_retry_policy_.on_success(millis());
            next_write_ms_ = millis() + parent_->ble_write_gap_ms();
            return;

        case WriteOutcome::FAILED:
            break;
    }

    // Keep the same fragment at the front and retry it with backoff: the
    // stack did not accept it, so dropping it would corrupt the message.
    write_retry_policy_.on_failure(millis());
    next_write_ms_ = millis() + std::max<uint32_t>(CONGESTION_GAP_MS, parent_->ble_write_gap_ms());
    ESP_LOGW(ADAPTER_TAG, "[%s] BLE write completion failed: %d", parent_->log_name(), status);
}

void BleAdapterImpl::on_congest_event(bool congested) {
    if (congested) {
        congestion_gate_.on_congested(millis());
        return;
    }
    const uint32_t waited = congestion_gate_.on_uncongested(millis());
    if (waited > 0) {
        ESP_LOGD(ADAPTER_TAG, "[%s] Link congestion cleared after %u ms", parent_->log_name(), (unsigned) waited);
    }
}

void BleAdapterImpl::finish_head_fragment_(bool sent, bool congested) {
    if (!write_queue_.empty()) write_queue_.pop();

    TxMessageTracker::Completed done;
    if (!tx_tracker_.on_fragment_done(millis(), sent, congested, &done)) return;

    if (done.dropped > 0) {
        ESP_LOGW(ADAPTER_TAG, "[%s] TX msg #%u incomplete: %u of %u fragments dropped - the car will discard it",
                 parent_->log_name(), (unsigned) done.seq, (unsigned) done.dropped, (unsigned) done.fragments);
    } else if (done.duration_ms >= TxMessageTracker::LIBRARY_RESEND_MS) {
        ESP_LOGW(ADAPTER_TAG,
                 "[%s] TX msg #%u sent in %u ms (%u bytes, %u fragments, %u congested) - slower than the "
                 "library's %u ms resend timer, expect a duplicate request",
                 parent_->log_name(), (unsigned) done.seq, (unsigned) done.duration_ms, (unsigned) done.bytes,
                 (unsigned) done.fragments, (unsigned) done.congested,
                 (unsigned) TxMessageTracker::LIBRARY_RESEND_MS);
    } else {
        ESP_LOGD(ADAPTER_TAG, "[%s] TX msg #%u sent in %u ms (%u bytes, %u fragments, %u congested)",
                 parent_->log_name(), (unsigned) done.seq, (unsigned) done.duration_ms, (unsigned) done.bytes,
                 (unsigned) done.fragments, (unsigned) done.congested);
    }
}

void BleAdapterImpl::clear_queues() {
    std::queue<BLETXChunk> empty;
    write_queue_.swap(empty);
    write_in_flight_ = false;
    next_write_ms_ = millis();  // "now", not 0: the signed comparison must stay valid after 24.8 days
    write_retry_policy_.reset();
    congestion_gate_.reset();
    tx_tracker_.clear();
}

// --- StorageAdapterImpl ---

StorageAdapterImpl::StorageAdapterImpl(const std::string& storage_namespace)
    : storage_handle_(0), initialized_(false), storage_namespace_(storage_namespace) {}

StorageAdapterImpl::~StorageAdapterImpl() {
    if (storage_handle_ != 0) {
        nvs_close(storage_handle_);
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
    
    // Every Tesla gets an isolated NVS namespace containing its own private
    // key and sessions. There is intentionally no migration from the legacy
    // global "storage" namespace.
    err = nvs_open(storage_namespace_.c_str(), NVS_READWRITE, &storage_handle_);
    if (err != ESP_OK) return false;

    initialized_ = true;
    return true;
}

const char* StorageAdapterImpl::map_key(const std::string& key) {
    if (key == "session_vcsec") return "tk_vcsec";
    if (key == "session_infotainment") return "tk_info";
    if (key == "private_key") return "private_key";
    return nullptr;
}

bool StorageAdapterImpl::load(const std::string& key, std::vector<uint8_t>& buffer) {
    if (!initialized_) return false;
    
    const char* nvs_key = map_key(key);
    if (!nvs_key) return false;
    
    nvs_handle_t handle = storage_handle_;
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
    
    nvs_handle_t handle = storage_handle_;
    esp_err_t err = nvs_set_blob(handle, nvs_key, buffer.data(), buffer.size());
    if (err != ESP_OK) return false;

    return nvs_commit(handle) == ESP_OK;
}

bool StorageAdapterImpl::remove(const std::string& key) {
    if (!initialized_) return false;
    
    const char* nvs_key = map_key(key);
    if (!nvs_key) return false;
    
    nvs_handle_t handle = storage_handle_;
    esp_err_t err = nvs_erase_key(handle, nvs_key);
    return (err == ESP_OK) && (nvs_commit(handle) == ESP_OK);
}

} // namespace tesla_ble_vehicle
} // namespace esphome
