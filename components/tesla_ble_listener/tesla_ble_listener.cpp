#include "tesla_ble_listener.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include <string>
#include <vin_utils.h>

namespace esphome {
namespace tesla_ble_listener {
static const char *const TAG = "tesla_ble_listener";

// Log each Tesla at most this often, so a car parked next to the ESP32 does
// not flood the log with every advert.
static const uint32_t RELOG_INTERVAL_MS = 60000;

std::string get_vin_advertisement_name(const char *vin) {
  std::string result = TeslaBLE::get_vin_advertisement_name(vin);
  ESP_LOGD(TAG, "VIN advertisement name: %s", result.c_str());
  return result;
}

bool TeslaBLEListener::parse_device(
    const esp32_ble_tracker::ESPBTDevice &device) {
  const std::string &name = device.get_name();
  char addr_buf[MAC_ADDRESS_PRETTY_BUFFER_SIZE];
  const char *addr_str = device.address_str_to(addr_buf);
  ESP_LOGV(TAG, "Parsing device: [%s]: %s", addr_str, name.c_str());

  if (!this->vin_ad_name_.empty()) {
    if (name == this->vin_ad_name_) {
      ESP_LOGI(TAG, "Found Tesla vehicle | Name: %s | MAC: %s", name.c_str(),
               addr_str);
      return true;
    }
    return false;
  }

  // VCSEC advertises as "S" + 16 hex digits (hash of the VIN) + "C".
  if (!TeslaBLE::is_tesla_vehicle_name(name.c_str()))
    return false;

  const uint64_t address = device.address_uint64();
  const uint32_t now = millis();
  for (auto &seen : this->seen_) {
    if (seen.address != address)
      continue;
    if (now - seen.last_log_ms < RELOG_INTERVAL_MS)
      return true;
    seen.last_log_ms = now;
    ESP_LOGD(TAG, "Tesla seen | Name: %s | MAC: %s | RSSI: %d dBm",
             name.c_str(), addr_str, device.get_rssi());
    return true;
  }

  this->seen_.push_back({address, now});
  ESP_LOGD(TAG, "New Tesla seen | Name: %s | MAC: %s | RSSI: %d dBm",
           name.c_str(), addr_str, device.get_rssi());
  return true;
}
}  // namespace tesla_ble_listener
}  // namespace esphome
