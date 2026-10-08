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
// VCSEC_ClosureState_E
constexpr int kClosureClosed = 0;
constexpr int kClosureOpen = 1;
constexpr int kClosureAjar = 2;
constexpr int kClosureUnknown = 3;
constexpr int kClosureFailedUnlatch = 4;
constexpr int kClosureOpening = 5;
constexpr int kClosureClosing = 6;

// CarServer_ChargeState_ScheduledChargingMode
constexpr int kScheduledChargingOff = 0;
constexpr int kScheduledChargingStartAt = 1;
constexpr int kScheduledChargingDepartBy = 2;

// CarServer_ClimateState_SeatHeaterLevel_E
constexpr int kSeatHeaterOff = 0;
constexpr int kSeatHeaterLow = 1;
constexpr int kSeatHeaterMed = 2;
constexpr int kSeatHeaterHigh = 3;

// CarServer_ClimateState_ClimateKeeperMode tags (which_type)
constexpr int kKeeperUnknown = 1;
constexpr int kKeeperOff = 2;
constexpr int kKeeperOn = 3;
constexpr int kKeeperDog = 4;
constexpr int kKeeperParty = 5;  // shown as Camp Mode

// CarServer_ClimateState_DefrostMode tags (which_type)
constexpr int kDefrostOff = 1;
constexpr int kDefrostNormal = 2;
constexpr int kDefrostMax = 3;

// CarServer_StwHeatLevel
constexpr int kStwHeatUnknown = 0;
constexpr int kStwHeatOff = 1;
constexpr int kStwHeatLow = 2;
constexpr int kStwHeatHigh = 3;

// CarServer_ChargePortLatchState tags (which_type)
constexpr int kLatchSNA = 1;
constexpr int kLatchDisengaged = 2;
constexpr int kLatchEngaged = 3;
constexpr int kLatchBlocking = 4;

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

// Powershare (ChargeState fields 169-171): plain proto enum values, 0 = none/inactive.
inline std::string powershare_status(int status) {
  switch (status) {
    case 0:
      return "Inactive";
    case 1:
      return "Initializing";
    case 2:
      return "Active";
    case 3:
      return "Stopped";
    case 4:
      return "Handshaking";
    case 5:
      return "Reconnecting";
    default:
      return "Unknown";
  }
}

// Sharing power now, including "active, reconnecting soon".
inline bool powershare_active(int status) { return status == 2 || status == 5; }

inline std::string powershare_type(int type) {
  switch (type) {
    case 0:
      return "None";
    case 1:
      return "Load";
    case 2:
      return "Home";
    default:
      return "Unknown";
  }
}

inline std::string powershare_stop_reason(int reason) {
  switch (reason) {
    case 0:
      return "None";
    case 1:
      return "SOC Too Low";
    case 2:
      return "Retry";
    case 3:
      return "Fault";
    case 4:
      return "User";
    case 5:
      return "Reconnecting";
    case 6:
      return "Authentication";
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

// VCSEC closure state -> open? Ajar and moving count as open; a failed
// unlatch left it closed. Nothing when the car does not know.
inline std::optional<bool> closure_open(int state) {
  switch (state) {
    case kClosureClosed:
    case kClosureFailedUnlatch:
      return false;
    case kClosureOpen:
    case kClosureAjar:
    case kClosureOpening:
    case kClosureClosing:
      return true;
    default:
      return std::nullopt;
  }
}

// Charge Port Latch lock entity state from the latch and the charge port door.
enum class LatchLock { UNLOCKED, LOCKED, JAMMED };
// Engaged = cable held, Disengaged = cable free, Blocking = jammed. With no
// cable (not connected, or latch SNA) it follows the charge port door, which
// is what the lock / unlock commands move then: closed = locked, open =
// unlocked - the car briefly reports the pin as engaged while the flap moves,
// so the latch is ignored without a cable. Nothing when neither tells.
inline std::optional<LatchLock> charge_port_latch_lock(int latch_tag, std::optional<bool> door_open,
                                                       std::optional<bool> cable_connected = std::nullopt) {
  if (cable_connected.has_value() && !*cable_connected && door_open.has_value())
    return *door_open ? LatchLock::UNLOCKED : LatchLock::LOCKED;
  switch (latch_tag) {
    case kLatchEngaged:
      return LatchLock::LOCKED;
    case kLatchDisengaged:
      return LatchLock::UNLOCKED;
    case kLatchBlocking:
      return LatchLock::JAMMED;
    default:
      if (door_open.has_value())
        return *door_open ? LatchLock::UNLOCKED : LatchLock::LOCKED;
      return std::nullopt;
  }
}

// Cabin overheat protection activation temperature (ClimateState.CopActivationTemp)
constexpr int kCopTempLow = 1;
constexpr int kCopTempMedium = 2;
constexpr int kCopTempHigh = 3;

inline const char *cop_temp_option(int level) {
  switch (level) {
    case kCopTempLow:
      return "30 \u00b0C";
    case kCopTempMedium:
      return "35 \u00b0C";
    case kCopTempHigh:
      return "40 \u00b0C";
    default:
      return nullptr;
  }
}

inline std::optional<int> cop_temp_level(const std::string &option) {
  for (int level = kCopTempLow; level <= kCopTempHigh; level++)
    if (option == cop_temp_option(level)) return level;
  return std::nullopt;
}

// Cabin overheat protection mode (ClimateState.CabinOverheatProtection_E)
constexpr int kCopOff = 0;
constexpr int kCopOn = 1;
constexpr int kCopFanOnly = 2;

// One select for mode + activation temperature, as the Tesla app shows it:
// the temperature only applies to On (A/C), so it is part of the On options.
struct CopChoice {
  int mode;
  int level;  // kCopTempLow..High for kCopOn, 0 otherwise
};

inline std::optional<std::string> cop_option(int mode, int level) {
  switch (mode) {
    case kCopOff:
      return std::string("Off");
    case kCopFanOnly:
      return std::string("Fan Only");
    case kCopOn: {
      const char *temp = cop_temp_option(level);
      if (temp == nullptr) return std::nullopt;  // temperature not known yet
      return std::string("On ") + temp;
    }
    default:
      return std::nullopt;
  }
}

inline std::optional<CopChoice> cop_choice(const std::string &option) {
  if (option == "Off") return CopChoice{kCopOff, 0};
  if (option == "Fan Only") return CopChoice{kCopFanOnly, 0};
  if (option.rfind("On ", 0) == 0) {
    auto level = cop_temp_level(option.substr(3));
    if (level.has_value()) return CopChoice{kCopOn, *level};
  }
  return std::nullopt;
}

// Scheduled departure policy (preconditioning / off-peak charging):
// PreconditioningTimes / OffPeakChargingTimes oneof tags, 0 = off
constexpr int kPolicyOff = 0;
constexpr int kPolicyAllWeek = 1;
constexpr int kPolicyWeekdays = 2;

inline const char *departure_policy_option(int policy) {
  switch (policy) {
    case kPolicyOff:
      return "Off";
    case kPolicyAllWeek:
      return "All Week";
    case kPolicyWeekdays:
      return "Weekdays";
    default:
      return nullptr;
  }
}

inline std::optional<int> departure_policy(const std::string &option) {
  for (int p = kPolicyOff; p <= kPolicyWeekdays; p++)
    if (option == departure_policy_option(p)) return p;
  return std::nullopt;
}

// Climate entity preset from the car's climate keeper and defrost state, using
// the preset names TeslaClimate offers. 0 = field not reported. nullptr when
// the state does not tell (the entity keeps its current preset).
// Cars report the keeper as Unknown when no keeper mode is active (seen on
// two cars with climate off and on), so Unknown counts as Normal once the
// defrost state confirms the climate data is there.
inline const char *climate_preset(int keeper_tag, int defrost_tag) {
  if (defrost_tag == kDefrostMax)
    return "Defrost";
  switch (keeper_tag) {
    case kKeeperUnknown:
      return defrost_tag != 0 ? "Normal" : nullptr;
    case kKeeperOff:
      return "Normal";
    case kKeeperOn:
      return "Keep On";
    case kKeeperDog:
      return "Dog Mode";
    case kKeeperParty:
      return "Camp Mode";
    default:
      return nullptr;
  }
}

// Climate entity fan mode from the car's bioweapon defense state.
inline const char *climate_fan_mode(bool bioweapon_on) { return bioweapon_on ? "Bioweapon Mode" : "Normal"; }

// Steering wheel heat level; nothing when the car does not know.
inline std::optional<std::string> steering_wheel_heat_level(int level) {
  switch (level) {
    case kStwHeatOff:
      return std::string("Off");
    case kStwHeatLow:
      return std::string("Low");
    case kStwHeatHigh:
      return std::string("High");
    default:
      return std::nullopt;
  }
}

// CarServer_MediaPlaybackStatus
constexpr int kMediaStopped = 0;
constexpr int kMediaPlaying = 1;
constexpr int kMediaPaused = 2;

// What the media player entity shows. OFF while the car is asleep: the car
// has no media state to read then (ESPHome has no per-entity "unavailable").
enum class MediaPlay { OFF, IDLE, PLAYING, PAUSED };

// Broadcast radio (AM, FM, XM, DAB, US/EU radio, SiriusXM) can play without a
// title or artist (no RDS text), so it counts as playing even then.
inline bool media_source_is_radio(int source) {
  switch (source) {
    case 1: case 2: case 3: case 10: case 13: case 14: case 19:
      return true;
    default:
      return false;
  }
}

// has_now_playing: the car reported a title or an artist (or plays radio). A parked, empty car
// keeps reporting its last streaming source as "playing" with neither (seen on
// two cars overnight), while anything that really plays has at least a title
// (music: title and artist, the theater apps: the video title). Playing or
// paused with nothing loaded is shown as idle.
inline MediaPlay media_play_state(bool asleep, std::optional<int> playback_status, bool has_now_playing) {
  if (asleep)
    return MediaPlay::OFF;
  if (!playback_status.has_value() || !has_now_playing)
    return MediaPlay::IDLE;
  switch (*playback_status) {
    case kMediaPlaying:
      return MediaPlay::PLAYING;
    case kMediaPaused:
      return MediaPlay::PAUSED;
    default:
      return MediaPlay::IDLE;
  }
}

// The car takes volumes 0-10 (vehicle-command SetVolume); it reports its own
// maximum, which can be a little higher. The entity volume is 0-1 of the
// usable range.
constexpr float kMediaVolumeLimit = 10.0f;

inline float media_volume_max(std::optional<float> reported_max) {
  if (!reported_max.has_value() || !(*reported_max > 0.0f) || *reported_max > kMediaVolumeLimit)
    return kMediaVolumeLimit;
  return *reported_max;
}

inline float media_volume_fraction(float volume, float max) {
  if (!(max > 0.0f) || !(volume > 0.0f))
    return 0.0f;
  return volume >= max ? 1.0f : volume / max;
}

inline float media_volume_absolute(float fraction, float max) {
  if (!(fraction > 0.0f))
    return 0.0f;
  return fraction >= 1.0f ? max : fraction * max;
}

// CarServer_MediaSourceType -> text; nothing for None. A value the protocol
// file does not name (cars send 27, between NetEase Music and Browser) shows
// its number, so it is not mistaken for "no source".
inline std::optional<std::string> media_source(int source) {
  switch (source) {
    case 1: return std::string("AM");
    case 2: return std::string("FM");
    case 3: return std::string("XM");
    case 5: return std::string("Slacker");
    case 6: return std::string("Local Files");
    case 7: return std::string("iPod");
    case 8: return std::string("Bluetooth");
    case 9: return std::string("Aux In");
    case 10: return std::string("DAB");
    case 11: return std::string("Rdio");
    case 12: return std::string("Spotify");
    case 13: return std::string("Radio");
    case 14: return std::string("Radio");
    case 16: return std::string("Media File");
    case 17: return std::string("TuneIn");
    case 18: return std::string("Stingray");
    case 19: return std::string("SiriusXM");
    case 20: return std::string("Tidal");
    case 21: return std::string("QQ Music");
    case 22: return std::string("QQ Music");
    case 23: return std::string("Ximalaya");
    case 24: return std::string("Online Radio");
    case 25: return std::string("Online Radio");
    case 26: return std::string("NetEase Music");
    case 28: return std::string("Browser");
    case 29: return std::string("Theater");
    case 30: return std::string("Game");
    case 31: return std::string("Tutorial");
    case 32: return std::string("Toybox");
    case 33: return std::string("Recents & Favorites");
    case 34: return std::string("Home Apps");
    case 35: return std::string("Search");
    default:
      break;
  }
  if (source <= 0)
    return std::nullopt;
  return "Source " + std::to_string(source);
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
