#pragma once

#include "esphome/core/component.h"
#include "esphome/components/esp32_ble_tracker/esp32_ble_tracker.h"

#include <cstdint>
#include <string>
#include <vector>

namespace esphome
{
  namespace tesla_ble_listener
  {

    std::string get_vin_advertisement_name(const char *vin);

    class TeslaBLEListener : public esp32_ble_tracker::ESPBTDeviceListener
    {
    public:
      std::string vin_ad_name_;  // empty: no VIN set, report every Tesla seen

      bool parse_device(const esp32_ble_tracker::ESPBTDevice &device) override;
      void set_vin(const char *vin) { vin_ad_name_ = get_vin_advertisement_name(vin); }

    protected:
      struct Seen {
        uint64_t address;
        uint32_t last_log_ms;
      };
      std::vector<Seen> seen_;
    };

  } // namespace tesla_ble_listener
} // namespace esphome
