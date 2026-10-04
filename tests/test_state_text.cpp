// Behavioural tests for state_text (components/tesla_ble_vehicle/state_text.h).
// No ESPHome or tesla-ble dependency - builds with a plain C++ compiler:
//   make test-cpp  (or just  make test)
//
// These tests describe desired behaviour of the raw state -> text/flag
// conversions that feed the sensors: what each VCSEC/CarServer state maps to.

#include "test_helper.h"

#include "state_text.h"

using namespace esphome::tesla_ble_vehicle::state_text;

#define CHECK_OPT(opt, expected) CHECK((opt).has_value() && (opt).value() == (expected))
#define CHECK_STR(actual, expected) CHECK((actual) == (expected))

static void test_sleep_status() {
  CHECK_OPT(sleep_status(kSleepAwake), false);
  CHECK_OPT(sleep_status(kSleepAsleep), true);
  CHECK(!sleep_status(kSleepUnknown).has_value());
  CHECK(!sleep_status(99).has_value());
}

static void test_lock_status() {
  CHECK_OPT(lock_status(kLockUnlocked), true);
  CHECK_OPT(lock_status(kLockSelectiveUnlocked), true);
  CHECK_OPT(lock_status(kLockLocked), false);
  CHECK_OPT(lock_status(kLockInternalLocked), false);
  CHECK(!lock_status(99).has_value());
}

static void test_user_presence() {
  CHECK_OPT(user_presence(kPresencePresent), true);
  CHECK_OPT(user_presence(kPresenceNotPresent), false);
  CHECK(!user_presence(kPresenceUnknown).has_value());
  CHECK(!user_presence(99).has_value());
}

static void test_is_charging() {
  CHECK(is_charging(kChargingStateCharging));
  CHECK(is_charging(kChargingStateStarting));
  CHECK(!is_charging(kChargingStateComplete));
  CHECK(!is_charging(kChargingStateStopped));
  CHECK(!is_charging(kChargingStateDisconnected));
  CHECK(!is_charging(kChargingStateNoPower));
  CHECK(!is_charging(kChargingStateUnknown));
  CHECK(!is_charging(99));
}

static void test_charging_state_text() {
  CHECK_STR(charging_state(kChargingStateDisconnected), "Disconnected");
  CHECK_STR(charging_state(kChargingStateNoPower), "No Power");
  CHECK_STR(charging_state(kChargingStateStarting), "Starting");
  CHECK_STR(charging_state(kChargingStateCharging), "Charging");
  CHECK_STR(charging_state(kChargingStateComplete), "Complete");
  CHECK_STR(charging_state(kChargingStateStopped), "Stopped");
  CHECK_STR(charging_state(kChargingStateCalibrating), "Calibrating");
  CHECK_STR(charging_state(kChargingStateUnknown), "Unknown");
  CHECK_STR(charging_state(99), "Unknown");
}

static void test_charger_connected() {
  CHECK(!charger_connected(kChargingStateDisconnected));
  CHECK(!charger_connected(kChargingStateUnknown));
  CHECK(charger_connected(kChargingStateNoPower));
  CHECK(charger_connected(kChargingStateStarting));
  CHECK(charger_connected(kChargingStateCharging));
  CHECK(charger_connected(kChargingStateComplete));
  CHECK(charger_connected(kChargingStateStopped));
  CHECK(charger_connected(kChargingStateCalibrating));
}

static void test_iec61851_state_text() {
  CHECK_STR(iec61851_state(kChargingStateDisconnected), "A");
  CHECK_STR(iec61851_state(kChargingStateNoPower), "E");
  CHECK_STR(iec61851_state(kChargingStateStarting), "C");
  CHECK_STR(iec61851_state(kChargingStateCharging), "C");
  CHECK_STR(iec61851_state(kChargingStateCalibrating), "C");
  CHECK_STR(iec61851_state(kChargingStateComplete), "B");
  CHECK_STR(iec61851_state(kChargingStateStopped), "B");
  CHECK_STR(iec61851_state(kChargingStateUnknown), "F");
  CHECK_STR(iec61851_state(99), "F");
}

static void test_shift_state_text() {
  CHECK_STR(shift_state(kShiftP), "P");
  CHECK_STR(shift_state(kShiftR), "R");
  CHECK_STR(shift_state(kShiftN), "N");
  CHECK_STR(shift_state(kShiftD), "D");
  CHECK_STR(shift_state(kShiftSNA), "SNA");
  CHECK_STR(shift_state(kShiftInvalid), "Invalid");
  CHECK_STR(shift_state(99), "Unknown");
}

static void test_is_parked() {
  CHECK(is_parked(kShiftP));
  CHECK(!is_parked(kShiftR));
  CHECK(!is_parked(kShiftN));
  CHECK(!is_parked(kShiftD));
  CHECK(!is_parked(kShiftInvalid));
}

static void test_charge_limit_reason_text() {
  CHECK_STR(charge_limit_reason(kLimitNone), "None");
  CHECK_STR(charge_limit_reason(kLimitEvse), "EVSE");
  CHECK_STR(charge_limit_reason(kLimitBattTempLow), "BattTempLow");
  CHECK_STR(charge_limit_reason(kLimitHighSoc), "HighSoc");
  CHECK_STR(charge_limit_reason(kLimitCabin), "Cabin");
  CHECK_STR(charge_limit_reason(kLimitUnknown), "Unknown");
  CHECK_STR(charge_limit_reason(99), "Unknown");
}

static void test_scheduled_charging_mode_text() {
  CHECK_STR(scheduled_charging_mode(kScheduledChargingOff), "Off");
  CHECK_STR(scheduled_charging_mode(kScheduledChargingStartAt), "Start At");
  CHECK_STR(scheduled_charging_mode(kScheduledChargingDepartBy), "Depart By");
  CHECK_STR(scheduled_charging_mode(7), "Unknown");
}

static void test_seat_heater_level_text() {
  CHECK_STR(seat_heater_level(kSeatHeaterOff), "Off");
  CHECK_STR(seat_heater_level(kSeatHeaterLow), "Low");
  CHECK_STR(seat_heater_level(kSeatHeaterMed), "Medium");
  CHECK_STR(seat_heater_level(kSeatHeaterHigh), "High");
  CHECK_STR(seat_heater_level(-1), "Unknown");
}

static void test_time_of_day() {
  CHECK(time_of_day(0).value() == "00:00");
  CHECK(time_of_day(7 * 60 + 5).value() == "07:05");
  CHECK(time_of_day(23 * 60 + 59).value() == "23:59");
  CHECK(!time_of_day(24 * 60).has_value());
}

static void test_miles_to_km() {
  CHECK(miles_to_km(0.0f) == 0.0f);
  CHECK(miles_to_km(100.0f) > 160.93f && miles_to_km(100.0f) < 160.94f);
}

static void test_climate_preset() {
  CHECK_STR(std::string(climate_preset(kKeeperOff, kDefrostOff)), "Normal");
  CHECK_STR(std::string(climate_preset(kKeeperOn, kDefrostOff)), "Keep On");
  CHECK_STR(std::string(climate_preset(kKeeperDog, 0)), "Dog Mode");
  CHECK_STR(std::string(climate_preset(kKeeperParty, kDefrostNormal)), "Camp Mode");
  // Max defrost wins over the climate keeper mode
  CHECK_STR(std::string(climate_preset(kKeeperOff, kDefrostMax)), "Defrost");
  CHECK_STR(std::string(climate_preset(0, kDefrostMax)), "Defrost");
  // Nothing usable reported: keep the current preset
  CHECK(climate_preset(0, 0) == nullptr);
  // Cars report keeper Unknown when no keeper mode is active
  CHECK_STR(std::string(climate_preset(kKeeperUnknown, kDefrostOff)), "Normal");
  CHECK(climate_preset(kKeeperUnknown, 0) == nullptr);
}

static void test_climate_fan_mode() {
  CHECK_STR(std::string(climate_fan_mode(true)), "Bioweapon Mode");
  CHECK_STR(std::string(climate_fan_mode(false)), "Normal");
}

static void test_closure_open() {
  CHECK(closure_open(kClosureClosed) == std::optional<bool>(false));
  CHECK(closure_open(kClosureFailedUnlatch) == std::optional<bool>(false));
  CHECK(closure_open(kClosureOpen) == std::optional<bool>(true));
  CHECK(closure_open(kClosureAjar) == std::optional<bool>(true));
  CHECK(closure_open(kClosureOpening) == std::optional<bool>(true));
  CHECK(closure_open(kClosureClosing) == std::optional<bool>(true));
  CHECK(!closure_open(kClosureUnknown).has_value());
  CHECK(!closure_open(42).has_value());
}

static void test_steering_wheel_heat_level() {
  CHECK(steering_wheel_heat_level(kStwHeatOff).value() == "Off");
  CHECK(steering_wheel_heat_level(kStwHeatLow).value() == "Low");
  CHECK(steering_wheel_heat_level(kStwHeatHigh).value() == "High");
  CHECK(!steering_wheel_heat_level(kStwHeatUnknown).has_value());
  CHECK(!steering_wheel_heat_level(9).has_value());
}

static void test_charge_port_latch_lock() {
  using L = LatchLock;
  CHECK(charge_port_latch_lock(kLatchEngaged, std::nullopt) == L::LOCKED);
  CHECK(charge_port_latch_lock(kLatchDisengaged, false) == L::UNLOCKED);
  CHECK(charge_port_latch_lock(kLatchBlocking, true) == L::JAMMED);
  // No cable: follows the charge port door
  CHECK(charge_port_latch_lock(kLatchSNA, false) == L::LOCKED);
  CHECK(charge_port_latch_lock(kLatchSNA, true) == L::UNLOCKED);
  CHECK(charge_port_latch_lock(0, true) == L::UNLOCKED);
  CHECK(!charge_port_latch_lock(kLatchSNA, std::nullopt).has_value());
  // No cable: the latch pin briefly reads engaged while the flap opens - follow the flap
  CHECK(charge_port_latch_lock(kLatchEngaged, true, false) == L::UNLOCKED);
  CHECK(charge_port_latch_lock(kLatchEngaged, false, false) == L::LOCKED);
  // Cable connected: the latch decides
  CHECK(charge_port_latch_lock(kLatchEngaged, true, true) == L::LOCKED);
  CHECK(charge_port_latch_lock(kLatchDisengaged, true, true) == L::UNLOCKED);
}

static void test_cop_temp() {
  CHECK_STR(std::string(cop_temp_option(kCopTempLow)), "30 \u00b0C");
  CHECK_STR(std::string(cop_temp_option(kCopTempHigh)), "40 \u00b0C");
  CHECK(cop_temp_option(0) == nullptr);
  CHECK(cop_temp_level("35 \u00b0C") == std::optional<int>(kCopTempMedium));
  CHECK(!cop_temp_level("50 \u00b0C").has_value());
  for (int level = kCopTempLow; level <= kCopTempHigh; level++)
    CHECK(cop_temp_level(cop_temp_option(level)) == std::optional<int>(level));
}

static void test_cop_option() {
  CHECK(cop_option(kCopOff, 0).value() == "Off");
  CHECK(cop_option(kCopFanOnly, kCopTempHigh).value() == "Fan Only");
  CHECK(cop_option(kCopOn, kCopTempMedium).value() == "On 35 \u00b0C");
  CHECK(!cop_option(kCopOn, 0).has_value());  // temperature not known yet
  CHECK(!cop_option(-1, 0).has_value());
  auto c = cop_choice("On 30 \u00b0C");
  CHECK(c.has_value() && c->mode == kCopOn && c->level == kCopTempLow);
  c = cop_choice("Fan Only");
  CHECK(c.has_value() && c->mode == kCopFanOnly);
  CHECK(cop_choice("Off").has_value());
  CHECK(!cop_choice("On").has_value());
  CHECK(!cop_choice("On 50 \u00b0C").has_value());
  // Every option round-trips
  for (int level = kCopTempLow; level <= kCopTempHigh; level++) {
    auto back = cop_choice(cop_option(kCopOn, level).value());
    CHECK(back.has_value() && back->mode == kCopOn && back->level == level);
  }
}

static void test_departure_policy() {
  CHECK_STR(std::string(departure_policy_option(kPolicyOff)), "Off");
  CHECK_STR(std::string(departure_policy_option(kPolicyWeekdays)), "Weekdays");
  CHECK(departure_policy_option(3) == nullptr);
  for (int p = kPolicyOff; p <= kPolicyWeekdays; p++)
    CHECK(departure_policy(departure_policy_option(p)) == std::optional<int>(p));
  CHECK(!departure_policy("Weekends").has_value());
}

int main() {
  test_sleep_status();
  test_lock_status();
  test_user_presence();
  test_is_charging();
  test_charging_state_text();
  test_charger_connected();
  test_iec61851_state_text();
  test_shift_state_text();
  test_is_parked();
  test_charge_limit_reason_text();
  test_scheduled_charging_mode_text();
  test_seat_heater_level_text();
  test_time_of_day();
  test_miles_to_km();
  test_climate_preset();
  test_climate_fan_mode();
  test_closure_open();
  test_steering_wheel_heat_level();
  test_charge_port_latch_lock();
  test_cop_temp();
  test_cop_option();
  test_departure_policy();

  return test_summary();
}
