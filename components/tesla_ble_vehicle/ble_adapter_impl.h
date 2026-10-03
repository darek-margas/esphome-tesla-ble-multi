#pragma once

#include "adapters.h"
#include "gatt_tx_policy.h"
#include "write_retry_policy.h"
#include <esphome/components/esp32_ble_client/ble_client_base.h>
#include <esphome/core/log.h>
#include <vector>
#include <queue>
#include <esp_gattc_api.h>

namespace esphome {
namespace tesla_ble_vehicle {

class TeslaBLEVehicle; // Forward declaration

struct BLETXChunk {
    std::vector<uint8_t> data;
    esp_gatt_write_type_t write_type;
    esp_gatt_auth_req_t auth_req;

    BLETXChunk(std::vector<uint8_t> d, esp_gatt_write_type_t wt, esp_gatt_auth_req_t ar)
        : data(std::move(d)), write_type(wt), auth_req(ar) {}
};

class BleAdapterImpl : public TeslaBLE::BleAdapter {
public:
    explicit BleAdapterImpl(TeslaBLEVehicle* parent);

    void connect(const std::string& address) override;
    void disconnect() override;
    bool write(const std::vector<uint8_t>& data) override;

    // Custom method to be called by TeslaBLEVehicle loop
    void process_write_queue();
    
    // Called from the owning GATT client when ESP-IDF reports completion.
    void on_write_complete(esp_gatt_status_t status);

    // Called on ESP_GATTC_CONGEST_EVT for this link.
    void on_congest_event(bool congested);

    // Clear queues (on disconnect)
    void clear_queues();

    // True when nothing is waiting to be written.
    bool tx_idle() const { return write_queue_.empty() && !write_in_flight_; }

private:
    TeslaBLEVehicle* parent_;
    std::queue<BLETXChunk> write_queue_;
    WriteRetryPolicy write_retry_policy_;
    bool write_in_flight_{false};

    // Pops the head fragment and updates per-message diagnostics.
    void finish_head_fragment_(bool sent, bool congested);

    CongestionGate congestion_gate_;
    TxMessageTracker tx_tracker_;

    // ESP32's GATT client path is shared across all Tesla BLE links. Only one
    // Tesla fragment may be outstanding globally, not merely per vehicle.
    static BleAdapterImpl *global_write_owner_;
    static uint32_t global_next_write_ms_;
    
    static constexpr uint32_t CONGESTION_GAP_MS = 100;
    static const size_t BLOCK_LENGTH = 18; // Safe BLE MTU chunk size
};

} // namespace tesla_ble_vehicle
} // namespace esphome
