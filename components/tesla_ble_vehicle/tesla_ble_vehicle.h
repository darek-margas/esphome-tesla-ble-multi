#pragma once

#include <memory>
#include <map>
#include <string>
#include <functional>
#include <deque>
#include <esphome/components/esp32_ble_client/ble_client_base.h>
#include <esphome/components/esp32_ble_tracker/esp32_ble_tracker.h>
#include <esphome/components/binary_sensor/binary_sensor.h>
#include <esphome/components/sensor/sensor.h>
#include <esphome/components/text_sensor/text_sensor.h>
#include <esphome/components/button/button.h>
#include <esphome/components/switch/switch.h>
#include <esphome/components/number/number.h>
#include <esphome/components/lock/lock.h>
#include <esphome/components/cover/cover.h>
#include <esphome/components/climate/climate.h>
#include <esphome/components/select/select.h>
#include <esphome/components/datetime/time_entity.h>
#include <esphome/components/media_player/media_player.h>
#include <esphome/core/component.h>
#include <esphome/core/automation.h>
#include <esphome/core/preferences.h>

#include "ble_adapter_impl.h"
#include "control_state_policy.h"
#include "polling_policy.h"
#include "connection_reset_policy.h"
#include "link_scheduler.h"
#include "storage_adapter_impl.h"
#include <vehicle.h>
#include "vehicle_state_manager.h"
#include "state_text.h"

namespace esphome {
namespace tesla_ble_vehicle {

namespace espbt = esphome::esp32_ble_tracker;

static const char *const TAG = "tesla_ble_vehicle";
constexpr int DEFAULT_CHARGING_AMPS_MAX = 32;
constexpr int MIN_CHARGING_LIMIT = 50;
constexpr int MAX_CHARGING_LIMIT = 100;

// Tesla BLE service UUIDs
static const char *const SERVICE_UUID = "00000211-b2d1-43f0-9b88-960cebf8b91e";
static const char *const READ_UUID = "00000213-b2d1-43f0-9b88-960cebf8b91e";
static const char *const WRITE_UUID = "00000212-b2d1-43f0-9b88-960cebf8b91e";

/**
 * @brief Main Tesla BLE Vehicle component
 * 
 * This is the main component that coordinates all Tesla BLE operations.
 * It uses specialized managers for different aspects of the communication.
 * 
 * Sensor registration is done via generic methods:
 *   - set_binary_sensor(id, sensor)
 *   - set_sensor(id, sensor)
 *   - set_text_sensor(id, sensor)
 * 
 * This allows adding new sensors purely in Python without C++ changes.
 */
class TeslaBLEClient;

class TeslaBLEVehicle : public PollingComponent {
public:
    TeslaBLEVehicle();
    ~TeslaBLEVehicle() = default;

    // ESPHome component lifecycle
    void setup() override;
    void loop() override;
    void update() override;
    void dump_config() override;

    // BLE event handling
    void gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                           esp_ble_gattc_cb_param_t *param);

    // ==========================================================================
    // Configuration setters
    // ==========================================================================
    void set_vin(const char *vin);
    void set_debug_name(const std::string &name) { debug_name_ = name; }
    void set_role(const std::string &role);
    void set_charging_amps_max(int amps_max);
    
    // Polling interval setters
    void set_vcsec_poll_interval(uint32_t interval_ms);
    void set_infotainment_poll_interval_awake(uint32_t interval_ms);
    void set_infotainment_poll_interval_active(uint32_t interval_ms);
    void set_infotainment_sleep_timeout(uint32_t interval_ms);
    void set_wake_on_boot(bool wake) { wake_on_boot_ = wake; }
    void set_presence_timeout(uint32_t timeout_ms) { presence_timeout_ms_ = timeout_ms; }

    // ==========================================================================
    // Generic sensor setters - delegates to state manager
    // These are the primary interface for Python codegen
    // ==========================================================================
    void set_binary_sensor(const std::string& id, binary_sensor::BinarySensor* sensor);
    void set_sensor(const std::string& id, sensor::Sensor* sensor);
    void set_text_sensor(const std::string& id, text_sensor::TextSensor* sensor);

    // ==========================================================================
    // Control setters (switches and numbers need special handling)
    // ==========================================================================
    void set_charging_switch(switch_::Switch *sw);
    void set_steering_wheel_heat_switch(switch_::Switch *sw);
    void set_sentry_mode_switch(switch_::Switch *sw);
    void set_charging_amps_number(number::Number *number);
    void set_charging_limit_number(number::Number *number);
    void set_cabin_overheat_select(select::Select *sel);
    void set_scheduled_charging_switch(switch_::Switch *sw);
    void set_scheduled_charging_time_entity(datetime::TimeEntity *time);

    // ==========================================================================
    // Lock, Cover, and Climate setters
    // ==========================================================================
    void set_doors_lock(lock::Lock *lck);
    void set_charge_port_latch_lock(lock::Lock *lck);
    void set_trunk_cover(cover::Cover *cvr);
    void set_frunk_cover(cover::Cover *cvr);
    void set_windows_cover(cover::Cover *cvr);
    void set_charge_port_door_cover(cover::Cover *cvr);
    void set_climate(climate::Climate *clm);

    // ==========================================================================
    // Button setters
    // ==========================================================================
    void set_wake_button(button::Button *button);
    void set_pair_button(button::Button *button);
    void set_regenerate_key_button(button::Button *button);
    void set_force_update_button(button::Button *button);

    // ==========================================================================
    // Public vehicle actions
    // ==========================================================================
    int wake_vehicle();
    int start_pairing();
    int regenerate_key();
    void force_update();

    // Vehicle control actions
    int set_charging_state(bool charging);
    int set_charging_amps(int amps);
    int set_charging_limit(int limit);
    
    // Closure controls (VCSEC)
    void lock_vehicle();
    void unlock_vehicle();
    void open_trunk();
    void close_trunk();
    void open_frunk();
    void open_charge_port();
    void close_charge_port();
    void unlock_charge_port();
    void unlatch_driver_door();
    
    // HVAC controls (Infotainment)
    void set_climate_on(bool enable);
    void set_climate_temp(float temp);
    void set_climate_keeper(int mode);  // 0=Off, 1=On, 2=Dog, 3=Camp
    void set_bioweapon_mode(bool enable);
    void set_preconditioning_max(bool enable);  // Defrost
    void set_cabin_overheat_protection(int mode);  // 0=Off, 1=On, 2=Fan Only
    void set_cabin_overheat_temp(int level);  // 1=Low (30 C), 2=Medium (35 C), 3=High (40 C)
    // Sends only what changes: the mode and/or (for On) the temperature
    void set_cabin_overheat_choice(int mode, int level);
    void set_low_power_mode(bool enable, switch_::Switch *sw);
    void set_keep_accessory_power(bool enable, switch_::Switch *sw);
    void set_guest_mode(bool enable, switch_::Switch *sw);
    // Scheduled charging ("start charging at"): the switch keeps the car's
    // time; setting the time also enables it. minutes = after midnight.
    void set_scheduled_charging(bool enabled);
    void set_scheduled_charging_time(int minutes);
    // Scheduled departure: every change sends the whole schedule, built from
    // what the car last reported with the one changed value
    void set_scheduled_departure(bool enabled);
    void set_departure_time(int minutes);
    void set_departure_preconditioning(int policy);
    void set_departure_off_peak(int policy);
    void set_off_peak_end_time(int minutes);
    void set_scheduled_departure_switch(switch_::Switch *sw) { departure_switch_ = sw; }
    void set_departure_time_entity(datetime::TimeEntity *t) { departure_time_entity_ = t; }
    void set_off_peak_end_time_entity(datetime::TimeEntity *t) { off_peak_end_entity_ = t; }
    void set_departure_preconditioning_select(select::Select *sel) { departure_precondition_select_ = sel; }
    void set_departure_off_peak_select(select::Select *sel) { departure_off_peak_select_ = sel; }
    void set_steering_wheel_heat(bool enable);
    
    // Vehicle controls (Infotainment)
    void flash_lights();
    void honk_horn();
    void set_sentry_mode(bool enable);
    void vent_windows();
    void close_windows();

    // Media (Infotainment). The player shows playback and volume; artist,
    // title and source go to text sensors (ESPHome's media player carries no
    // metadata), next/previous to buttons (Home Assistant does not offer them
    // for ESPHome media players).
    void set_media_player(media_player::MediaPlayer *player) { media_player_ = player; }
    void media_control(const media_player::MediaPlayerCall &call);
    void media_next_track();
    void media_previous_track();

    // Command tracking sensors
    void set_last_command_text_sensor(text_sensor::TextSensor *sensor) { last_command_sensor_ = sensor; }

    // Manager accessors
    VehicleStateManager* get_state_manager() const { return state_manager_.get(); }
    
    // BLE connection state
    bool is_connected() const;
    void set_ble_client(TeslaBLEClient *client) { ble_client_ = client; }
    TeslaBLEClient *ble_client() const { return ble_client_; }
    uint16_t get_read_handle() const { return read_handle_; }
    uint16_t get_write_handle() const { return write_handle_; }
    void update_ble_rssi(int8_t rssi);
    uint32_t ble_write_gap_ms() const;

    // One-link-at-a-time scheduling across all configured cars.
    bool link_turn_allows_connect() const;
    // BLE link up, notifications registered and the Tesla session layer
    // connected: commands can be sent right now.
    bool link_ready() const;
    void note_link_activity();
    // Called for every advertisement from this car's MAC address.
    void note_advert_seen(int rssi);
    // BLE MAC discovery. Without ble_mac_address the car is found by the
    // advert name derived from its VIN; the MAC is then kept in NVS.
    void set_mac_from_config(bool from_config) { mac_from_config_ = from_config; }
    bool advert_name_matches(const std::string &name) const {
      return !advert_name_.empty() && name == advert_name_;
    }
    void adopt_discovered_address(uint64_t address);

    // Car name for log lines (falls back to the VIN).
    const char *log_name() const { return debug_name_.empty() ? vin_.c_str() : debug_name_.c_str(); }

    // tesla-ble logs through one global callback with no vehicle context.
    // While a LogScope is alive, those lines are prefixed with this car's
    // name. Scopes nest (the previous context is restored).
    static const char *log_context() { return log_context_; }
    class LogScope {
     public:
      explicit LogScope(const TeslaBLEVehicle *vehicle) : previous_(log_context_) {
        log_context_ = vehicle != nullptr ? vehicle->log_name() : nullptr;
      }
      ~LogScope() { log_context_ = previous_; }
      LogScope(const LogScope &) = delete;
      LogScope &operator=(const LogScope &) = delete;

     private:
      const char *previous_;
    };

private:
    // Initialization helpers
    void initialize_managers();
    void configure_pending_sensors();

    // Connection handlers
    void handle_connection_established();
    void handle_connection_lost();

    // Link turn scheduling (see link_scheduler.h). All cars share one
    // scheduler; each car is one slot.
    static std::vector<TeslaBLEVehicle *> link_vehicles_;
    static LinkScheduler link_scheduler_;
    int link_slot_{LinkScheduler::NONE};
    bool ever_ready_{false};
    bool yielding_link_{false};
    bool link_ready_{false};
    bool turn_requested_{false};
    bool ready_this_turn_{false};
    bool infotainment_ever_polled_{false};
    uint32_t last_link_activity_ms_{0};
    // Presence from advertisements: a Tesla advertises all the time while in
    // range (also asleep), and the scanner hears it while the other car is
    // connected. Only a car heard recently gets a turn, so a car that is away
    // never takes the link from the one that is here.
    uint32_t last_advert_ms_{0};
    uint32_t last_advert_log_ms_{0};
    uint32_t last_advert_publish_ms_{0};
    static constexpr uint32_t ADVERT_RSSI_PUBLISH_MS = 10000;
    bool wake_on_boot_{true};
    uint32_t turn_started_ms_{0};
    bool reachable_published_{false};
    bool reachable_known_{false};
    static constexpr uint32_t ADVERT_FRESH_MS = 60000;
    // Present goes to away only after this long without the link or an
    // advert: the scanner misses adverts while the other car has its turn.
    uint32_t presence_timeout_ms_{300000};
    // Safety net if adverts are never reported: still try a turn this often.
    static constexpr uint32_t BLIND_TURN_MS = 600000;
    static constexpr uint32_t ADVERT_LOG_INTERVAL_MS = 60000;
    bool heard_recently_(uint32_t now) const;
    void update_reachable_(uint32_t now);
    // A car that misses its turn (out of range) waits before asking again,
    // so a car that is away does not keep taking the link from the other.
    uint8_t missed_turns_{0};
    uint32_t retry_turn_after_ms_{0};
    static constexpr uint32_t MISSED_TURN_BACKOFF_MS = 30000;
    static constexpr uint32_t MAX_MISSED_TURN_BACKOFF_MS = 300000;
    LinkScheduler::Input link_input_(uint32_t now) const;
    static void run_link_scheduler_(uint32_t now);
    void yield_link_();

    // Commands issued while this car does not hold the link wait here and
    // are sent once it connects. They expire if the car stays unreachable.
    struct PendingCommand {
      uint32_t queued_ms;
      std::string name;
      std::function<void()> run;
      std::function<void()> expire;
    };
    std::deque<PendingCommand> pending_commands_;
    static constexpr size_t MAX_PENDING_COMMANDS = 8;
    static constexpr uint32_t PENDING_COMMAND_TIMEOUT_MS = 120000;
    bool queue_until_connected_(const std::string &name, std::function<void()> run,
                                std::function<void()> expire);
    void flush_pending_commands_();
    void expire_pending_commands_(uint32_t now);

    // Infotainment decision after connecting waits for the VCSEC status, so
    // a reconnect never polls (or wakes) a car whose sleep state is unknown.
    bool infotainment_check_pending_{false};
    static constexpr uint32_t INFOTAINMENT_CHECK_TIMEOUT_MS = 8000;
    void maybe_poll_infotainment_(uint32_t now);
    void register_notify_();
    void schedule_notify_retry_();
    void on_missing_characteristic_(const char *which);

    // Adapters & Managers
    std::shared_ptr<BleAdapterImpl> ble_adapter_;
    std::shared_ptr<StorageAdapterImpl> storage_adapter_;
    std::shared_ptr<::TeslaBLE::Vehicle> vehicle_;
    std::unique_ptr<VehicleStateManager> state_manager_;

    // Configuration
    std::string vin_;
    std::string debug_name_;
    std::string advert_name_;  // "S<16 hex>C", from the VIN
    bool mac_from_config_{false};
    std::string role_;
    
    // Polling intervals
    uint32_t vcsec_poll_interval_{10000};
    
    // Polling state
    uint32_t last_vcsec_poll_{0};
    bool pairing_in_progress_{false};
    uint32_t pairing_started_ms_{0};
    static constexpr uint32_t PAIRING_POLL_PAUSE_MS = 35000;
    static constexpr uint32_t PAIRING_REQUEST_GUARD_MS = 180000;
    InfotainmentPollPolicy poll_policy_;
    ConnectionResetPolicy connection_reset_policy_;

    // Sequential infotainment polling. TeslaBLE::Vehicle::infotainment_poll()
    // starts five logical commands at once; here they are queued and sent one
    // after another, so a user command can go ahead of the remaining polls.
    struct InfotainmentWorkItem {
      std::function<void()> start;
      bool interactive;
      // User commands: re-issues the command if it is dropped before it ran.
      std::function<void()> requeue;
    };

    // This car's infotainment work: one command at a time (the library sends
    // one and waits for its answer anyway), user commands ahead of
    // background polls. Only one car is connected at a time, so there is no
    // cross-car coordination here.
    std::deque<InfotainmentWorkItem> infotainment_queue_;
    bool infotainment_busy_{false};
    void start_next_infotainment_work_();

    uint8_t user_commands_in_flight_{0};
    void enqueue_poll_batch_(TeslaBLE::WakePolicy policy, uint32_t delay_ms = 0);
    void enqueue_poll_job_(const char *name, int32_t data_type, TeslaBLE::WakePolicy policy);
    void complete_poll_batch_job_();
    uint8_t poll_batch_start_{0};
    bool poll_batch_in_progress_{false};
    uint8_t poll_batch_remaining_{0};
    void enqueue_infotainment_work_(std::function<void()> start, bool interactive,
                                    std::function<void()> requeue = nullptr);
    // send_command_with_tracking with a retry budget: a user command that
    // fails because the link or session was reset is sent again once.
    void send_command_tracked_(UniversalMessage_Domain domain, const std::string &name,
                               std::function<int(TeslaBLE::Client *, uint8_t *, size_t *)> builder,
                               TeslaBLE::WakePolicy wake_policy, std::function<void(bool)> on_result,
                               uint8_t retries_left, bool woken = false);
    // Infotainment needs a moment after VCSEC reports the car awake; a session
    // request sent at once is often lost (and the library then waits 25 s).
    static constexpr uint32_t WAKE_SETTLE_MS = 8000;
    // After a user command the car keeps its BLE turn this long, so the
    // follow-up reads (e.g. lock state 1.5 s / 8 s later) run before hand-over.
    static constexpr uint32_t USER_COMMAND_HOLD_MS = 10000;
    uint32_t last_user_command_ms_{0};
    bool user_command_seen_{false};
    bool should_retry_command_(const std::string &name, const TeslaBLE::OperationResult &result,
                               uint8_t retries_left);
    void release_infotainment_slot_();
    void defer_release_infotainment_slot_();
    void cancel_queued_infotainment_work_();

    TeslaBLEClient *ble_client_{nullptr};
    static const char *log_context_;

    // BLE state
    espbt::ESPBTUUID service_uuid_;
    espbt::ESPBTUUID read_uuid_;
    espbt::ESPBTUUID write_uuid_;
    uint16_t read_handle_{0};
    uint16_t write_handle_{0};
    bool notify_ready_{false};
    bool notify_registration_pending_{false};
    static constexpr uint32_t NOTIFY_RETRY_MS = 500;
    uint32_t last_rssi_request_{0};
    static constexpr uint32_t RSSI_POLL_INTERVAL_MS = 10000;

    // ==========================================================================
    // Pending sensors (stored before state manager is initialized)
    // ==========================================================================
    std::map<std::string, binary_sensor::BinarySensor*> pending_binary_sensors_;
    std::map<std::string, sensor::Sensor*> pending_sensors_;
    std::map<std::string, text_sensor::TextSensor*> pending_text_sensors_;
    
    // Pending switches
    switch_::Switch *pending_charging_switch_{nullptr};
    switch_::Switch *pending_steering_wheel_heat_switch_{nullptr};
    switch_::Switch *pending_sentry_mode_switch_{nullptr};
    
    // Pending numbers
    number::Number *pending_charging_amps_number_{nullptr};
    number::Number *pending_charging_limit_number_{nullptr};
    select::Select *pending_cabin_overheat_select_{nullptr};
    void send_assumed_switch_(const char *name_on, const char *name_off, int32_t action_tag, bool enable,
                              switch_::Switch *sw);
    switch_::Switch *pending_scheduled_charging_switch_{nullptr};
    datetime::TimeEntity *pending_scheduled_charging_time_{nullptr};
    void send_scheduled_charging_(bool enabled, int minutes);
    // Scheduled departure, as last reported by the car (-1 = unknown)
    struct Departure {
      int enabled{-1};
      int time{-1};
      int preconditioning{-1};
      int off_peak{-1};
      int off_peak_end{-1};
    } departure_;
    void send_departure_(Departure d);
    void publish_departure_();
    switch_::Switch *departure_switch_{nullptr};
    datetime::TimeEntity *departure_time_entity_{nullptr};
    datetime::TimeEntity *off_peak_end_entity_{nullptr};
    select::Select *departure_precondition_select_{nullptr};
    select::Select *departure_off_peak_select_{nullptr};

    // Media player, as last reported by the car
    media_player::MediaPlayer *media_player_{nullptr};
    float media_volume_max_{state_text::kMediaVolumeLimit};
    void handle_media_state_(const CarServer_MediaState &media, const TeslaBLE::MediaNowPlaying &now_playing);
    void publish_media_off_();
    void send_media_command_(const char *name, int32_t action_tag, CarServer_MediaUpdateVolume volume);
    void send_media_command_(const char *name, int32_t action_tag);

    // Pending locks
    lock::Lock *pending_doors_lock_{nullptr};
    lock::Lock *pending_charge_port_latch_lock_{nullptr};
    
    // Pending covers
    cover::Cover *pending_trunk_cover_{nullptr};
    cover::Cover *pending_frunk_cover_{nullptr};
    cover::Cover *pending_windows_cover_{nullptr};
    cover::Cover *pending_charge_port_door_cover_{nullptr};
    
    // Pending climate
    climate::Climate *pending_climate_{nullptr};
    
    std::string last_rx_hex_;

    // Fallback until the vehicle reports its maximum.
    int configured_charging_amps_max_{DEFAULT_CHARGING_AMPS_MAX};

    uint32_t charging_amps_max_pref_hash_() const;
    uint32_t ble_mac_pref_hash_() const;
    void restore_ble_mac_();
    void restore_charging_amps_max_();
    void save_charging_amps_max_(int max);
    uint32_t charge_session_pref_hash_() const;
    void restore_charge_session_();
    void save_charge_session_(int32_t phases, float power_kw);

    // Command tracking
    void handle_command_result(const std::string &name, TeslaBLE::OperationResult result);
    void schedule_state_refresh_(ControlStateRefresh refresh);
    void settle_lock_(bool charge_port, bool succeeded);
    void send_command_with_tracking(
        UniversalMessage_Domain domain,
        const std::string &name,
        std::function<int(TeslaBLE::Client *, uint8_t *, size_t *)> builder,
        TeslaBLE::WakePolicy wake_policy = TeslaBLE::WakePolicy::WAKE_IF_NEEDED,
        std::function<void(bool)> on_result = nullptr);

    text_sensor::TextSensor *last_command_sensor_{nullptr};

    // Friends
    friend class VehicleStateManager;
};

class TeslaBLEClient : public esp32_ble_client::BLEClientBase {
 public:
  void set_vehicle(TeslaBLEVehicle *vehicle) { vehicle_ = vehicle; }

  // Link parameters in BLE units: interval in 1.25 ms steps, supervision
  // timeout in 10 ms steps. Applied as the preferred parameters before
  // connecting and requested again once service discovery is done.
  void set_link_params(uint16_t interval_units, uint16_t timeout_units) {
    link_interval_units_ = interval_units;
    link_timeout_units_ = timeout_units;
  }

  // Reads the parameters the link actually runs with and logs them when they
  // change. ESPHome does not forward ESP_GAP_BLE_UPDATE_CONN_PARAMS_EVT to
  // components, so this polls esp_ble_get_current_conn_params() instead.
  void log_link_params_if_changed(const char *name);

#ifdef USE_ESP32_BLE_DEVICE
  // Records every advert from this car (presence), then lets only the car
  // whose turn it is start a connection.
  bool parse_device(const espbt::ESPBTDevice &device) override;
#endif
  // Keeps the configured MAC: ESPHome may clear address_ on some paths.
  void set_address(uint64_t address) override {
    if (address != 0) tesla_address_ = address;
    esp32_ble_client::BLEClientBase::set_address(address);
  }
  void connect() override;
  bool gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                           esp_ble_gattc_cb_param_t *param) override;
  void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) override;

 protected:
  TeslaBLEVehicle *vehicle_{nullptr};
  uint64_t tesla_address_{0};
  uint16_t link_interval_units_{12};  // 15 ms
  uint16_t link_timeout_units_{600};  // 6 s
  // Last logged values; 0 means not logged on this connection yet.
  uint16_t logged_interval_{0};
  uint16_t logged_latency_{0};
  uint16_t logged_timeout_{0};
};

// =============================================================================
// Generic Tesla Button - use DEFINE_TESLA_BUTTON macro for each button type
// =============================================================================

template<typename T> class WithParent : public T {
 public:
  void set_parent(TeslaBLEVehicle *parent) { parent_ = parent; }
 protected:
  TeslaBLEVehicle *parent_{nullptr};
};

using TeslaButtonBase = WithParent<button::Button>;

// Macro to define a Tesla button that calls a specific parent method
#define DEFINE_TESLA_BUTTON(ClassName, ParentMethod) \
    class ClassName : public TeslaButtonBase { \
    protected: \
        void press_action() override { if (parent_) parent_->ParentMethod(); } \
    };

// Define all button types using the macro
DEFINE_TESLA_BUTTON(TeslaWakeButton, wake_vehicle)
DEFINE_TESLA_BUTTON(TeslaPairButton, start_pairing)
DEFINE_TESLA_BUTTON(TeslaRegenerateKeyButton, regenerate_key)
DEFINE_TESLA_BUTTON(TeslaForceUpdateButton, force_update)
DEFINE_TESLA_BUTTON(TeslaFlashLightsButton, flash_lights)
DEFINE_TESLA_BUTTON(TeslaHonkHornButton, honk_horn)
DEFINE_TESLA_BUTTON(TeslaUnlatchDriverDoorButton, unlatch_driver_door)
DEFINE_TESLA_BUTTON(TeslaReleaseChargeCableButton, unlock_charge_port)
DEFINE_TESLA_BUTTON(TeslaMediaNextTrackButton, media_next_track)
DEFINE_TESLA_BUTTON(TeslaMediaPreviousTrackButton, media_previous_track)

// =============================================================================
// Generic Tesla Switch - use DEFINE_TESLA_SWITCH macro for each switch type
// =============================================================================

using TeslaSwitchBase = WithParent<switch_::Switch>;

// Macro to define a Tesla switch that calls a specific parent method with bool state.
#define DEFINE_TESLA_SWITCH(ClassName, ParentMethod) \
    class ClassName : public TeslaSwitchBase { \
    protected: \
        void write_state(bool state) override { \
            if (parent_) parent_->ParentMethod(state); \
        } \
    };

// Define all switch types using the macro
DEFINE_TESLA_SWITCH(TeslaChargingSwitch, set_charging_state)
DEFINE_TESLA_SWITCH(TeslaSteeringWheelHeatSwitch, set_steering_wheel_heat)
DEFINE_TESLA_SWITCH(TeslaSentryModeSwitch, set_sentry_mode)
DEFINE_TESLA_SWITCH(TeslaScheduledChargingSwitch, set_scheduled_charging)
DEFINE_TESLA_SWITCH(TeslaScheduledDepartureSwitch, set_scheduled_departure)

// Modes the car does not report over BLE: the switch shows the last state
// that was set successfully (assumed state, both buttons shown).
#define DEFINE_TESLA_ASSUMED_SWITCH(ClassName, ParentMethod) \
    class ClassName : public TeslaSwitchBase { \
    public: \
        bool assumed_state() override { return true; } \
    protected: \
        void write_state(bool state) override { \
            if (parent_) parent_->ParentMethod(state, this); \
        } \
    };

DEFINE_TESLA_ASSUMED_SWITCH(TeslaLowPowerModeSwitch, set_low_power_mode)
DEFINE_TESLA_ASSUMED_SWITCH(TeslaKeepAccessoryPowerSwitch, set_keep_accessory_power)
DEFINE_TESLA_ASSUMED_SWITCH(TeslaGuestModeSwitch, set_guest_mode)

// "Start charging at" time; read back from the car's charge state
class TeslaScheduledChargingTime : public WithParent<datetime::TimeEntity> {
public:
    void update_time(int minutes) {
        this->hour_ = minutes / 60;
        this->minute_ = minutes % 60;
        this->second_ = 0;
        this->publish_state();
    }
protected:
    void control(const datetime::TimeCall &call) override {
        if (!parent_) return;
        const int hour = call.get_hour().value_or(this->hour_);
        const int minute = call.get_minute().value_or(this->minute_);
        parent_->set_scheduled_charging_time(hour * 60 + minute);
    }
};

// Time pickers that call a parent method with minutes after midnight
#define DEFINE_TESLA_TIME(ClassName, ParentMethod) \
    class ClassName : public WithParent<datetime::TimeEntity> { \
    public: \
        void update_time(int minutes) { \
            this->hour_ = minutes / 60; \
            this->minute_ = minutes % 60; \
            this->second_ = 0; \
            this->publish_state(); \
        } \
    protected: \
        void control(const datetime::TimeCall &call) override { \
            if (!parent_) return; \
            const int hour = call.get_hour().value_or(this->hour_); \
            const int minute = call.get_minute().value_or(this->minute_); \
            parent_->ParentMethod(hour * 60 + minute); \
        } \
    };

DEFINE_TESLA_TIME(TeslaDepartureTime, set_departure_time)
DEFINE_TESLA_TIME(TeslaOffPeakEndTime, set_off_peak_end_time)

// Off / All Week / Weekdays
#define DEFINE_TESLA_POLICY_SELECT(ClassName, ParentMethod) \
    class ClassName : public WithParent<select::Select> { \
    protected: \
        void control(const std::string &value) override { \
            if (!parent_) return; \
            auto policy = state_text::departure_policy(value); \
            if (policy.has_value()) parent_->ParentMethod(*policy); \
        } \
    };

DEFINE_TESLA_POLICY_SELECT(TeslaDeparturePreconditioningSelect, set_departure_preconditioning)
DEFINE_TESLA_POLICY_SELECT(TeslaDepartureOffPeakSelect, set_departure_off_peak)

class TeslaChargingAmpsNumber : public WithParent<number::Number> {
protected:
    void control(float value) override;
};

class TeslaChargingLimitNumber : public WithParent<number::Number> {
protected:
    void control(float value) override;
};


// Off / Fan Only / On 30 °C / On 35 °C / On 40 °C
class TeslaCabinOverheatSelect : public WithParent<select::Select> {
protected:
    void control(const std::string &value) override {
        if (!parent_) return;
        auto choice = state_text::cop_choice(value);
        if (choice.has_value()) parent_->set_cabin_overheat_choice(choice->mode, choice->level);
    }
};

// =============================================================================
// Lock classes - combined sensor + control for doors and charge port
// =============================================================================

using TeslaLockBase = WithParent<lock::Lock>;

class TeslaDoorsLock : public TeslaLockBase {
protected:
    void control(const lock::LockCall &call) override;
};

class TeslaChargePortLatchLock : public TeslaLockBase {
protected:
    void control(const lock::LockCall &call) override;
};

// =============================================================================
// Cover classes - combined sensor + control for trunk, frunk, windows
// =============================================================================

class TeslaCoverBase : public WithParent<cover::Cover> {
 public:
  cover::CoverTraits get_traits() override;
};

class TeslaTrunkCover : public TeslaCoverBase {
protected:
    void control(const cover::CoverCall &call) override;
};

class TeslaFrunkCover : public TeslaCoverBase {
protected:
    void control(const cover::CoverCall &call) override;
};

class TeslaWindowsCover : public TeslaCoverBase {
protected:
    void control(const cover::CoverCall &call) override;
};

class TeslaChargePortDoorCover : public TeslaCoverBase {
protected:
    void control(const cover::CoverCall &call) override;
};

// =============================================================================
// Climate class - HVAC control with temperature
// =============================================================================

class TeslaClimate : public WithParent<climate::Climate> {
public:
    TeslaClimate();
    climate::ClimateTraits traits() override;
    void control(const climate::ClimateCall &call) override;
    
    // Called by state manager to update current state. preset / fan_mode:
    // nullptr leaves the current value unchanged.
    void update_state(bool is_on, float current_temp, float target_temp,
                      const char *preset = nullptr, const char *fan_mode = nullptr);
};

// =============================================================================
// Media player - playback and volume of the car's media
// =============================================================================

class TeslaMediaPlayer : public WithParent<media_player::MediaPlayer> {
public:
    media_player::MediaPlayerTraits get_traits() override {
        media_player::MediaPlayerTraits traits;
        // Not URL playback, browsing, stop, mute or announcements (the
        // defaults); the car does play/pause and volume. No next/previous:
        // Home Assistant shows the arrows but its ESPHome integration has no
        // handler for them (they fail) - the track buttons do that instead.
        traits.clear_feature_flags(media_player::BASE_MEDIA_PLAYER_FEATURES);
        traits.add_feature_flags(media_player::MediaPlayerEntityFeature::PAUSE |
                                 media_player::MediaPlayerEntityFeature::PLAY |
                                 media_player::MediaPlayerEntityFeature::VOLUME_SET |
                                 media_player::MediaPlayerEntityFeature::VOLUME_STEP);
        return traits;
    }

protected:
    void control(const media_player::MediaPlayerCall &call) override {
        if (parent_) parent_->media_control(call);
    }
};

// =============================================================================
// Action classes for automation
// =============================================================================

template<typename... Ts> class WakeAction : public Action<Ts...> {
public:
    WakeAction(TeslaBLEVehicle *parent) : parent_(parent) {}
    void play(Ts... x) override { parent_->wake_vehicle(); }
protected:
    TeslaBLEVehicle *parent_;
};

template<typename... Ts> class PairAction : public Action<Ts...> {
public:
    PairAction(TeslaBLEVehicle *parent) : parent_(parent) {}
    void play(Ts... x) override { parent_->start_pairing(); }
protected:
    TeslaBLEVehicle *parent_;
};

template<typename... Ts> class RegenerateKeyAction : public Action<Ts...> {
public:
    RegenerateKeyAction(TeslaBLEVehicle *parent) : parent_(parent) {}
    void play(Ts... x) override { parent_->regenerate_key(); }
protected:
    TeslaBLEVehicle *parent_;
};

template<typename... Ts> class ForceUpdateAction : public Action<Ts...> {
public:
    ForceUpdateAction(TeslaBLEVehicle *parent) : parent_(parent) {}
    void play(Ts... x) override { parent_->force_update(); }
protected:
    TeslaBLEVehicle *parent_;
};

template<typename... Ts> class SetChargingAction : public Action<Ts...> {
public:
    SetChargingAction(TeslaBLEVehicle *parent) : parent_(parent) {}
    void set_state(esphome::TemplatableValue<bool, Ts...> state) { state_ = state; }
    void play(Ts... x) override {
        bool state = state_.value(x...);
        parent_->set_charging_state(state);
    }
protected:
    TeslaBLEVehicle *parent_;
    esphome::TemplatableValue<bool, Ts...> state_;
};

template<typename... Ts> class SetChargingAmpsAction : public Action<Ts...> {
public:
    SetChargingAmpsAction(TeslaBLEVehicle *parent) : parent_(parent) {}
    void set_amps(esphome::TemplatableValue<int, Ts...> amps) { amps_ = amps; }
    void play(Ts... x) override {
        int amps = amps_.value(x...);
        parent_->set_charging_amps(amps);
    }
protected:
    TeslaBLEVehicle *parent_;
    esphome::TemplatableValue<int, Ts...> amps_;
};

template<typename... Ts> class SetChargingLimitAction : public Action<Ts...> {
public:
    SetChargingLimitAction(TeslaBLEVehicle *parent) : parent_(parent) {}
    void set_limit(esphome::TemplatableValue<int, Ts...> limit) { limit_ = limit; }
    void play(Ts... x) override {
        int limit = limit_.value(x...);
        parent_->set_charging_limit(limit);
    }
protected:
    TeslaBLEVehicle *parent_;
    esphome::TemplatableValue<int, Ts...> limit_;
};

} // namespace tesla_ble_vehicle
} // namespace esphome
