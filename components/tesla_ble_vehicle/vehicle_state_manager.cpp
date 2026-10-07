#include "vehicle_state_manager.h"
#include "state_text.h"
#include "tesla_ble_vehicle.h"
#ifdef USE_API
#include <esphome/components/api/api_server.h>
#endif
#include <esphome/core/helpers.h>
#include <cmath>
#include <algorithm>
#include <cinttypes>

namespace esphome {
namespace tesla_ble_vehicle {

// The raw values duplicated in state_text.h must match the nanopb-generated
// enums/tags from the tesla-ble library, or every state conversion would be
// wrong. Compile-time check.
static_assert(static_cast<int>(VCSEC_VehicleSleepStatus_E_VEHICLE_SLEEP_STATUS_AWAKE) == state_text::kSleepAwake);
static_assert(static_cast<int>(VCSEC_VehicleSleepStatus_E_VEHICLE_SLEEP_STATUS_ASLEEP) == state_text::kSleepAsleep);
static_assert(static_cast<int>(VCSEC_VehicleLockState_E_VEHICLELOCKSTATE_UNLOCKED) == state_text::kLockUnlocked);
static_assert(static_cast<int>(VCSEC_VehicleLockState_E_VEHICLELOCKSTATE_LOCKED) == state_text::kLockLocked);
static_assert(static_cast<int>(VCSEC_VehicleLockState_E_VEHICLELOCKSTATE_INTERNAL_LOCKED) == state_text::kLockInternalLocked);
static_assert(static_cast<int>(VCSEC_VehicleLockState_E_VEHICLELOCKSTATE_SELECTIVE_UNLOCKED) == state_text::kLockSelectiveUnlocked);
static_assert(static_cast<int>(VCSEC_UserPresence_E_VEHICLE_USER_PRESENCE_NOT_PRESENT) == state_text::kPresenceNotPresent);
static_assert(static_cast<int>(VCSEC_UserPresence_E_VEHICLE_USER_PRESENCE_PRESENT) == state_text::kPresencePresent);
static_assert(CarServer_ChargeState_ChargingState_Unknown_tag == state_text::kChargingStateUnknown);
static_assert(CarServer_ChargeState_ChargingState_Disconnected_tag == state_text::kChargingStateDisconnected);
static_assert(CarServer_ChargeState_ChargingState_NoPower_tag == state_text::kChargingStateNoPower);
static_assert(CarServer_ChargeState_ChargingState_Starting_tag == state_text::kChargingStateStarting);
static_assert(CarServer_ChargeState_ChargingState_Charging_tag == state_text::kChargingStateCharging);
static_assert(CarServer_ChargeState_ChargingState_Complete_tag == state_text::kChargingStateComplete);
static_assert(CarServer_ChargeState_ChargingState_Stopped_tag == state_text::kChargingStateStopped);
static_assert(CarServer_ChargeState_ChargingState_Calibrating_tag == state_text::kChargingStateCalibrating);
static_assert(CarServer_ShiftState_Invalid_tag == state_text::kShiftInvalid);
static_assert(CarServer_ShiftState_P_tag == state_text::kShiftP);
static_assert(CarServer_ShiftState_R_tag == state_text::kShiftR);
static_assert(CarServer_ShiftState_N_tag == state_text::kShiftN);
static_assert(CarServer_ShiftState_D_tag == state_text::kShiftD);
static_assert(CarServer_ShiftState_SNA_tag == state_text::kShiftSNA);
static_assert(static_cast<int>(CarServer_ChargeState_ChargeLimitReason_ChargeLimitReasonUnknown) == state_text::kLimitUnknown);
static_assert(static_cast<int>(CarServer_ChargeState_ScheduledChargingMode_ScheduledChargingModeOff) == state_text::kScheduledChargingOff);
static_assert(static_cast<int>(CarServer_ChargeState_ScheduledChargingMode_ScheduledChargingModeStartAt) == state_text::kScheduledChargingStartAt);
static_assert(static_cast<int>(CarServer_ChargeState_ScheduledChargingMode_ScheduledChargingModeDepartBy) == state_text::kScheduledChargingDepartBy);
static_assert(static_cast<int>(CarServer_ClimateState_SeatHeaterLevel_E_SeatHeaterLevelOff) == state_text::kSeatHeaterOff);
static_assert(static_cast<int>(CarServer_ClimateState_SeatHeaterLevel_E_SeatHeaterLevelLow) == state_text::kSeatHeaterLow);
static_assert(static_cast<int>(CarServer_ClimateState_SeatHeaterLevel_E_SeatHeaterLevelMed) == state_text::kSeatHeaterMed);
static_assert(static_cast<int>(CarServer_ClimateState_SeatHeaterLevel_E_SeatHeaterLevelHigh) == state_text::kSeatHeaterHigh);
static_assert(CarServer_ClimateState_ClimateKeeperMode_Unknown_tag == state_text::kKeeperUnknown);
static_assert(CarServer_ClimateState_ClimateKeeperMode_Off_tag == state_text::kKeeperOff);
static_assert(CarServer_ClimateState_ClimateKeeperMode_On_tag == state_text::kKeeperOn);
static_assert(CarServer_ClimateState_ClimateKeeperMode_Dog_tag == state_text::kKeeperDog);
static_assert(CarServer_ClimateState_ClimateKeeperMode_Party_tag == state_text::kKeeperParty);
static_assert(CarServer_ClimateState_DefrostMode_Off_tag == state_text::kDefrostOff);
static_assert(CarServer_ClimateState_DefrostMode_Normal_tag == state_text::kDefrostNormal);
static_assert(CarServer_ClimateState_DefrostMode_Max_tag == state_text::kDefrostMax);
static_assert(CarServer_PreconditioningTimes_all_week_tag == state_text::kPolicyAllWeek);
static_assert(CarServer_PreconditioningTimes_weekdays_tag == state_text::kPolicyWeekdays);
static_assert(CarServer_OffPeakChargingTimes_all_week_tag == state_text::kPolicyAllWeek);
static_assert(CarServer_OffPeakChargingTimes_weekdays_tag == state_text::kPolicyWeekdays);
static_assert(static_cast<int>(CarServer_ClimateState_CabinOverheatProtection_E_CabinOverheatProtectionOff) == state_text::kCopOff);
static_assert(static_cast<int>(CarServer_ClimateState_CabinOverheatProtection_E_CabinOverheatProtectionOn) == state_text::kCopOn);
static_assert(static_cast<int>(CarServer_ClimateState_CabinOverheatProtection_E_CabinOverheatProtectionFanOnly) == state_text::kCopFanOnly);
static_assert(static_cast<int>(CarServer_ClimateState_CopActivationTemp_CopActivationTempLow) == state_text::kCopTempLow);
static_assert(static_cast<int>(CarServer_ClimateState_CopActivationTemp_CopActivationTempMedium) == state_text::kCopTempMedium);
static_assert(static_cast<int>(CarServer_ClimateState_CopActivationTemp_CopActivationTempHigh) == state_text::kCopTempHigh);
static_assert(CarServer_ChargePortLatchState_SNA_tag == state_text::kLatchSNA);
static_assert(CarServer_ChargePortLatchState_Disengaged_tag == state_text::kLatchDisengaged);
static_assert(CarServer_ChargePortLatchState_Engaged_tag == state_text::kLatchEngaged);
static_assert(CarServer_ChargePortLatchState_Blocking_tag == state_text::kLatchBlocking);
static_assert(static_cast<int>(CarServer_StwHeatLevel_StwHeatLevel_Unknown) == state_text::kStwHeatUnknown);
static_assert(static_cast<int>(CarServer_StwHeatLevel_StwHeatLevel_Off) == state_text::kStwHeatOff);
static_assert(static_cast<int>(CarServer_StwHeatLevel_StwHeatLevel_Low) == state_text::kStwHeatLow);
static_assert(static_cast<int>(CarServer_StwHeatLevel_StwHeatLevel_High) == state_text::kStwHeatHigh);
static_assert(static_cast<int>(VCSEC_ClosureState_E_CLOSURESTATE_CLOSED) == state_text::kClosureClosed);
static_assert(static_cast<int>(VCSEC_ClosureState_E_CLOSURESTATE_OPEN) == state_text::kClosureOpen);
static_assert(static_cast<int>(VCSEC_ClosureState_E_CLOSURESTATE_AJAR) == state_text::kClosureAjar);
static_assert(static_cast<int>(VCSEC_ClosureState_E_CLOSURESTATE_UNKNOWN) == state_text::kClosureUnknown);
static_assert(static_cast<int>(VCSEC_ClosureState_E_CLOSURESTATE_FAILED_UNLATCH) == state_text::kClosureFailedUnlatch);
static_assert(static_cast<int>(VCSEC_ClosureState_E_CLOSURESTATE_OPENING) == state_text::kClosureOpening);
static_assert(static_cast<int>(VCSEC_ClosureState_E_CLOSURESTATE_CLOSING) == state_text::kClosureClosing);
static_assert(static_cast<int>(CarServer_ChargeState_ChargeLimitReason_ChargeLimitReasonNone) == state_text::kLimitNone);
static_assert(static_cast<int>(CarServer_ChargeState_ChargeLimitReason_ChargeLimitReasonEvse) == state_text::kLimitEvse);
static_assert(static_cast<int>(CarServer_ChargeState_ChargeLimitReason_ChargeLimitReasonBattTempLow) == state_text::kLimitBattTempLow);
static_assert(static_cast<int>(CarServer_ChargeState_ChargeLimitReason_ChargeLimitReasonHighSoc) == state_text::kLimitHighSoc);
static_assert(static_cast<int>(CarServer_ChargeState_ChargeLimitReason_ChargeLimitReasonCabin) == state_text::kLimitCabin);
static_assert(static_cast<int>(CarServer_MediaPlaybackStatus_Stopped) == state_text::kMediaStopped);
static_assert(static_cast<int>(CarServer_MediaPlaybackStatus_Playing) == state_text::kMediaPlaying);
static_assert(static_cast<int>(CarServer_MediaPlaybackStatus_Paused) == state_text::kMediaPaused);
static_assert(static_cast<int>(CarServer_MediaSourceType_MediaSourceType_AM) == 1);
static_assert(static_cast<int>(CarServer_MediaSourceType_MediaSourceType_Bluetooth) == 8);
static_assert(static_cast<int>(CarServer_MediaSourceType_MediaSourceType_Spotify) == 12);
static_assert(static_cast<int>(CarServer_MediaSourceType_MediaSourceType_MediaFile) == 16);
static_assert(static_cast<int>(CarServer_MediaSourceType_MediaSourceType_Browser) == 28);
static_assert(static_cast<int>(CarServer_MediaSourceType_MediaSourceType_Search) == 35);

VehicleStateManager::VehicleStateManager(TeslaBLEVehicle* parent)
    : parent_(parent) {}

// =============================================================================
// Generic sensor setters/getters
// =============================================================================

void VehicleStateManager::set_binary_sensor(const std::string& id, binary_sensor::BinarySensor* sensor) {
    if (sensor == nullptr) {
        return;
    }
    binary_sensors_[id] = sensor;
    ESP_LOGD(STATE_MANAGER_TAG, "Registered binary sensor: %s", id.c_str());
}

void VehicleStateManager::set_sensor(const std::string& id, sensor::Sensor* sensor) {
    if (sensor == nullptr) {
        return;
    }
    sensors_[id] = sensor;
    ESP_LOGD(STATE_MANAGER_TAG, "Registered sensor: %s", id.c_str());
}

void VehicleStateManager::set_text_sensor(const std::string& id, text_sensor::TextSensor* sensor) {
    if (sensor == nullptr) {
        return;
    }
    text_sensors_[id] = sensor;
    ESP_LOGD(STATE_MANAGER_TAG, "Registered text sensor: %s", id.c_str());
}

binary_sensor::BinarySensor* VehicleStateManager::get_binary_sensor(const std::string& id) {
    auto it = binary_sensors_.find(id);
    return (it != binary_sensors_.end()) ? it->second : nullptr;
}

sensor::Sensor* VehicleStateManager::get_sensor(const std::string& id) {
    auto it = sensors_.find(id);
    return (it != sensors_.end()) ? it->second : nullptr;
}

text_sensor::TextSensor* VehicleStateManager::get_text_sensor(const std::string& id) {
    auto it = text_sensors_.find(id);
    return (it != text_sensors_.end()) ? it->second : nullptr;
}

// Const versions
const binary_sensor::BinarySensor* VehicleStateManager::get_binary_sensor(const std::string& id) const {
    auto it = binary_sensors_.find(id);
    return (it != binary_sensors_.end()) ? it->second : nullptr;
}

const sensor::Sensor* VehicleStateManager::get_sensor(const std::string& id) const {
    auto it = sensors_.find(id);
    return (it != sensors_.end()) ? it->second : nullptr;
}

const text_sensor::TextSensor* VehicleStateManager::get_text_sensor(const std::string& id) const {
    auto it = text_sensors_.find(id);
    return (it != text_sensors_.end()) ? it->second : nullptr;
}

// =============================================================================
// Helper methods for publishing by ID
// =============================================================================

bool VehicleStateManager::publish_binary_sensor(const std::string& id, bool state) {
    auto* sensor = get_binary_sensor(id);
    return sensor != nullptr && publish_sensor_state(sensor, state);
}

bool VehicleStateManager::publish_sensor(const std::string& id, float state) {
    auto* sensor = get_sensor(id);
    return sensor != nullptr && publish_sensor_state(sensor, state);
}

bool VehicleStateManager::publish_text_sensor(const std::string& id, const std::string& state) {
    auto* sensor = get_text_sensor(id);
    return sensor != nullptr && publish_sensor_state(sensor, state);
}

// =============================================================================
// VCSEC State Updates
// =============================================================================

void VehicleStateManager::update_vehicle_status(const VCSEC_VehicleStatus& status) {
    ESP_LOGD(STATE_MANAGER_TAG, "Updating vehicle status");
    
    update_sleep_status(status.vehicleSleepStatus);
    update_lock_status(status.vehicleLockState);
    update_user_presence(status.userPresence);
    
    // Closures from VCSEC: also reported while the car sleeps, so doors,
    // frunk and trunk stay current without waking it for an infotainment poll.
    if (status.has_closureStatuses) {
        const auto &c = status.closureStatuses;
        auto open = [](VCSEC_ClosureState_E state) { return state_text::closure_open(static_cast<int>(state)); };
        if (auto o = open(c.frontDriverDoor)) publish_binary_sensor("door_driver_front", *o);
        if (auto o = open(c.rearDriverDoor)) publish_binary_sensor("door_driver_rear", *o);
        if (auto o = open(c.frontPassengerDoor)) publish_binary_sensor("door_passenger_front", *o);
        if (auto o = open(c.rearPassengerDoor)) publish_binary_sensor("door_passenger_rear", *o);
        if (auto o = open(c.frontTrunk)) publish_cover_open(frunk_cover_, *o);
        if (auto o = open(c.rearTrunk)) publish_cover_open(trunk_cover_, *o);
        if (auto o = open(c.chargePort)) update_charge_flap_open(*o);
    }
}

void VehicleStateManager::publish_cover_open(cover::Cover *cover, bool open) {
    if (cover == nullptr) return;
    const float position = open ? cover::COVER_OPEN : cover::COVER_CLOSED;
    if (cover->position == position && cover->has_state()) return;
    cover->position = position;
    cover->publish_state();
    cover->set_has_state(true);  // cover::publish_state() does not set it
}

void VehicleStateManager::update_sleep_status(VCSEC_VehicleSleepStatus_E status) {
    auto asleep = state_text::sleep_status(static_cast<int>(status));
    if (asleep.has_value()) {
        update_asleep(asleep.value());
    } else {
        set_sensor_available(get_binary_sensor("asleep"), false);
    }
}

void VehicleStateManager::update_lock_status(VCSEC_VehicleLockState_E status) {
    auto unlocked = state_text::lock_status(static_cast<int>(status));
    if (unlocked.has_value()) {
        update_unlocked(unlocked.value());
    }
}

void VehicleStateManager::update_user_presence(VCSEC_UserPresence_E presence) {
    auto present = state_text::user_presence(static_cast<int>(presence));
    if (present.has_value()) {
        update_user_present(present.value());
    } else {
        set_sensor_available(get_binary_sensor("user_present"), false);
    }
}

// =============================================================================
// CarServer State Updates
// =============================================================================

void VehicleStateManager::update_charge_state(const CarServer_ChargeState& charge_state) {
    ESP_LOGD(STATE_MANAGER_TAG, "Updating charge state");
    // Track whether charger is disconnected in this message to avoid stale estimate recomputation.
    bool charger_was_disconnected = false;
    
    // Update charging status and charging state text
    if (charge_state.has_charging_state) {
        const bool was_charging = is_charging_;
        const bool new_charging_state = state_text::is_charging(charge_state.charging_state.which_type);
        
        ESP_LOGD(STATE_MANAGER_TAG, "Charging state check: was=%s, new=%s, state_type=%d", 
                 was_charging ? "ON" : "OFF", 
                 new_charging_state ? "ON" : "OFF",
                 charge_state.charging_state.which_type);
        
        is_charging_ = new_charging_state;
        
        // Sync charging switch with vehicle state
        if (charging_switch_ && (!charging_switch_->has_state() || charging_switch_->state != is_charging_)) {
            ESP_LOGD(STATE_MANAGER_TAG, "Syncing charging switch to vehicle state: %s", is_charging_ ? "ON" : "OFF");
            publish_sensor_state(charging_switch_, is_charging_);
        }
        
        if (was_charging != is_charging_) {
            ESP_LOGD(STATE_MANAGER_TAG, "Charging state changed: %s", is_charging_ ? "ON" : "OFF");
            if (was_charging) save_charge_session_if_changed();
        }
        
        // Update text sensors
        publish_text_sensor("charging_state", state_text::charging_state(charge_state.charging_state.which_type));
        publish_text_sensor("iec61851_state", state_text::iec61851_state(charge_state.charging_state.which_type));
        
        // Update charger connected binary sensor
        const bool charger_connected = state_text::charger_connected(charge_state.charging_state.which_type);
        publish_binary_sensor("charger", charger_connected);
        cable_connected_ = charger_connected;
        if (!charger_connected) {
            // No cable: clear cached estimate inputs so a later reconnect without voltage doesn't reuse stale AC voltage.
            // The sensors keep showing the last charging session's values.
            charger_was_disconnected = true;
            cached_charger_voltage_ = NAN;
            cached_charger_current_ = 0.0f;
            cached_charger_phases_ = std::nullopt;
        }
    }
    
    // Update battery level
    if (charge_state.which_optional_battery_level) {
        const float battery_level = static_cast<float>(charge_state.optional_battery_level.battery_level);
        if (battery_level >= 0.0f && battery_level <= 100.0f && std::isfinite(battery_level)) {
            if (publish_sensor("battery_level", battery_level)) {
                ESP_LOGI(STATE_MANAGER_TAG, "Updating battery level to %.1f%%", battery_level);
            }
        }
    }
    
    // Update charger power - prefer native kW value from vehicle
    if (charge_state.which_optional_charger_power) {
        const float power_kw = static_cast<float>(charge_state.optional_charger_power.charger_power);
        if (power_kw >= 0.0f && power_kw <= 500.0f && std::isfinite(power_kw)) {
            publish_sensor("charger_power", power_kw);
        }
    }
    
    // Update range (battery_range is in miles, published in km)
    if (charge_state.which_optional_battery_range) {
        const float range = charge_state.optional_battery_range.battery_range;
        if (range >= 0.0f && range <= 500.0f && std::isfinite(range)) {
            publish_sensor("range", state_text::miles_to_km(range));
        }
    }
    
    // Estimated and ideal range (miles, like battery_range; published in km)
    if (charge_state.which_optional_est_battery_range) {
        const float range = charge_state.optional_est_battery_range.est_battery_range;
        if (range >= 0.0f && range <= 500.0f && std::isfinite(range)) {
            publish_sensor("est_battery_range", state_text::miles_to_km(range));
        }
    }
    if (charge_state.which_optional_ideal_battery_range) {
        const float range = charge_state.optional_ideal_battery_range.ideal_battery_range;
        if (range >= 0.0f && range <= 500.0f && std::isfinite(range)) {
            publish_sensor("ideal_battery_range", state_text::miles_to_km(range));
        }
    }

    // Usable battery level (%) - excludes energy the car keeps in reserve
    if (charge_state.which_optional_usable_battery_level) {
        const int32_t level = charge_state.optional_usable_battery_level.usable_battery_level;
        if (level >= 0 && level <= 100) {
            publish_sensor("usable_battery_level", static_cast<float>(level));
        }
    }

    // Range added this charging session (rated miles, published in km)
    if (charge_state.which_optional_charge_miles_added_rated) {
        const float added = charge_state.optional_charge_miles_added_rated.charge_miles_added_rated;
        if (added >= 0.0f && added <= 1000.0f && std::isfinite(added)) {
            publish_sensor("range_added", state_text::miles_to_km(added));
        }
    }

    // Update energy added (kWh)
    if (charge_state.which_optional_charge_energy_added) {
        const float energy = charge_state.optional_charge_energy_added.charge_energy_added;
        if (energy >= 0.0f && std::isfinite(energy)) {
            publish_sensor("energy_added", energy);
        }
    }
    
    // Update time to full charge (minutes)
    if (charge_state.which_optional_minutes_to_full_charge) {
        const float minutes = static_cast<float>(charge_state.optional_minutes_to_full_charge.minutes_to_full_charge);
        if (minutes >= 0.0f && std::isfinite(minutes)) {
            publish_sensor("time_to_full", minutes);
        }
    }
    
    // Time to charge limit (minutes)
    if (charge_state.which_optional_minutes_to_charge_limit) {
        const int32_t minutes = charge_state.optional_minutes_to_charge_limit.minutes_to_charge_limit;
        if (minutes >= 0) {
            publish_sensor("time_to_charge_limit", static_cast<float>(minutes));
        }
    }

    // Update charger voltage (cache for estimated power)
    if (charge_state.which_optional_charger_voltage) {
        const float voltage = static_cast<float>(charge_state.optional_charger_voltage.charger_voltage);
        if (voltage >= 0.0f && voltage <= 600.0f && std::isfinite(voltage)) {
            publish_sensor("charger_voltage", voltage);
            cached_charger_voltage_ = voltage;
        }
    }
    
    // Update charger current (cached for estimated power)
    if (charge_state.which_optional_charger_actual_current) {
        const float current = static_cast<float>(charge_state.optional_charger_actual_current.charger_actual_current);
        if (current >= 0.0f && current <= 100.0f && std::isfinite(current)) {
            publish_sensor("charger_current", current);
            cached_charger_current_ = current;
        }
    }

// Update EVSE max current (what the charger can theoretically provide)
    if (charge_state.which_optional_charger_pilot_current) {
        const float pilot_current = static_cast<float>(charge_state.optional_charger_pilot_current.charger_pilot_current);
        if (pilot_current >= 0.0f && pilot_current <= 100.0f && std::isfinite(pilot_current)) {
            publish_sensor("evse_max_current", pilot_current);
        }
    }

    // Update vehicle max acceptable charge current (onboard charger limit)
    if (charge_state.which_optional_charge_current_request_max) {
        const int32_t max_amps = charge_state.optional_charge_current_request_max.charge_current_request_max;
        if (max_amps > 0 && max_amps <= 100) {
            publish_sensor("vehicle_max_charge_current", static_cast<float>(max_amps));
            if (max_amps != charging_amps_max_) {
                ESP_LOGI(STATE_MANAGER_TAG, "Received new max charging amps: %" PRId32 " A", max_amps);
                update_charging_amps_max(max_amps);
            }
        }
    }

    // Update charge current request (what the car is actively requesting from the EVSE)
    if (charge_state.which_optional_charge_current_request) {
        const int32_t request = charge_state.optional_charge_current_request.charge_current_request;
        if (request >= 0 && request <= 100) {
            publish_sensor("charge_current_request", static_cast<float>(request));
        }
    }

    ESP_LOGD(STATE_MANAGER_TAG, "charge_limit_reason which=%d actual=%" PRId32 " request=%" PRId32 " pilot=%" PRId32,
             charge_state.which_optional_charge_limit_reason,
             charge_state.optional_charger_actual_current.charger_actual_current,
             charge_state.optional_charge_current_request.charge_current_request,
             charge_state.optional_charger_pilot_current.charger_pilot_current);

    const bool appears_externally_limited = is_charging_ && charge_state.which_optional_charge_current_request &&
                                            ((charge_state.which_optional_charger_actual_current &&
                                              charge_state.optional_charger_actual_current.charger_actual_current + 1 <
                                                  charge_state.optional_charge_current_request.charge_current_request) ||
                                             (charge_state.which_optional_charger_pilot_current &&
                                              charge_state.optional_charger_pilot_current.charger_pilot_current <
                                                  charge_state.optional_charge_current_request.charge_current_request));

    // Publish charge limit reason as text sensor.
    // Some BLE responses omit charge_limit_reason even when charging is externally limited.
    if (charge_state.which_optional_charge_limit_reason) {
        const auto reason = charge_state.optional_charge_limit_reason.charge_limit_reason;
        publish_text_sensor("charge_limit_reason", state_text::charge_limit_reason(static_cast<int>(reason)));
    } else if (appears_externally_limited) {
        publish_text_sensor("charge_limit_reason", "ExternalLimit");
    } else {
        publish_text_sensor("charge_limit_reason", "Unknown");
    }
    
    // Update charging rate
    if (charge_state.which_optional_charge_rate_mph) {
        const float rate_mph = static_cast<float>(charge_state.optional_charge_rate_mph.charge_rate_mph);
        publish_sensor("charging_rate", state_text::miles_to_km(rate_mph));
    }

    // Update charging amps (set to charging amp setpoint)
    if (charge_state.which_optional_charge_current_request && charging_amps_number_) {
        const float amps = static_cast<float>(charge_state.optional_charge_current_request.charge_current_request);
        update_charging_amps(amps);
    }
    
    // Update charge limit
    if (charge_state.which_optional_charge_limit_soc && charging_limit_number_) {
        const float limit = static_cast<float>(charge_state.optional_charge_limit_soc.charge_limit_soc);
        update_charging_limit(limit);
    }
    
    // Update charge port door cover (physical door open/closed)
    if (charge_state.which_optional_charge_port_door_open) {
        update_charge_flap_open(charge_state.optional_charge_port_door_open.charge_port_door_open);
    }
    
    // Charge schedule
    if (charge_state.which_optional_scheduled_charging_mode) {
        const int mode = static_cast<int>(charge_state.optional_scheduled_charging_mode.scheduled_charging_mode);
        publish_text_sensor("scheduled_charging_mode", state_text::scheduled_charging_mode(mode));
        scheduled_charging_on_ = mode == state_text::kScheduledChargingStartAt;
        if (scheduled_charging_switch_ != nullptr &&
            (!scheduled_charging_switch_->has_state() || scheduled_charging_switch_->state != scheduled_charging_on_)) {
            publish_sensor_state(scheduled_charging_switch_, scheduled_charging_on_);
        }
    }
    if (charge_state.which_optional_scheduled_charging_pending) {
        publish_binary_sensor("scheduled_charging_pending", charge_state.optional_scheduled_charging_pending.scheduled_charging_pending);
    }
    if (charge_state.which_optional_scheduled_charging_start_time_minutes) {
        const uint32_t minutes = charge_state.optional_scheduled_charging_start_time_minutes.scheduled_charging_start_time_minutes;
        if (minutes < 24 * 60) {
            const bool changed = static_cast<int>(minutes) != scheduled_charging_minutes_;
            scheduled_charging_minutes_ = static_cast<int>(minutes);
            if (scheduled_charging_time_ != nullptr && (changed || !scheduled_charging_time_->has_state())) {
                static_cast<TeslaScheduledChargingTime *>(scheduled_charging_time_)->update_time(scheduled_charging_minutes_);
            }
        }
    }
    // Scheduled departure (entities live on the vehicle)
    {
        auto &d = parent_->departure_;
        const auto before = d;
        if (charge_state.which_optional_scheduled_charging_mode) {
            d.enabled = static_cast<int>(charge_state.optional_scheduled_charging_mode.scheduled_charging_mode) ==
                        state_text::kScheduledChargingDepartBy;
            // Policies are only sent while set; the mode tells the data is there
            d.preconditioning = charge_state.has_preconditioning_times
                                    ? static_cast<int>(charge_state.preconditioning_times.which_times)
                                    : state_text::kPolicyOff;
            d.off_peak = charge_state.has_off_peak_charging_times
                             ? static_cast<int>(charge_state.off_peak_charging_times.which_times)
                             : state_text::kPolicyOff;
        }
        if (charge_state.which_optional_scheduled_departure_time_minutes &&
            charge_state.optional_scheduled_departure_time_minutes.scheduled_departure_time_minutes < 24 * 60)
            d.time = static_cast<int>(charge_state.optional_scheduled_departure_time_minutes.scheduled_departure_time_minutes);
        if (charge_state.which_optional_off_peak_hours_end_time &&
            charge_state.optional_off_peak_hours_end_time.off_peak_hours_end_time < 24 * 60)
            d.off_peak_end = static_cast<int>(charge_state.optional_off_peak_hours_end_time.off_peak_hours_end_time);
        if (d.enabled != before.enabled || d.time != before.time || d.preconditioning != before.preconditioning ||
            d.off_peak != before.off_peak || d.off_peak_end != before.off_peak_end ||
            (parent_->departure_switch_ != nullptr && !parent_->departure_switch_->has_state()))
            parent_->publish_departure_();
    }

    // Update charger phases (integer 1..3, cached for estimated power)
    if (charge_state.which_optional_charger_phases) {
        const float phases = static_cast<float>(charge_state.optional_charger_phases.charger_phases);
        if (phases >= 1.0f && phases <= 3.0f && std::isfinite(phases)) {
            publish_sensor("charger_phases", phases);
            cached_charger_phases_ = static_cast<int32_t>(phases);
            session_phases_ = static_cast<int32_t>(phases);
        }
    }

    // Update charge port latch lock (cable latch engaged/disengaged)
    if (charge_state.has_charge_port_latch) {
        latch_tag_ = charge_state.charge_port_latch.which_type;
        update_charge_port_latch_lock_();
    }

    // Deferred estimated power calculation: single publish per poll from cached voltage/current/phases.
    // Skipped when we already published 0 for disconnected to avoid overwriting with stale cached values.
    if (!charger_was_disconnected) {
        update_estimated_power();
    }
}

void VehicleStateManager::update_climate_state(const CarServer_ClimateState& climate_state) {
    ESP_LOGD(STATE_MANAGER_TAG, "Updating climate state");
    
    // Inside temperature (used internally for climate entity)
    if (climate_state.which_optional_inside_temp_celsius) {
        const float temp = climate_state.optional_inside_temp_celsius.inside_temp_celsius;
        if (temp >= -40.0f && temp <= 60.0f && std::isfinite(temp)) {
            current_inside_temp_ = temp;
        }
    }
    
    // Outside temperature
    if (climate_state.which_optional_outside_temp_celsius) {
        const float temp = climate_state.optional_outside_temp_celsius.outside_temp_celsius;
        if (temp >= -50.0f && temp <= 60.0f && std::isfinite(temp)) {
            publish_sensor("outside_temp", temp);
        }
    }
    
    // Driver temperature setting (used for climate entity target temp)
    if (climate_state.which_optional_driver_temp_setting) {
        const float temp = climate_state.optional_driver_temp_setting.driver_temp_setting;
        if (temp >= 15.0f && temp <= 30.0f && std::isfinite(temp)) {
            target_temp_ = temp;
        }
    }
    
    // Climate on status (used internally for climate entity)
    if (climate_state.which_optional_is_climate_on) {
        climate_on_ = climate_state.optional_is_climate_on.is_climate_on;
    }
    
    // Cabin Overheat Protection: mode and activation temperature in one select
    if (climate_state.which_optional_cabin_overheat_protection) {
        cop_mode_ = static_cast<int>(climate_state.optional_cabin_overheat_protection.cabin_overheat_protection);
    }
    if (climate_state.which_optional_cop_activation_temperature) {
        cop_level_ = static_cast<int>(climate_state.optional_cop_activation_temperature.cop_activation_temperature);
    }
    publish_cabin_overheat_();

    if (climate_state.which_optional_cabin_overheat_protection_actively_cooling) {
        publish_binary_sensor("cabin_overheat_active", climate_state.optional_cabin_overheat_protection_actively_cooling.cabin_overheat_protection_actively_cooling);
    }

    if (climate_state.which_optional_passenger_temp_setting) {
        const float temp = climate_state.optional_passenger_temp_setting.passenger_temp_setting;
        if (temp >= 15.0f && temp <= 30.0f && std::isfinite(temp)) {
            publish_sensor("passenger_temp_setting", temp);
        }
    }
    if (climate_state.which_optional_battery_heater) {
        publish_binary_sensor("battery_heater", climate_state.optional_battery_heater.battery_heater);
    }
    if (climate_state.which_optional_battery_heater_no_power) {
        publish_binary_sensor("battery_heater_no_power", climate_state.optional_battery_heater_no_power.battery_heater_no_power);
    } else if (climate_state.which_optional_battery_heater) {
        // Only sent when the problem occurs: absent next to a battery heater state means OK
        publish_binary_sensor("battery_heater_no_power", false);
    }
    if (climate_state.which_optional_steering_wheel_heat_level) {
        auto level = state_text::steering_wheel_heat_level(
            static_cast<int>(climate_state.optional_steering_wheel_heat_level.steering_wheel_heat_level));
        if (level.has_value()) publish_text_sensor("steering_wheel_heat_level", level.value());
    }
    if (climate_state.which_optional_is_preconditioning) {
        publish_binary_sensor("preconditioning", climate_state.optional_is_preconditioning.is_preconditioning);
    }
    if (climate_state.which_optional_is_front_defroster_on) {
        publish_binary_sensor("front_defroster", climate_state.optional_is_front_defroster_on.is_front_defroster_on);
    }
    if (climate_state.which_optional_is_rear_defroster_on) {
        publish_binary_sensor("rear_defroster", climate_state.optional_is_rear_defroster_on.is_rear_defroster_on);
    }

    // Seat heaters (each field only set when that seat has a heater)
    if (climate_state.which_optional_seat_heater_left) {
        const int level = climate_state.optional_seat_heater_left.seat_heater_left;
        publish_text_sensor("seat_heater_front_left_level", state_text::seat_heater_level(level));
        publish_binary_sensor("seat_heater_front_left", level > state_text::kSeatHeaterOff);
    }
    if (climate_state.which_optional_seat_heater_right) {
        const int level = climate_state.optional_seat_heater_right.seat_heater_right;
        publish_text_sensor("seat_heater_front_right_level", state_text::seat_heater_level(level));
        publish_binary_sensor("seat_heater_front_right", level > state_text::kSeatHeaterOff);
    }
    if (climate_state.which_optional_seat_heater_rear_left) {
        const int level = climate_state.optional_seat_heater_rear_left.seat_heater_rear_left;
        publish_text_sensor("seat_heater_rear_left_level", state_text::seat_heater_level(level));
        publish_binary_sensor("seat_heater_rear_left", level > state_text::kSeatHeaterOff);
    }
    if (climate_state.which_optional_seat_heater_rear_center) {
        const int level = climate_state.optional_seat_heater_rear_center.seat_heater_rear_center;
        publish_text_sensor("seat_heater_rear_center_level", state_text::seat_heater_level(level));
        publish_binary_sensor("seat_heater_rear_center", level > state_text::kSeatHeaterOff);
    }
    if (climate_state.which_optional_seat_heater_rear_right) {
        const int level = climate_state.optional_seat_heater_rear_right.seat_heater_rear_right;
        publish_text_sensor("seat_heater_rear_right_level", state_text::seat_heater_level(level));
        publish_binary_sensor("seat_heater_rear_right", level > state_text::kSeatHeaterOff);
    }

    // Steering wheel heater - sync switch state from vehicle
    if (climate_state.which_optional_steering_wheel_heater && steering_wheel_heat_switch_ != nullptr) {
        const bool heater_on = climate_state.optional_steering_wheel_heater.steering_wheel_heater;
        if (!steering_wheel_heat_switch_->has_state() || steering_wheel_heat_switch_->state != heater_on) {
            ESP_LOGD(STATE_MANAGER_TAG, "Syncing steering wheel heat switch to vehicle state: %s", heater_on ? "ON" : "OFF");
            publish_sensor_state(steering_wheel_heat_switch_, heater_on);
        }
    }
    
    // What the car reports for the climate entity, logged when it changes
    // (-1 = field not sent)
    {
        char summary[96];
        snprintf(summary, sizeof(summary), "climate_on=%d keeper=%d defrost=%d auto=%d preconditioning=%d",
                 climate_state.which_optional_is_climate_on ? climate_state.optional_is_climate_on.is_climate_on : -1,
                 climate_state.has_climate_keeper_mode ? climate_state.climate_keeper_mode.which_type : -1,
                 climate_state.has_defrost_mode ? climate_state.defrost_mode.which_type : -1,
                 climate_state.which_optional_is_auto_conditioning_on ? climate_state.optional_is_auto_conditioning_on.is_auto_conditioning_on : -1,
                 climate_state.which_optional_is_preconditioning ? climate_state.optional_is_preconditioning.is_preconditioning : -1);
        if (last_climate_summary_ != summary) {
            ESP_LOGI(STATE_MANAGER_TAG, "Climate reported: %s", summary);
            last_climate_summary_ = summary;
        }
    }

    // Preset (climate keeper / defrost) and fan mode (bioweapon defense)
    const char *preset = state_text::climate_preset(
        climate_state.has_climate_keeper_mode ? climate_state.climate_keeper_mode.which_type : 0,
        climate_state.has_defrost_mode ? climate_state.defrost_mode.which_type : 0);
    const char *fan_mode = climate_state.which_optional_bioweapon_mode_on
        ? state_text::climate_fan_mode(climate_state.optional_bioweapon_mode_on.bioweapon_mode_on)
        : nullptr;  // only reported by cars with a HEPA filter

    // Update climate entity with current state
    if (auto* tesla_climate = static_cast<TeslaClimate*>(climate_)) {
        tesla_climate->update_state(climate_on_, current_inside_temp_, target_temp_, preset, fan_mode);
    }
}

void VehicleStateManager::update_drive_state(const CarServer_DriveState& drive_state) {
    ESP_LOGD(STATE_MANAGER_TAG, "Updating drive state");
    
    // Shift state
    if (drive_state.has_shift_state) {
        publish_text_sensor("shift_state", state_text::shift_state(drive_state.shift_state.which_type));
        
        // Parking brake sensor - true when in P
        const bool parked = state_text::is_parked(drive_state.shift_state.which_type);
        publish_binary_sensor("parking_brake", parked);
    }
    
    // Odometer (hundredths of a mile, published in km)
    if (drive_state.which_optional_odometer_in_hundredths_of_a_mile) {
        const float odometer = static_cast<float>(drive_state.optional_odometer_in_hundredths_of_a_mile.odometer_in_hundredths_of_a_mile) / 100.0f;
        if (odometer >= 0.0f && std::isfinite(odometer)) {
            publish_sensor("odometer", state_text::miles_to_km(odometer));
        }
    }
}

void VehicleStateManager::update_tire_pressure_state(const CarServer_TirePressureState& tire_pressure_state) {
    ESP_LOGD(STATE_MANAGER_TAG, "Updating tire pressure state");
    
    // Tire pressures in bar
    if (tire_pressure_state.which_optional_tpms_pressure_fl) {
        const float pressure = tire_pressure_state.optional_tpms_pressure_fl.tpms_pressure_fl;
        if (pressure >= 0.0f && pressure <= 5.0f && std::isfinite(pressure)) {
            publish_sensor("tpms_front_left", pressure);
        }
    }
    
    if (tire_pressure_state.which_optional_tpms_pressure_fr) {
        const float pressure = tire_pressure_state.optional_tpms_pressure_fr.tpms_pressure_fr;
        if (pressure >= 0.0f && pressure <= 5.0f && std::isfinite(pressure)) {
            publish_sensor("tpms_front_right", pressure);
        }
    }
    
    if (tire_pressure_state.which_optional_tpms_pressure_rl) {
        const float pressure = tire_pressure_state.optional_tpms_pressure_rl.tpms_pressure_rl;
        if (pressure >= 0.0f && pressure <= 5.0f && std::isfinite(pressure)) {
            publish_sensor("tpms_rear_left", pressure);
        }
    }
    
    if (tire_pressure_state.which_optional_tpms_pressure_rr) {
        const float pressure = tire_pressure_state.optional_tpms_pressure_rr.tpms_pressure_rr;
        if (pressure >= 0.0f && pressure <= 5.0f && std::isfinite(pressure)) {
            publish_sensor("tpms_rear_right", pressure);
        }
    }

    // Low-pressure warnings: hard = significantly low, soft = slightly low
    const auto &t = tire_pressure_state;
    if (t.which_optional_tpms_hard_warning_fl) publish_binary_sensor("tpms_hard_warning_front_left", t.optional_tpms_hard_warning_fl.tpms_hard_warning_fl);
    if (t.which_optional_tpms_hard_warning_fr) publish_binary_sensor("tpms_hard_warning_front_right", t.optional_tpms_hard_warning_fr.tpms_hard_warning_fr);
    if (t.which_optional_tpms_hard_warning_rl) publish_binary_sensor("tpms_hard_warning_rear_left", t.optional_tpms_hard_warning_rl.tpms_hard_warning_rl);
    if (t.which_optional_tpms_hard_warning_rr) publish_binary_sensor("tpms_hard_warning_rear_right", t.optional_tpms_hard_warning_rr.tpms_hard_warning_rr);
    if (t.which_optional_tpms_soft_warning_fl) publish_binary_sensor("tpms_soft_warning_front_left", t.optional_tpms_soft_warning_fl.tpms_soft_warning_fl);
    if (t.which_optional_tpms_soft_warning_fr) publish_binary_sensor("tpms_soft_warning_front_right", t.optional_tpms_soft_warning_fr.tpms_soft_warning_fr);
    if (t.which_optional_tpms_soft_warning_rl) publish_binary_sensor("tpms_soft_warning_rear_left", t.optional_tpms_soft_warning_rl.tpms_soft_warning_rl);
    if (t.which_optional_tpms_soft_warning_rr) publish_binary_sensor("tpms_soft_warning_rear_right", t.optional_tpms_soft_warning_rr.tpms_soft_warning_rr);
}

void VehicleStateManager::update_closures_state(const CarServer_ClosuresState& closures_state) {
    ESP_LOGD(STATE_MANAGER_TAG, "Updating closures state");
    
    // Doors - update individual binary sensors
    if (closures_state.which_optional_door_open_driver_front) {
        publish_binary_sensor("door_driver_front", closures_state.optional_door_open_driver_front.door_open_driver_front);
    }
    if (closures_state.which_optional_door_open_driver_rear) {
        publish_binary_sensor("door_driver_rear", closures_state.optional_door_open_driver_rear.door_open_driver_rear);
    }
    if (closures_state.which_optional_door_open_passenger_front) {
        publish_binary_sensor("door_passenger_front", closures_state.optional_door_open_passenger_front.door_open_passenger_front);
    }
    if (closures_state.which_optional_door_open_passenger_rear) {
        publish_binary_sensor("door_passenger_rear", closures_state.optional_door_open_passenger_rear.door_open_passenger_rear);
    }
    
    // Trunks - update cover entities
    if (closures_state.which_optional_door_open_trunk_front) {
        publish_cover_open(frunk_cover_, closures_state.optional_door_open_trunk_front.door_open_trunk_front);
    }
    if (closures_state.which_optional_door_open_trunk_rear) {
        publish_cover_open(trunk_cover_, closures_state.optional_door_open_trunk_rear.door_open_trunk_rear);
    }
    
    // Windows - update individual binary sensors and aggregate cover
    bool window_df = false, window_dr = false, window_pf = false, window_pr = false;
    if (closures_state.which_optional_window_open_driver_front) {
        window_df = closures_state.optional_window_open_driver_front.window_open_driver_front;
        publish_binary_sensor("window_driver_front", window_df);
    }
    if (closures_state.which_optional_window_open_driver_rear) {
        window_dr = closures_state.optional_window_open_driver_rear.window_open_driver_rear;
        publish_binary_sensor("window_driver_rear", window_dr);
    }
    if (closures_state.which_optional_window_open_passenger_front) {
        window_pf = closures_state.optional_window_open_passenger_front.window_open_passenger_front;
        publish_binary_sensor("window_passenger_front", window_pf);
    }
    if (closures_state.which_optional_window_open_passenger_rear) {
        window_pr = closures_state.optional_window_open_passenger_rear.window_open_passenger_rear;
        publish_binary_sensor("window_passenger_rear", window_pr);
    }
    const bool any_window_open = window_df || window_dr || window_pf || window_pr;
    
    if (windows_cover_ != nullptr) {
        windows_cover_->position = any_window_open ? cover::COVER_OPEN : cover::COVER_CLOSED;
        windows_cover_->publish_state();
    }
    
    // Sunroof (any percent open > 0 means open)
    if (closures_state.which_optional_sun_roof_percent_open) {
        const bool sunroof_open = closures_state.optional_sun_roof_percent_open.sun_roof_percent_open > 0;
        publish_binary_sensor("sunroof", sunroof_open);
    }
    
    // Sentry mode - sync switch state from vehicle
    if (closures_state.has_sentry_mode_state && sentry_mode_switch_ != nullptr) {
        const bool sentry_active = (closures_state.sentry_mode_state.which_type == CarServer_ClosuresState_SentryModeState_Armed_tag ||
                              closures_state.sentry_mode_state.which_type == CarServer_ClosuresState_SentryModeState_Aware_tag ||
                              closures_state.sentry_mode_state.which_type == CarServer_ClosuresState_SentryModeState_Panic_tag);
        if (!sentry_mode_switch_->has_state() || sentry_mode_switch_->state != sentry_active) {
            ESP_LOGD(STATE_MANAGER_TAG, "Syncing sentry mode switch to vehicle state: %s", sentry_active ? "ON" : "OFF");
            publish_sensor_state(sentry_mode_switch_, sentry_active);
        }
    }

    if (closures_state.which_optional_sentry_mode_available) {
        sentry_mode_available_ = closures_state.optional_sentry_mode_available.sentry_mode_available;
    }

    // Speed limit mode (only set when the car supports it)
    if (closures_state.has_speed_limit_mode) {
        const auto &mode = closures_state.speed_limit_mode;
        if (mode.which_optional_active) {
            publish_binary_sensor("speed_limit_mode", mode.optional_active.active);
        }
        if (mode.which_optional_current_limit_mph) {
            const float limit = mode.optional_current_limit_mph.current_limit_mph;
            if (limit > 0.0f && limit <= 200.0f && std::isfinite(limit)) {
                publish_sensor("speed_limit", state_text::miles_to_km(limit));
            }
        }
    }
    
    // Locked state (update the doors lock entity from closures if available)
    if (closures_state.which_optional_locked) {
        update_unlocked(!closures_state.optional_locked.locked);
    }
}

// =============================================================================
// Direct state update methods
// =============================================================================

void VehicleStateManager::update_asleep(bool asleep) {
    if (publish_binary_sensor("asleep", asleep)) {
        ESP_LOGI(STATE_MANAGER_TAG, "Vehicle sleep state: %s", asleep ? "ASLEEP" : "AWAKE");
    }
}

void VehicleStateManager::update_unlocked(bool unlocked) {
    doors_unlocked_ = unlocked;
    // Update doors lock entity
    if (doors_lock_ != nullptr) {
        auto new_state = unlocked ? lock::LOCK_STATE_UNLOCKED : lock::LOCK_STATE_LOCKED;
        if (doors_lock_->state != new_state) {
            doors_lock_->publish_state(new_state);
            ESP_LOGI(STATE_MANAGER_TAG, "Vehicle lock state: %s", unlocked ? "UNLOCKED" : "LOCKED");
        }
    }
}

void VehicleStateManager::update_user_present(bool present) {
    if (publish_binary_sensor("user_present", present)) {
        ESP_LOGI(STATE_MANAGER_TAG, "User presence: %s", present ? "PRESENT" : "NOT_PRESENT");
    }
}

void VehicleStateManager::update_charge_flap_open(bool open) {
    // Update charge port door cover entity with VCSEC data
    publish_cover_open(charge_port_door_cover_, open);
    charge_port_door_open_ = open;
    // Without a cable the latch lock follows the door
    if ((cable_connected_.has_value() && !*cable_connected_) ||
        (latch_tag_ != state_text::kLatchEngaged && latch_tag_ != state_text::kLatchDisengaged &&
         latch_tag_ != state_text::kLatchBlocking))
        update_charge_port_latch_lock_();
}

void VehicleStateManager::update_charging_amps(float amps) {
    ESP_LOGD(STATE_MANAGER_TAG, "Charging amps setpoint from vehicle: %.1f A", amps);
    publish_sensor_state(charging_amps_number_, amps);
}

void VehicleStateManager::update_charging_limit(float limit) {
    publish_sensor_state(charging_limit_number_, limit);
}

void VehicleStateManager::update_charging_control_state(bool charging) {
    publish_sensor_state(charging_switch_, charging);
}

void VehicleStateManager::update_steering_wheel_heat(bool enabled) {
    publish_sensor_state(steering_wheel_heat_switch_, enabled);
}

void VehicleStateManager::update_sentry_mode(bool enabled) {
    publish_sensor_state(sentry_mode_switch_, enabled);
}

void VehicleStateManager::republish_sentry_mode() {
    if (sentry_mode_switch_ != nullptr) sentry_mode_switch_->publish_state(sentry_mode_switch_->state);
}

void VehicleStateManager::republish_charging_amps() {
    if (charging_amps_number_ != nullptr && charging_amps_number_->has_state()) {
        charging_amps_number_->publish_state(charging_amps_number_->state);
    }
}

void VehicleStateManager::republish_charging_limit() {
    if (charging_limit_number_ != nullptr && charging_limit_number_->has_state()) {
        charging_limit_number_->publish_state(charging_limit_number_->state);
    }
}

void VehicleStateManager::update_charger_connected(bool connected) {
    publish_binary_sensor("charger", connected);
}

void VehicleStateManager::update_estimated_power() {
    // Pure helper: compute estimated power (kW) = V * A * phases / 1000.
    // Using std::optional for explicit missing-data handling (modern C++17) and early returns.
    if (!cached_charger_phases_.has_value()) {
        return;
    }
    const int32_t phases = cached_charger_phases_.value();
    if (phases < 1 || phases > 3) {
        return;
    }
    if (!std::isfinite(cached_charger_voltage_) || !std::isfinite(cached_charger_current_)) {
        return;
    }
    if (cached_charger_voltage_ < 0.0f || cached_charger_voltage_ > 600.0f ||
        cached_charger_current_ < 0.0f || cached_charger_current_ > 100.0f) {
        return;
    }
    const float power_kw = cached_charger_voltage_ * cached_charger_current_ *
                           static_cast<float>(phases) / 1000.0f;
    if (!std::isfinite(power_kw) || power_kw < 0.0f || power_kw > 100.0f) {
        return;
    }
    // Only while charging: afterwards the sensor keeps the session's last value
    if (!is_charging_) {
        return;
    }
    publish_sensor("charger_power_estimated", power_kw);
    session_power_kw_ = power_kw;
}

void VehicleStateManager::republish_scheduled_charging() {
    // Undo an optimistic switch flip the car never got
    if (scheduled_charging_switch_ != nullptr) {
        scheduled_charging_switch_->publish_state(scheduled_charging_on_);
    }
}

void VehicleStateManager::update_charge_port_latch_lock_() {
    auto st = state_text::charge_port_latch_lock(latch_tag_, charge_port_door_open_, cable_connected_);
    if (!st.has_value()) return;
    latch_lock_state_ = *st == state_text::LatchLock::LOCKED   ? lock::LOCK_STATE_LOCKED
                      : *st == state_text::LatchLock::UNLOCKED ? lock::LOCK_STATE_UNLOCKED
                                                               : lock::LOCK_STATE_JAMMED;
    if (charge_port_latch_lock_ != nullptr && charge_port_latch_lock_->state != latch_lock_state_) {
        charge_port_latch_lock_->publish_state(latch_lock_state_);
        ESP_LOGD(STATE_MANAGER_TAG, "Charge port latch: %s", LOG_STR_ARG(lock::lock_state_to_string(latch_lock_state_)));
    }
}

void VehicleStateManager::republish_charge_port_latch() {
    // Replace a stuck LOCKING / UNLOCKING with the last state the car reported
    if (charge_port_latch_lock_ == nullptr || latch_lock_state_ == lock::LOCK_STATE_NONE) return;
    if (charge_port_latch_lock_->state != latch_lock_state_) charge_port_latch_lock_->publish_state(latch_lock_state_);
}

void VehicleStateManager::republish_doors_lock() {
    if (doors_lock_ == nullptr || !doors_unlocked_.has_value()) return;
    const auto st = *doors_unlocked_ ? lock::LOCK_STATE_UNLOCKED : lock::LOCK_STATE_LOCKED;
    if (doors_lock_->state != st) doors_lock_->publish_state(st);
}

void VehicleStateManager::publish_cabin_overheat_() {
    if (cabin_overheat_select_ == nullptr) return;
    auto option = state_text::cop_option(cop_mode_, cop_level_);
    if (option.has_value()) cabin_overheat_select_->publish_state(*option);
}

void VehicleStateManager::republish_cabin_overheat() {
    publish_cabin_overheat_();
}

void VehicleStateManager::save_charge_session_if_changed() {
    if (session_phases_ == 0 || !std::isfinite(session_power_kw_)) return;
    // Skip unchanged or near-identical sessions: saves flash writes
    if (session_phases_ == saved_session_phases_ && std::isfinite(saved_session_power_kw_) &&
        std::fabs(session_power_kw_ - saved_session_power_kw_) < 0.05f) {
        return;
    }
    parent_->save_charge_session_(session_phases_, session_power_kw_);
    saved_session_phases_ = session_phases_;
    saved_session_power_kw_ = session_power_kw_;
}

void VehicleStateManager::restore_charge_session(int32_t phases, float power_kw) {
    if (phases < 1 || phases > 3 || !std::isfinite(power_kw) || power_kw < 0.0f || power_kw > 100.0f) return;
    session_phases_ = saved_session_phases_ = phases;
    session_power_kw_ = saved_session_power_kw_ = power_kw;
    publish_sensor("charger_phases", static_cast<float>(phases));
    publish_sensor("charger_power_estimated", power_kw);
}

void VehicleStateManager::update_ble_rssi(float rssi) {
    publish_sensor("ble_rssi", rssi);
}

void VehicleStateManager::update_discovery(const std::string& state, const std::string& mac) {
    publish_text_sensor("discovery", state);
    publish_text_sensor("ble_mac", mac);
}

void VehicleStateManager::update_present(bool present) {
    publish_binary_sensor("present", present);
}

void VehicleStateManager::update_ble_advert_rssi(float rssi) {
    publish_sensor("ble_advert_rssi", rssi);
}

void VehicleStateManager::update_media_text(const std::string& title, const std::string& artist,
                                            const std::string& source) {
    publish_text_sensor("media_title", title);
    publish_text_sensor("media_artist", artist);
    publish_text_sensor("media_source", source);
}

// =============================================================================
// Connection state management
// =============================================================================

void VehicleStateManager::set_sensors_available(bool available) {
    ESP_LOGD(STATE_MANAGER_TAG, "Setting sensors available: %s", available ? "true" : "false");
    
    // Set availability for key binary sensors
    set_sensor_available(get_binary_sensor("asleep"), available);
    set_sensor_available(get_binary_sensor("user_present"), available);
}

void VehicleStateManager::reset_all_states() {
    ESP_LOGD(STATE_MANAGER_TAG, "Resetting all vehicle states");
    is_charging_ = false;
    set_sensors_available(false);
}

// =============================================================================
// State queries
// =============================================================================

bool VehicleStateManager::is_asleep() const {
    auto* sensor = get_binary_sensor("asleep");
    return sensor ? sensor->state : true;
}

bool VehicleStateManager::is_sentry_mode() const {
    return sentry_mode_switch_ != nullptr && sentry_mode_switch_->state;
}

bool VehicleStateManager::is_charge_flap_open() const {
    // Use charge port door cover entity if available
    if (charge_port_door_cover_) {
        return charge_port_door_cover_->position == cover::COVER_OPEN;
    }
    return false;
}

float VehicleStateManager::get_charging_amps() const {
    return charging_amps_number_ ? charging_amps_number_->state : 0.0f;
}

// =============================================================================
// Dynamic limits
// =============================================================================

void VehicleStateManager::update_charging_amps_max(int32_t new_max) {
    if (new_max <= 0) {
        ESP_LOGW(STATE_MANAGER_TAG, "Invalid max charging amps: %" PRId32 " A", new_max);
        return;
    }

    if (new_max == charging_amps_max_) {
        return;
    }

    charging_amps_max_ = new_max;

    if (parent_) {
        parent_->save_charging_amps_max_(new_max);
    }

    if (charging_amps_number_) {
        charging_amps_number_->traits.set_max_value(static_cast<float>(new_max));
#ifdef USE_API
        if (api::global_api_server != nullptr) {
            // ESPHome sends number traits only during API entity discovery.
            ESP_LOGI(STATE_MANAGER_TAG, "Reconnecting API clients to refresh charging amps limit");
            for (const auto &client : api::global_api_server->active_clients()) {
                client->on_fatal_error();
            }
        }
#endif
    }
    ESP_LOGI(STATE_MANAGER_TAG, "Updated max charging amps to %" PRId32 " A", new_max);
}

// =============================================================================
// Private helper methods
// =============================================================================

} // namespace tesla_ble_vehicle
} // namespace esphome
