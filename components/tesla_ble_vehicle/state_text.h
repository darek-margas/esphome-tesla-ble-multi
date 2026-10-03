#pragma once

#include <cstdint>
#include <cstdio>
#include <optional>
#include <string>

namespace esphome {
namespace tesla_ble_vehicle {
namespace state_text {

// Raw protobuf enum/tag values, duplicated from the nanopb-generated headers of
// the external tesla-ble library (vcsec.pb.h, vehicle.pb.h). Kept here so this
// module has no external dependency and can be unit-tested in isolation.
// vehicle_state_manager.cpp static_asserts these match the library values.

// VCSEC_VehicleSleepStatus_E
constexpr int kSleepUnknown = 0;
constexpr int kSleepAwake = 1;
constexpr int kSleepAsleep = 2;

// VCSEC_VehicleLockState_E
constexpr int kLockUnlocked = 0;
constexpr int kLockLocked = 1;
constexpr int kLockInternalLocked = 2;
constexpr int kLockSelectiveUnlocked = 3;

// VCSEC_UserPresence_E
constexpr int kPresenceUnknown = 0;
constexpr int kPresenceNotPresent = 1;
constexpr int kPresencePresent = 2;

// CarServer_ChargeState_ChargingState tags (which_type)
constexpr int kChargingStateUnknown = 1;
constexpr int kChargingStateDisconnected = 2;
constexpr int kChargingStateNoPower = 3;
constexpr int kChargingStateStarting = 4;
constexpr int kChargingStateCharging = 5;
constexpr int kChargingStateComplete = 6;
constexpr int kChargingStateStopped = 7;
constexpr int kChargingStateCalibrating = 8;

// CarServer_ShiftState tags (which_type)
constexpr int kShiftInvalid = 1;
constexpr int kShiftP = 2;
constexpr int kShiftR = 3;
constexpr int kShiftN = 4;
constexpr int kShiftD = 5;
constexpr int kShiftSNA = 6;

// CarServer_ChargeState_ChargeLimitReason
constexpr int kLimitUnknown = 0;
constexpr int kLimitNone = 1;
constexpr int kLimitEvse = 2;
constexpr int kLimitBattTempLow = 3;
constexpr int kLimitHighSoc = 4;
constexpr int kLimitCabin = 5;

// Map a VCSEC enum to true/false, or nullopt when the status is unknown.
// CarServer_ChargeState_ScheduledChargingMode
constexpr int kScheduledChargingOff = 0;
constexpr int kScheduledChargingStartAt = 1;
constexpr int kScheduledChargingDepartBy = 2;

// CarServer_ClimateState_SeatHeaterLevel_E
constexpr int kSeatHeaterOff = 0;
constexpr int kSeatHeaterLow = 1;
constexpr int kSeatHeaterMed = 2;
constexpr int kSeatHeaterHigh = 3;

constexpr float kKmPerMile = 1.609344f;

inline float miles_to_km(float miles) { return miles * kKmPerMile; }

inline std::optional<bool> sleep_status(int status) {
  switch (status) {
    case kSleepAwake:
      return false;
    case kSleepAsleep:
      return true;
    default:
      return std::nullopt;
  }
}

inline std::optional<bool> lock_status(int status) {
  switch (status) {
    case kLockUnlocked:
    case kLockSelectiveUnlocked:
      return true;
    case kLockLocked:
    case kLockInternalLocked:
      return false;
    default:
      return std::nullopt;
  }
}

inline std::optional<bool> user_presence(int presence) {
  switch (presence) {
    case kPresencePresent:
      return true;
    case kPresenceNotPresent:
      return false;
    default:
      return std::nullopt;
  }
}

// Whether a charging-state tag means the car is actively charging.
inline bool is_charging(int which_type) {
  return which_type == kChargingStateCharging || which_type == kChargingStateStarting;
}

inline std::string charging_state(int which_type) {
  switch (which_type) {
    case kChargingStateDisconnected:
      return "Disconnected";
    case kChargingStateNoPower:
      return "No Power";
    case kChargingStateStarting:
      return "Starting";
    case kChargingStateCharging:
      return "Charging";
    case kChargingStateComplete:
      return "Complete";
    case kChargingStateStopped:
      return "Stopped";
    case kChargingStateCalibrating:
      return "Calibrating";
    default:
      return "Unknown";
  }
}

inline bool charger_connected(int which_type) {
  switch (which_type) {
    case kChargingStateDisconnected:
    case kChargingStateUnknown:
      return false;
    default:
      return true;
  }
}

inline std::string iec61851_state(int which_type) {
  switch (which_type) {
    case kChargingStateDisconnected:
      return "A";
    case kChargingStateNoPower:
      return "E";
    case kChargingStateStarting:
    case kChargingStateCharging:
    case kChargingStateCalibrating:
      return "C";
    case kChargingStateComplete:
    case kChargingStateStopped:
      return "B";
    default:
      return "F";
  }
}

inline std::string shift_state(int which_type) {
  switch (which_type) {
    case kShiftP:
      return "P";
    case kShiftR:
      return "R";
    case kShiftN:
      return "N";
    case kShiftD:
      return "D";
    case kShiftSNA:
      return "SNA";
    case kShiftInvalid:
      return "Invalid";
    default:
      return "Unknown";
  }
}

inline bool is_parked(int which_type) { return which_type == kShiftP; }

inline std::string charge_limit_reason(int reason) {
  switch (reason) {
    case kLimitNone:
      return "None";
    case kLimitEvse:
      return "EVSE";
    case kLimitBattTempLow:
      return "BattTempLow";
    case kLimitHighSoc:
      return "HighSoc";
    case kLimitCabin:
      return "Cabin";
    default:
      return "Unknown";
  }
}

inline std::string scheduled_charging_mode(int mode) {
  switch (mode) {
    case kScheduledChargingOff:
      return "Off";
    case kScheduledChargingStartAt:
      return "Start At";
    case kScheduledChargingDepartBy:
      return "Depart By";
    default:
      return "Unknown";
  }
}

inline std::string seat_heater_level(int level) {
  switch (level) {
    case kSeatHeaterOff:
      return "Off";
    case kSeatHeaterLow:
      return "Low";
    case kSeatHeaterMed:
      return "Medium";
    case kSeatHeaterHigh:
      return "High";
    default:
      return "Unknown";
  }
}

// Minutes after midnight -> "HH:MM"; nothing for values past the end of a day.
inline std::optional<std::string> time_of_day(uint32_t minutes) {
  if (minutes >= 24 * 60)
    return std::nullopt;
  char buf[6];
  snprintf(buf, sizeof(buf), "%02u:%02u", static_cast<unsigned>(minutes / 60),
           static_cast<unsigned>(minutes % 60));
  return std::string(buf);
}

}  // namespace state_text
}  // namespace tesla_ble_vehicle
}  // namespace esphome
