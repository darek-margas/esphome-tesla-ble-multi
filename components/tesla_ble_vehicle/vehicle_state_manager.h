#pragma once

#include <esphome/core/log.h>
#include <esphome/components/binary_sensor/binary_sensor.h>
#include <esphome/components/sensor/sensor.h>
#include <esphome/components/text_sensor/text_sensor.h>
#include <esphome/components/switch/switch.h>
#include <esphome/components/number/number.h>
#include <esphome/components/lock/lock.h>
#include <esphome/components/cover/cover.h>
#include <esphome/components/climate/climate.h>
#include <esphome/components/select/select.h>
#include <esphome/components/datetime/time_entity.h>
#include <optional>
#include <map>
#include <string>
#include <cmath>
#include <type_traits>
#include <car_server.pb.h>
#include <vcsec.pb.h>

namespace esphome {
namespace tesla_ble_vehicle {

static const char *const STATE_MANAGER_TAG = "tesla_state_manager";

// Forward declarations
class TeslaBLEVehicle;

/**
 * @brief Vehicle state manager
 * 
 * This class manages the vehicle's state including sensors, switches, and numbers.
 * It uses a map-based approach for sensor storage, making it easy to add new sensors
 * without modifying C++ code - just add to the Python sensor definitions.
 * 
 * Sensors are stored by string ID and can be accessed via get_*() methods in update functions.
 */
class VehicleStateManager {
public:
    explicit VehicleStateManager(TeslaBLEVehicle* parent);
    
    // ==========================================================================
    // Generic sensor setters - use these from Python codegen
    // ==========================================================================
    void set_binary_sensor(const std::string& id, binary_sensor::BinarySensor* sensor);
    void set_sensor(const std::string& id, sensor::Sensor* sensor);
    void set_text_sensor(const std::string& id, text_sensor::TextSensor* sensor);
    
    // Generic sensor getters - use these in update methods
    binary_sensor::BinarySensor* get_binary_sensor(const std::string& id);
    sensor::Sensor* get_sensor(const std::string& id);
    text_sensor::TextSensor* get_text_sensor(const std::string& id);
    
    // Const versions for state queries
    const binary_sensor::BinarySensor* get_binary_sensor(const std::string& id) const;
    const sensor::Sensor* get_sensor(const std::string& id) const;
    const text_sensor::TextSensor* get_text_sensor(const std::string& id) const;
    
    // ==========================================================================
    // Control setters (switches and numbers need special handling)
    // ==========================================================================
    void set_charging_switch(switch_::Switch* sw) { charging_switch_ = sw; }
    void set_sentry_mode_switch(switch_::Switch* sw) { sentry_mode_switch_ = sw; }
    void set_steering_wheel_heat_switch(switch_::Switch* sw) { steering_wheel_heat_switch_ = sw; }
    void set_charging_amps_number(number::Number* number) {
        charging_amps_number_ = number;
        if (number != nullptr && charging_amps_max_ > 0) {
            number->traits.set_max_value(static_cast<float>(charging_amps_max_));
        }
    }
    void set_charging_limit_number(number::Number* number) { charging_limit_number_ = number; }
    void set_cabin_overheat_select(select::Select* sel) { cabin_overheat_select_ = sel; }
    int cop_mode() const { return cop_mode_; }    // -1 = not reported yet
    int cop_level() const { return cop_level_; }  // 0 = not reported yet
    void republish_cabin_overheat();
    void set_scheduled_charging_switch(switch_::Switch* sw) { scheduled_charging_switch_ = sw; }
    void set_scheduled_charging_time(datetime::TimeEntity* time) { scheduled_charging_time_ = time; }
    int scheduled_charging_minutes() const { return scheduled_charging_minutes_; }  // -1 = unknown
    void republish_scheduled_charging();
    // Put a lock showing LOCKING / UNLOCKING back to the last state the car reported
    void republish_charge_port_latch();
    void republish_doors_lock();
    bool is_climate_on() const { return climate_on_; }
    
    // ==========================================================================
    // Lock, Cover, and Climate setters
    // ==========================================================================
    void set_doors_lock(lock::Lock* lck) { doors_lock_ = lck; }
    void set_charge_port_latch_lock(lock::Lock* lck) { charge_port_latch_lock_ = lck; }
    void set_trunk_cover(cover::Cover* cvr) { trunk_cover_ = cvr; }
    void set_frunk_cover(cover::Cover* cvr) { frunk_cover_ = cvr; }
    void set_windows_cover(cover::Cover* cvr) { windows_cover_ = cvr; }
    void set_charge_port_door_cover(cover::Cover* cvr) { charge_port_door_cover_ = cvr; }
    void set_climate(climate::Climate* clm) { climate_ = clm; }
    
    // Lock, Cover, Climate getters (for state manager access)
    lock::Lock* get_doors_lock() { return doors_lock_; }
    lock::Lock* get_charge_port_latch_lock() { return charge_port_latch_lock_; }
    cover::Cover* get_trunk_cover() { return trunk_cover_; }
    cover::Cover* get_frunk_cover() { return frunk_cover_; }
    cover::Cover* get_windows_cover() { return windows_cover_; }
    cover::Cover* get_charge_port_door_cover() { return charge_port_door_cover_; }
    climate::Climate* get_climate() { return climate_; }
    
    // ==========================================================================
    // State updates from VCSEC
    // ==========================================================================
    void update_vehicle_status(const VCSEC_VehicleStatus& status);
    void update_sleep_status(VCSEC_VehicleSleepStatus_E status);
    void update_lock_status(VCSEC_VehicleLockState_E status);
    void update_user_presence(VCSEC_UserPresence_E presence);
    
    // ==========================================================================
    // State updates from CarServer (Infotainment)
    // ==========================================================================
    void update_charge_state(const CarServer_ChargeState& charge_state);
    void update_climate_state(const CarServer_ClimateState& climate_state);
    void update_drive_state(const CarServer_DriveState& drive_state);
    void update_tire_pressure_state(const CarServer_TirePressureState& tire_pressure_state);
    void update_closures_state(const CarServer_ClosuresState& closures_state);
    
    // ==========================================================================
    // Direct state updates (for specific use cases)
    // ==========================================================================
    void update_asleep(bool asleep);
    void update_unlocked(bool unlocked);
    void update_user_present(bool present);
    void update_charge_flap_open(bool open);
    void update_charging_amps(float amps);
    void update_charging_limit(float limit);
    void update_charging_control_state(bool charging);
    void update_steering_wheel_heat(bool enabled);
    void update_sentry_mode(bool enabled);
    void republish_sentry_mode();  // undo a switch flip that was not sent
    void republish_charging_amps();
    void republish_charging_limit();
    void update_charger_connected(bool connected);
    void update_ble_rssi(float rssi);
    void update_present(bool present);
    // BLE MAC discovery state ("Searching", "Found", ...) and the MAC in use
    void update_discovery(const std::string& state, const std::string& mac);
    void update_ble_advert_rssi(float rssi);  // NAN when not heard
    // Now playing; empty strings clear them (car asleep / nothing reported)
    void update_media_text(const std::string& title, const std::string& artist, const std::string& source);
    
    // ==========================================================================
    // Connection state management
    // ==========================================================================
    void set_sensors_available(bool available);
    void reset_all_states();
    
    // ==========================================================================
    // State queries
    // ==========================================================================
    bool is_asleep() const;
    bool is_sentry_mode() const;
    // false only when the car reported that sentry mode is not available
    bool sentry_mode_available() const { return sentry_mode_available_; }
    bool is_charge_flap_open() const;
    bool is_charging() const { return is_charging_; }
    float get_charging_amps() const;
    
    // ==========================================================================
    // Dynamic limits
    // ==========================================================================
    void update_charging_amps_max(int32_t new_max);
    // Last charging session's phases / estimated power, restored from flash at boot
    void restore_charge_session(int32_t phases, float power_kw);
    int get_charging_amps_max() const { return charging_amps_max_; }
    void set_charging_amps_max(int max) {
        if (max <= 0) return;
        charging_amps_max_ = max;
        if (charging_amps_number_) {
            charging_amps_number_->traits.set_max_value(static_cast<float>(max));
        }
    }
    
    
private:
    TeslaBLEVehicle* parent_;
    
    // ==========================================================================
    // Sensor storage maps - sensors are stored by string ID
    // ==========================================================================
    std::map<std::string, binary_sensor::BinarySensor*> binary_sensors_;
    std::map<std::string, sensor::Sensor*> sensors_;
    std::map<std::string, text_sensor::TextSensor*> text_sensors_;
    
    // ==========================================================================
    // Controls (special handling needed, not in maps)
    // ==========================================================================
    switch_::Switch* charging_switch_{nullptr};
    switch_::Switch* sentry_mode_switch_{nullptr};
    switch_::Switch* steering_wheel_heat_switch_{nullptr};
    number::Number* charging_amps_number_{nullptr};
    number::Number* charging_limit_number_{nullptr};
    select::Select* cabin_overheat_select_{nullptr};
    int cop_mode_{-1};
    int cop_level_{0};
    void publish_cabin_overheat_();
    switch_::Switch* scheduled_charging_switch_{nullptr};
    datetime::TimeEntity* scheduled_charging_time_{nullptr};
    int scheduled_charging_minutes_{-1};
    bool scheduled_charging_on_{false};
    
    // ==========================================================================
    // Lock, Cover, and Climate entities
    // ==========================================================================
    lock::Lock* doors_lock_{nullptr};
    lock::Lock* charge_port_latch_lock_{nullptr};
    cover::Cover* trunk_cover_{nullptr};
    cover::Cover* frunk_cover_{nullptr};
    cover::Cover* windows_cover_{nullptr};
    cover::Cover* charge_port_door_cover_{nullptr};
    climate::Climate* climate_{nullptr};
    
    // ==========================================================================
    // Internal state tracking
    // ==========================================================================
    bool is_charging_{false};
    int charging_amps_max_{0};
    
    // Climate state tracking
    float current_inside_temp_{NAN};
    float target_temp_{21.0f};
    bool climate_on_{false};
    bool sentry_mode_available_{true};
    int latch_tag_{0};
    std::optional<bool> charge_port_door_open_;
    std::optional<bool> cable_connected_;
    lock::LockState latch_lock_state_{lock::LOCK_STATE_NONE};
    std::optional<bool> doors_unlocked_;
    void update_charge_port_latch_lock_();
    std::string last_climate_summary_;

    // Cached values for estimated power calculation (V * A * phases / 1000)
    // Using NAN/optional sentinel pattern consistent with existing climate temps (NAN) and modern optional for phases.
    float cached_charger_voltage_{NAN};
    float cached_charger_current_{NAN};
    std::optional<int32_t> cached_charger_phases_{std::nullopt};
    void update_estimated_power();

    // Charger Phases / Charger Power Estimated keep the last charging session's
    // values after charging stops or the cable is unplugged, and survive a
    // reboot: saved to flash once per session, when charging stops.
    int32_t session_phases_{0};         // 0 = none yet
    float session_power_kw_{NAN};
    int32_t saved_session_phases_{0};
    float saved_session_power_kw_{NAN};
    void save_charge_session_if_changed();

    
    // ==========================================================================
    // Helper methods for publishing sensor state
    // ==========================================================================
    bool publish_binary_sensor(const std::string& id, bool state);
    bool publish_sensor(const std::string& id, float state);
    bool publish_text_sensor(const std::string& id, const std::string& state);
    void publish_cover_open(cover::Cover *cover, bool open);  // publishes only on change

    template<typename T, typename V> static bool publish_sensor_state(T *entity, V state) {
      if (entity == nullptr) return false;
      if constexpr (std::is_floating_point<V>::value) {
        // NAN means "unknown". Any comparison with NAN is false, so handle
        // the transitions explicitly or a sensor stays unknown forever.
        const bool was_nan = std::isnan(entity->state);
        const bool is_nan = std::isnan(state);
        if (!entity->has_state() || was_nan != is_nan ||
            (!is_nan && std::abs(entity->state - state) > 0.001f)) {
          entity->publish_state(state);
          return true;
        }
      } else {
        if (!entity->has_state() || entity->state != state) {
          entity->publish_state(state);
          return true;
        }
      }
      return false;
    }

    template<typename T> static void set_sensor_available(T *entity, bool available) {
      if (entity != nullptr) entity->set_has_state(available);
    }

};

} // namespace tesla_ble_vehicle
} // namespace esphome
