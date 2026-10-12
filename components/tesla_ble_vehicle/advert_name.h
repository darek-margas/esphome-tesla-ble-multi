#pragma once

#include <cctype>
#include <cstddef>
#include <string>

namespace esphome {
namespace tesla_ble_vehicle {

// Which BLE advert name belongs to a car, so it can be found from its VIN alone
// (the advert name is in the scan response, so it needs an active scan).
//
// Two naming schemes are in use:
//  - "S" + 16 hex characters of SHA1(VIN) + a beacon letter. The car has
//    several beacons (C center, R rear, D driver, P passenger), each with its
//    own MAC; Tesla's vehicle-command connects to the "C" one, so only that
//    one is taken. tesla-ble computes this name (get_vin_advertisement_name).
//  - Cars from 2023 on: "Tesla " + the last 6 characters of the VIN.
//
// Letters are compared without case. Pure logic with no ESPHome or tesla-ble
// dependency so it can be unit-tested in isolation (tests/test_advert_name.cpp).

// The 2023+ name for a VIN, or "" for a VIN that is not 17 characters.
inline std::string tesla_advert_name_2023(const std::string &vin) {
  if (vin.size() != 17)
    return "";
  std::string name = "Tesla " + vin.substr(11);
  for (size_t i = 6; i < name.size(); ++i)
    name[i] = static_cast<char>(std::toupper(static_cast<unsigned char>(name[i])));
  return name;
}

inline bool same_text_ignoring_case(const std::string &a, const std::string &b) {
  if (a.size() != b.size())
    return false;
  for (size_t i = 0; i < a.size(); ++i) {
    if (std::toupper(static_cast<unsigned char>(a[i])) != std::toupper(static_cast<unsigned char>(b[i])))
      return false;
  }
  return true;
}

// name: the advert name seen; hash_name: "S<16 hex>C" for this car (from
// tesla-ble, "" if the VIN is invalid); vin: this car's VIN.
inline bool advert_name_matches_vin(const std::string &name, const std::string &hash_name, const std::string &vin) {
  if (name.empty())
    return false;
  if (!hash_name.empty() && same_text_ignoring_case(name, hash_name))
    return true;
  const std::string name_2023 = tesla_advert_name_2023(vin);
  return !name_2023.empty() && same_text_ignoring_case(name, name_2023);
}

}  // namespace tesla_ble_vehicle
}  // namespace esphome
