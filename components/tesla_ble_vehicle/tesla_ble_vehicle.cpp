#include "tesla_ble_vehicle.h"
#include "command_warning_policy.h"
#include <client.h>
#include <cinttypes>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <defs.h>
#include <esp_log.h>
#include <esphome/core/helpers.h>
#include <tb_utils.h>
#include <vin_utils.h>

// The guest mode / overheat temperature / low power / scheduled departure /
// media commands need the darek-margas tesla-ble fork v5.2.0-dm.3 or newer. With an older library the
// messages still compile (same protocol definitions) but the library refuses
// to build them at run time ("Unsupported vehicle action type"), so fail the
// build instead. Fix: set the tesla-ble ref in your YAML to v5.2.0-dm.3.
static_assert(std::is_member_function_pointer<decltype(&TeslaBLE::Vehicle::set_media_state_callback)>::value,
              "tesla-ble library too old: use ref v5.2.0-dm.3 (darek-margas fork) or newer");

namespace esphome {
namespace tesla_ble_vehicle {

const char *TeslaBLEVehicle::log_context_ = nullptr;
std::vector<TeslaBLEVehicle *> TeslaBLEVehicle::link_vehicles_;
LinkScheduler TeslaBLEVehicle::link_scheduler_;
bool TeslaBLEVehicle::discovery_forced_active_scan_ = false;
uint32_t TeslaBLEVehicle::discovery_checked_ms_ = 0;

void tesla_ble_log_callback(TeslaBLE::LogLevel level, const char *tag, int line,
                            const char *format, va_list args) {
  if (tag == nullptr)
    tag = "TeslaBLE";
  if (format == nullptr)
    return;

  int esphome_level;
  switch (level) {
  case TeslaBLE::LogLevel::ERROR:
    esphome_level = ESPHOME_LOG_LEVEL_ERROR;
    break;
  case TeslaBLE::LogLevel::WARN:
    esphome_level = ESPHOME_LOG_LEVEL_WARN;
    break;
  case TeslaBLE::LogLevel::INFO:
    esphome_level = ESPHOME_LOG_LEVEL_INFO;
    break;
  case TeslaBLE::LogLevel::DEBUG:
    esphome_level = ESPHOME_LOG_LEVEL_DEBUG;
    break;
  case TeslaBLE::LogLevel::VERBOSE:
    esphome_level = ESPHOME_LOG_LEVEL_VERBOSE;
    break;
  default:
    return;
  }
  const char *car = TeslaBLEVehicle::log_context();
  if (car == nullptr) {
    esp_log_vprintf_(esphome_level, tag, line, format, args);
    return;
  }
  // Prefix library lines with the car they belong to.
  char message[512];
  vsnprintf(message, sizeof(message), format, args);
  esp_log_printf_(esphome_level, tag, line, "[%s] %s", car, message);
}

TeslaBLEVehicle::TeslaBLEVehicle() : vin_(""), role_("DRIVER") {
  ESP_LOGCONFIG(TAG, "Constructing Tesla BLE Vehicle component");
}

void TeslaBLEVehicle::setup() {
  ESP_LOGCONFIG(TAG, "Setting up TeslaBLEVehicle");
  service_uuid_ = espbt::ESPBTUUID::from_raw(SERVICE_UUID);
  read_uuid_ = espbt::ESPBTUUID::from_raw(READ_UUID);
  write_uuid_ = espbt::ESPBTUUID::from_raw(WRITE_UUID);
  initialize_managers();
  restore_charging_amps_max_();
  configure_pending_sensors();
  restore_charge_session_();

  if (vin_.empty()) {
    ESP_LOGE(TAG, "VIN not configured - component will not function properly");
    this->status_set_warning("VIN not configured");
    return;
  }

  vehicle_->set_vin(vin_);

  advert_name_ = TeslaBLE::get_vin_advertisement_name(vin_);
  restore_ble_mac_();

  link_slot_ = static_cast<int>(link_vehicles_.size());
  link_vehicles_.push_back(this);
  link_scheduler_.set_count(link_vehicles_.size());
}

void TeslaBLEVehicle::initialize_managers() {
  ESP_LOGD(TAG, "Initializing components...");

  ble_adapter_ = std::make_shared<BleAdapterImpl>(this);
  // NVS namespace is per vehicle for sessions. Keep it <= 15 chars.
  // FNV-1a over the VIN gives a stable compact namespace.
  uint32_t vin_hash = 2166136261u;
  for (char ch : vin_) {
    vin_hash ^= static_cast<uint8_t>(ch);
    vin_hash *= 16777619u;
  }
  char storage_namespace[16];
  snprintf(storage_namespace, sizeof(storage_namespace), "ts%08x", (unsigned) vin_hash);
  storage_adapter_ = std::make_shared<StorageAdapterImpl>(storage_namespace);

  if (!storage_adapter_->initialize()) {
    ESP_LOGE(TAG, "Failed to initialize storage adapter");
  }

  TeslaBLE::set_log_callback(tesla_ble_log_callback);
  vehicle_ =
      std::make_shared<TeslaBLE::Vehicle>(ble_adapter_, storage_adapter_);
  state_manager_ = std::make_unique<VehicleStateManager>(this);

  ESP_LOGD(TAG, "Wiring up callbacks...");

  vehicle_->set_raw_message_callback([this](const std::vector<uint8_t> &data) {
    std::string hex = TeslaBLE::format_hex(data.data(), data.size());
    if (hex != last_rx_hex_) {
      ESP_LOGV(TAG, "BLE RX: %s", hex.c_str());
      last_rx_hex_ = hex;
    }
  });

  vehicle_->set_vehicle_status_callback([this](const VCSEC_VehicleStatus &s) {
    if (state_manager_)
      state_manager_->update_vehicle_status(s);
    if (state_manager_ && state_manager_->is_asleep())
      publish_media_off_();
    // First VCSEC status after connecting: the sleep state is known now, so
    // the infotainment decision cannot wake a sleeping car by accident.
    if (infotainment_check_pending_) {
      infotainment_check_pending_ = false;
      this->cancel_timeout("infotainment-check");
      maybe_poll_infotainment_(millis());
    }
  });

  vehicle_->set_charge_state_callback([this](const CarServer_ChargeState &s) {
    if (state_manager_)
      state_manager_->update_charge_state(s);
  });

  vehicle_->set_climate_state_callback([this](const CarServer_ClimateState &s) {
    if (state_manager_)
      state_manager_->update_climate_state(s);
  });

  vehicle_->set_drive_state_callback([this](const CarServer_DriveState &s) {
    if (state_manager_)
      state_manager_->update_drive_state(s);
  });

  vehicle_->set_tire_pressure_state_callback(
      [this](const CarServer_TirePressureState &s) {
        if (state_manager_)
          state_manager_->update_tire_pressure_state(s);
      });

  vehicle_->set_closures_state_callback(
      [this](const CarServer_ClosuresState &s) {
        if (state_manager_)
          state_manager_->update_closures_state(s);
      });

  vehicle_->set_media_state_callback(
      [this](const CarServer_MediaState &s, const TeslaBLE::MediaNowPlaying &now_playing) {
        handle_media_state_(s, now_playing);
      });

  ESP_LOGD(TAG, "All components initialized");
}

void TeslaBLEVehicle::configure_pending_sensors() {
  if (!state_manager_) {
    ESP_LOGE(TAG, "State manager not available");
    return;
  }

  for (const auto &pair : pending_binary_sensors_)
    state_manager_->set_binary_sensor(pair.first, pair.second);
  for (const auto &pair : pending_sensors_)
    state_manager_->set_sensor(pair.first, pair.second);
  for (const auto &pair : pending_text_sensors_)
    state_manager_->set_text_sensor(pair.first, pair.second);

  if (pending_charging_switch_)
    state_manager_->set_charging_switch(pending_charging_switch_);
  if (pending_sentry_mode_switch_)
    state_manager_->set_sentry_mode_switch(pending_sentry_mode_switch_);
  if (pending_steering_wheel_heat_switch_)
    state_manager_->set_steering_wheel_heat_switch(
        pending_steering_wheel_heat_switch_);
  if (pending_charging_amps_number_)
    state_manager_->set_charging_amps_number(pending_charging_amps_number_);
  if (pending_charging_limit_number_)
    state_manager_->set_charging_limit_number(pending_charging_limit_number_);
  if (pending_cabin_overheat_select_)
    state_manager_->set_cabin_overheat_select(pending_cabin_overheat_select_);
  if (pending_scheduled_charging_switch_)
    state_manager_->set_scheduled_charging_switch(pending_scheduled_charging_switch_);
  if (pending_scheduled_charging_time_)
    state_manager_->set_scheduled_charging_time(pending_scheduled_charging_time_);
  if (pending_doors_lock_)
    state_manager_->set_doors_lock(pending_doors_lock_);
  if (pending_charge_port_latch_lock_)
    state_manager_->set_charge_port_latch_lock(pending_charge_port_latch_lock_);
  if (pending_trunk_cover_)
    state_manager_->set_trunk_cover(pending_trunk_cover_);
  if (pending_frunk_cover_)
    state_manager_->set_frunk_cover(pending_frunk_cover_);
  if (pending_windows_cover_)
    state_manager_->set_windows_cover(pending_windows_cover_);
  if (pending_charge_port_door_cover_)
    state_manager_->set_charge_port_door_cover(pending_charge_port_door_cover_);
  if (pending_climate_)
    state_manager_->set_climate(pending_climate_);
  // Off until the car reports its media state (it only does while awake)
  if (media_player_ != nullptr) {
    media_player_->state = media_player::MEDIA_PLAYER_STATE_OFF;
    media_player_->publish_state();
  }

  // Lock and cover changes logged with the car name: the [S] state lines
  // in the ESPHome log viewer only carry the entity name.
  for (lock::Lock *lck : {pending_doors_lock_, pending_charge_port_latch_lock_}) {
    if (lck == nullptr) continue;
    auto last = std::make_shared<lock::LockState>(lock::LOCK_STATE_NONE);
    lck->add_on_state_callback([this, lck, last](lock::LockState state) {
      if (state == *last) return;
      *last = state;
      ESP_LOGI(TAG, "[%s] %s: %s", log_name(), lck->get_name().c_str(), LOG_STR_ARG(lock::lock_state_to_string(state)));
    });
  }
  for (cover::Cover *cvr : {pending_trunk_cover_, pending_frunk_cover_, pending_windows_cover_,
                            pending_charge_port_door_cover_}) {
    if (cvr == nullptr) continue;
    auto last = std::make_shared<float>(-1.0f);  // log changes only (some covers republish every poll)
    cvr->add_on_state_callback([this, cvr, last]() {
      if (cvr->position == *last) return;
      *last = cvr->position;
      ESP_LOGI(TAG, "[%s] %s: %s", log_name(), cvr->get_name().c_str(),
               cvr->position == cover::COVER_OPEN ? "OPEN" : "CLOSED");
    });
  }

  ESP_LOGD(TAG, "Configured %d binary, %d numeric, %d text sensors",
           pending_binary_sensors_.size(), pending_sensors_.size(),
           pending_text_sensors_.size());
}

void TeslaBLEVehicle::loop() {
  LogScope log_scope(this);
  if (vehicle_)
    vehicle_->loop();
  if (link_slot_ == 0)
    update_discovery_(millis());
  if (ble_adapter_)
    ble_adapter_->process_write_queue();

  if (is_connected() && ble_client_ != nullptr) {
    const uint32_t now = millis();
    if (now - last_rssi_request_ >= RSSI_POLL_INTERVAL_MS) {
      last_rssi_request_ = now;
      esp_ble_gap_read_rssi(ble_client_->get_remote_bda());
      ble_client_->log_link_params_if_changed(log_name());
    }
  }

  // Detect wedged links. Two flavours:
  //  - GATT established but the Vehicle reports disconnected (e.g. after the
  //    library's auth-stuck watchdog reset session state)
  //  - setup stalled in CONNECTED: service discovery or notify registration
  //    failed (gattc_event_handler logs-and-breaks), so ESTABLISHED is never
  //    reached and nothing else ever retries
  // Only a fresh connect cycle re-runs discovery / notify registration, so
  // force one instead of waiting for a reboot.
  const bool gatt_established = is_connected();
  const bool stalled_setup = ble_client_ != nullptr &&
                             ble_client_->state() == espbt::ClientState::CONNECTED;
  const bool vehicle_connected = link_ready_ && vehicle_ != nullptr && vehicle_->is_connected();
  if (connection_reset_policy_.should_force_reconnect(millis(), gatt_established || stalled_setup,
                                                      vehicle_connected)) {
    ESP_LOGW(TAG, "[%s] GATT connection up but vehicle is disconnected - forcing reconnect", log_name());
    connection_reset_policy_.on_force_reconnect(millis());
    if (ble_client_ != nullptr) ble_client_->disconnect();
  }

  expire_pending_commands_(millis());
  update_reachable_(millis());
  // One scheduler for all cars; the first car drives it.
  if (link_slot_ == 0)
    run_link_scheduler_(millis());
}

// =============================================================================
// One BLE link at a time (see link_scheduler.h)
// =============================================================================

void TeslaBLEVehicle::note_link_activity() { last_link_activity_ms_ = millis(); }

bool TeslaBLEVehicle::link_ready() const {
  return link_ready_ && is_connected() && notify_ready_ && vehicle_ != nullptr && vehicle_->is_connected();
}

bool TeslaBLEVehicle::link_turn_allows_connect() const {
  if (link_slot_ == LinkScheduler::NONE)
    return false;
  return link_scheduler_.may_connect(link_slot_);
}

LinkScheduler::Input TeslaBLEVehicle::link_input_(uint32_t now) const {
  LinkScheduler::Input in;
  const bool vcsec_due = now - last_vcsec_poll_ >= vcsec_poll_interval_;
  const bool backing_off = static_cast<int32_t>(now - retry_turn_after_ms_) < 0;
  in.ready = link_ready();
  // Only a car that is actually here may ask for the link. The blind retry
  // is a safety net in case adverts are not reported for some reason.
  const bool present = in.ready || heard_recently_(now);
  const bool blind_retry = now - turn_started_ms_ >= BLIND_TURN_MS;
  in.wants = has_ble_address() && (present || blind_retry) &&
             (!pending_commands_.empty() || turn_requested_ ||
              (!backing_off && (!ever_ready_ || vcsec_due)));
  const bool pairing = pairing_in_progress_ &&
                       static_cast<uint32_t>(now - pairing_started_ms_) < PAIRING_POLL_PAUSE_MS;
  in.busy = user_commands_in_flight_ > 0 || poll_batch_in_progress_ || infotainment_check_pending_ ||
            !pending_commands_.empty() || pairing || (ble_adapter_ && !ble_adapter_->tx_idle()) ||
            (user_command_seen_ && now - last_user_command_ms_ < USER_COMMAND_HOLD_MS);
  const auto state = ble_client_ != nullptr ? ble_client_->state() : espbt::ClientState::IDLE;
  in.link_idle = state == espbt::ClientState::IDLE || state == espbt::ClientState::INIT;
  in.last_activity_ms = last_link_activity_ms_;
  return in;
}

void TeslaBLEVehicle::run_link_scheduler_(uint32_t now) {
  std::vector<LinkScheduler::Input> inputs;
  inputs.reserve(link_vehicles_.size());
  for (auto *v : link_vehicles_)
    inputs.push_back(v->link_input_(now));

  const int previous_owner = link_scheduler_.owner();
  const int released = link_scheduler_.tick(now, inputs);
  if (released != LinkScheduler::NONE)
    link_vehicles_[released]->yield_link_();

  const int owner = link_scheduler_.owner();
  if (owner != LinkScheduler::NONE && owner != previous_owner) {
    link_vehicles_[owner]->turn_started_ms_ = now;
    if (link_vehicles_.size() > 1)
      ESP_LOGI(TAG, "[%s] BLE turn starts", link_vehicles_[owner]->log_name());
  }
}

void TeslaBLEVehicle::note_advert_seen(int rssi) {
  const uint32_t now = millis();
  last_advert_ms_ = now == 0 ? 1 : now;
  if (state_manager_ &&
      (last_advert_publish_ms_ == 0 || now - last_advert_publish_ms_ >= ADVERT_RSSI_PUBLISH_MS)) {
    last_advert_publish_ms_ = now == 0 ? 1 : now;
    state_manager_->update_ble_advert_rssi(static_cast<float>(rssi));
  }
  if (last_advert_log_ms_ == 0 || now - last_advert_log_ms_ >= ADVERT_LOG_INTERVAL_MS) {
    last_advert_log_ms_ = now == 0 ? 1 : now;
    ESP_LOGD(TAG, "[%s] Advert seen (RSSI %d dBm)", log_name(), rssi);
  }
}

bool TeslaBLEVehicle::heard_recently_(uint32_t now) const {
  return heard_within(now, last_advert_ms_, ADVERT_FRESH_MS);
}

void TeslaBLEVehicle::update_reachable_(uint32_t now) {
  // Advert RSSI goes unknown on the short window, independent of Present
  if (last_advert_publish_ms_ != 0 && !heard_recently_(now) && state_manager_) {
    state_manager_->update_ble_advert_rssi(NAN);
    last_advert_publish_ms_ = 0;
  }

  const bool reachable = is_connected() || heard_within(now, last_advert_ms_, presence_timeout_ms_);
  if (reachable_known_ && reachable == reachable_published_)
    return;
  // After boot the scanner needs a moment to hear the car: report away only
  // once it had the chance, so a reboot does not log away -> home
  if (!reachable_known_ && !reachable && now < ADVERT_FRESH_MS)
    return;
  if (reachable_known_) {
    if (reachable) {
      ESP_LOGI(TAG, "[%s] Present (BLE heard)", log_name());
    } else {
      ESP_LOGI(TAG, "[%s] Not heard for %u s - not present", log_name(),
               (unsigned) (presence_timeout_ms_ / 1000));
    }
  }
  reachable_known_ = true;
  reachable_published_ = reachable;
  if (state_manager_)
    state_manager_->update_present(reachable);
}

void TeslaBLEVehicle::yield_link_() {
  LogScope log_scope(this);
  if (ready_this_turn_) {
    missed_turns_ = 0;
  } else if (missed_turns_ < 8) {
    ++missed_turns_;
  }
  if (missed_turns_ > 0) {
    uint32_t backoff = MISSED_TURN_BACKOFF_MS << (missed_turns_ - 1);
    if (backoff > MAX_MISSED_TURN_BACKOFF_MS) backoff = MAX_MISSED_TURN_BACKOFF_MS;
    retry_turn_after_ms_ = millis() + backoff;
    ESP_LOGW(TAG, "[%s] Not reachable during its BLE turn - next try in %u s", log_name(),
             (unsigned) (backoff / 1000));
  }
  ready_this_turn_ = false;

  const auto state = ble_client_ != nullptr ? ble_client_->state() : espbt::ClientState::IDLE;
  if (state == espbt::ClientState::IDLE || state == espbt::ClientState::INIT)
    return;
  ESP_LOGI(TAG, "[%s] Yielding BLE link to the next car", log_name());
  yielding_link_ = true;
  ble_client_->disconnect();
}

bool TeslaBLEVehicle::queue_until_connected_(const std::string &name, std::function<void()> run,
                                             std::function<void()> expire) {
  if (!has_ble_address()) {
    // Nothing to connect to: fail now instead of waiting for a turn that
    // never comes.
    ESP_LOGW(TAG, "[%s] No BLE MAC - '%s' not sent (press Find Car)", log_name(), name.c_str());
    if (expire) expire();
    return false;
  }
  if (pending_commands_.size() >= MAX_PENDING_COMMANDS) {
    ESP_LOGW(TAG, "[%s] Too many queued commands - dropping '%s'", log_name(),
             pending_commands_.front().name.c_str());
    if (pending_commands_.front().expire) pending_commands_.front().expire();
    pending_commands_.pop_front();
  }
  ESP_LOGI(TAG, "[%s] Not connected - '%s' waits for this car's BLE turn", log_name(), name.c_str());
  pending_commands_.push_back(PendingCommand{millis(), name, std::move(run), std::move(expire)});
  turn_requested_ = true;
  return true;
}

void TeslaBLEVehicle::flush_pending_commands_() {
  std::deque<PendingCommand> pending;
  pending.swap(pending_commands_);
  for (auto &cmd : pending) {
    ESP_LOGI(TAG, "[%s] Sending queued '%s'", log_name(), cmd.name.c_str());
    if (cmd.run) cmd.run();
  }
}

void TeslaBLEVehicle::expire_pending_commands_(uint32_t now) {
  while (!pending_commands_.empty() &&
         now - pending_commands_.front().queued_ms >= PENDING_COMMAND_TIMEOUT_MS) {
    LogScope log_scope(this);
    ESP_LOGW(TAG, "[%s] '%s' expired - car not reachable", log_name(),
             pending_commands_.front().name.c_str());
    auto expire = std::move(pending_commands_.front().expire);
    pending_commands_.pop_front();
    if (expire) expire();
  }
}

void TeslaBLEVehicle::update() {
  LogScope log_scope(this);
  if (!link_ready())
    return;

  uint32_t now = millis();

  // Pairing is an unauthenticated VCSEC exchange and needs a quiet link.
  // Do not add background polls while it is waiting for the card approval
  // response. Existing commands are left untouched; no BLE reset is done.
  if (pairing_in_progress_) {
    const uint32_t pairing_age = static_cast<uint32_t>(now - pairing_started_ms_);
    if (pairing_age < PAIRING_POLL_PAUSE_MS)
      return;
    if (pairing_age >= PAIRING_REQUEST_GUARD_MS)
      pairing_in_progress_ = false;
  }

  // VCSEC Polling
  if (now - last_vcsec_poll_ >= vcsec_poll_interval_) {
    ESP_LOGI(TAG, "[%s] Polling VCSEC", log_name());
    vehicle_->vcsec_poll();
    last_vcsec_poll_ = now;
  }

  if (!infotainment_check_pending_)
    maybe_poll_infotainment_(now);
}

void TeslaBLEVehicle::maybe_poll_infotainment_(uint32_t now) {
  if (!link_ready() || !state_manager_)
    return;

  // Infotainment Polling - use faster interval when vehicle is active
  const bool is_asleep = state_manager_->is_asleep();
  // Only sentry mode and active charging warrant keeping the car awake. A car
  // that is merely left unlocked, or reports user presence because a phone is
  // in range, can still fall asleep on its own and must be allowed to do so
  // (issues #201/#202). To add faster polling for unlocked/user-present later,
  // extend the decision inputs here (and gate it behind an opt-in config).
  InfotainmentPollDecision decision =
      poll_policy_.update(now, is_asleep, state_manager_->is_charging(),
                          state_manager_->is_sentry_mode(), state_manager_->is_climate_on());
  if (decision.poll_now && infotainment_ever_polled_)
    ESP_LOGI(TAG, "[%s] Car woke up - reading its state once", log_name());

  if (!infotainment_ever_polled_ || decision.poll_now || poll_policy_.should_poll(now, decision.interval_ms)) {
    TeslaBLE::WakePolicy policy =
        decision.wake_policy == WakePolicy::NO_WAKE_SKIP
            ? TeslaBLE::WakePolicy::NO_WAKE_SKIP
            : TeslaBLE::WakePolicy::WAKE_IF_NEEDED;
    // The first poll after boot wakes the car once (wake_on_boot) so every
    // sensor gets a value; later ones follow the policy, which only allows a
    // wake while the car is known to be awake.
    if (!infotainment_ever_polled_) {
      policy = wake_on_boot_ ? TeslaBLE::WakePolicy::WAKE_IF_NEEDED : TeslaBLE::WakePolicy::NO_WAKE_SKIP;
      if (wake_on_boot_ && is_asleep)
        ESP_LOGI(TAG, "[%s] First poll after boot - waking the car once to fill sensors", log_name());
    }
    ESP_LOGI(TAG, "[%s] Polling Infotainment (%s)", log_name(),
             policy == TeslaBLE::WakePolicy::NO_WAKE_SKIP
                 ? "sleeping - NO_WAKE_SKIP"
                 : "active - WAKE_IF_NEEDED");
    enqueue_poll_batch_(policy);
    poll_policy_.on_poll(now);
    infotainment_ever_polled_ = true;
  }
}


void TeslaBLEVehicle::enqueue_infotainment_work_(std::function<void()> start, bool interactive,
                                                 std::function<void()> requeue) {
  InfotainmentWorkItem item{std::move(start), interactive, std::move(requeue)};

  // User commands run before queued background polls, FIFO among themselves.
  if (interactive) {
    auto pos = infotainment_queue_.begin();
    while (pos != infotainment_queue_.end() && pos->interactive) ++pos;
    infotainment_queue_.insert(pos, std::move(item));
  } else {
    infotainment_queue_.push_back(std::move(item));
  }
  start_next_infotainment_work_();
}

void TeslaBLEVehicle::start_next_infotainment_work_() {
  // Waiting work stays queued until the link is ready; a lost link clears it
  // (cancel_queued_infotainment_work_).
  if (infotainment_busy_ || infotainment_queue_.empty() || !link_ready())
    return;
  auto next = std::move(infotainment_queue_.front());
  infotainment_queue_.pop_front();
  infotainment_busy_ = true;
  if (next.start) {
    LogScope log_scope(this);
    next.start();
  }
}

void TeslaBLEVehicle::defer_release_infotainment_slot_() {
  // Called from the library's completion callback: start the next command on
  // the next loop pass, not re-entrantly from inside the library. No extra
  // delay - the library sends one command at a time and waits for its answer,
  // and only one car is connected, so there is nothing to space out.
  this->set_timeout("release-infotainment-slot", 0, [this]() {
    release_infotainment_slot_();
  });
}

void TeslaBLEVehicle::release_infotainment_slot_() {
  infotainment_busy_ = false;
  start_next_infotainment_work_();
}

void TeslaBLEVehicle::cancel_queued_infotainment_work_() {
  // Background polls are simply dropped; user commands are not lost.
  std::vector<std::function<void()>> requeue;
  for (auto &item : infotainment_queue_) {
    if (item.interactive && item.requeue) requeue.push_back(std::move(item.requeue));
  }
  infotainment_queue_.clear();
  for (auto &again : requeue) again();
}

void TeslaBLEVehicle::complete_poll_batch_job_() {
  if (!poll_batch_in_progress_) return;
  if (poll_batch_remaining_ > 0) --poll_batch_remaining_;
  if (poll_batch_remaining_ == 0) {
    poll_batch_in_progress_ = false;
    ESP_LOGD(TAG, "[%s] Infotainment batch complete", log_name());
  }
}

void TeslaBLEVehicle::enqueue_poll_job_(const char *name, int32_t data_type,
                                        TeslaBLE::WakePolicy policy) {
  enqueue_infotainment_work_(
      [this, name = std::string(name), data_type, policy]() {
        if (!link_ready()) {
          complete_poll_batch_job_();
          release_infotainment_slot_();
          return;
        }

        vehicle_->send_command_result(
            UniversalMessage_Domain_DOMAIN_INFOTAINMENT,
            name,
            [data_type](TeslaBLE::Client *client, uint8_t *buff, size_t *len) {
              return client->build_car_server_get_vehicle_data_message(buff, len, data_type);
            },
            [this, name](TeslaBLE::OperationResult result) {
              if (!result.is_success() && !result.is_skipped()) {
                const TeslaBLE::CommandError *error = result.error();
                ESP_LOGW(TAG, "[%s] %s failed: %s", log_name(), name.c_str(),
                         error != nullptr ? error->message().c_str() : "unknown error");
              }
              complete_poll_batch_job_();
              defer_release_infotainment_slot_();
            },
            policy);
      },
      false);
}

void TeslaBLEVehicle::enqueue_poll_batch_(TeslaBLE::WakePolicy policy, uint32_t delay_ms) {
  if (!link_ready()) return;
  if (poll_batch_in_progress_) {
    ESP_LOGD(TAG, "[%s] Infotainment batch already in progress - skipping new batch", log_name());
    return;
  }

  poll_batch_in_progress_ = true;
  poll_batch_remaining_ = 5;

  auto enqueue_all = [this, policy]() {
    if (!link_ready()) {
      poll_batch_in_progress_ = false;
      poll_batch_remaining_ = 0;
      return;
    }

    struct PollSpec {
      const char *name;
      int32_t data_type;
    };
    static const PollSpec polls[] = {
        {"Charge State Poll", CarServer_GetVehicleData_getChargeState_tag},
        {"Climate State Poll", CarServer_GetVehicleData_getClimateState_tag},
        {"Drive State Poll", CarServer_GetVehicleData_getDriveState_tag},
        {"Closures State Poll", CarServer_GetVehicleData_getClosuresState_tag},
        {"Tire Pressure Poll", CarServer_GetVehicleData_getTirePressureState_tag},
    };
    static constexpr uint8_t POLL_COUNT = sizeof(polls) / sizeof(polls[0]);

    // Media only while the car is awake: it has nothing to report asleep, and
    // this poll must never be the one that keeps it awake. Counted before any
    // job is queued, so an early finish cannot close the batch too soon.
    const bool poll_media = media_player_ != nullptr && state_manager_ && !state_manager_->is_asleep();
    poll_batch_remaining_ = POLL_COUNT + (poll_media ? 1 : 0);

    const uint8_t start = poll_batch_start_ % POLL_COUNT;
    poll_batch_start_ = static_cast<uint8_t>((start + 1) % POLL_COUNT);
    ESP_LOGI(TAG, "[%s] Infotainment batch starts with %s", log_name(), polls[start].name);

    for (uint8_t offset = 0; offset < POLL_COUNT; ++offset) {
      const PollSpec &poll = polls[(start + offset) % POLL_COUNT];
      enqueue_poll_job_(poll.name, poll.data_type, policy);
    }
    if (poll_media) {
      enqueue_poll_job_("Media State Poll", CarServer_GetVehicleData_getMediaState_tag,
                        TeslaBLE::WakePolicy::NO_WAKE_SKIP);
    }
  };

  if (delay_ms == 0) {
    enqueue_all();
  } else {
    this->set_timeout("infotainment-batch", delay_ms, [enqueue_all]() mutable {
      enqueue_all();
    });
  }
}

void TeslaBLEVehicle::dump_config() {
  ESP_LOGCONFIG(TAG, "Tesla BLE Vehicle:");
  ESP_LOGCONFIG(TAG, "  VIN: %s", vin_.empty() ? "Not set" : vin_.c_str());
  ESP_LOGCONFIG(TAG, "  Role: %s", role_.c_str());
  ESP_LOGCONFIG(TAG, "  Max Charging Amps: %d",
                state_manager_ ? state_manager_->get_charging_amps_max()
                               : DEFAULT_CHARGING_AMPS_MAX);
  ESP_LOGCONFIG(TAG, "  Polling: VCSEC=%" PRIu32 "ms, Awake=%" PRIu32
                     "ms, Active=%" PRIu32 "ms",
                 vcsec_poll_interval_, poll_policy_.awake_interval_ms(),
                 poll_policy_.active_interval_ms());
  ESP_LOGCONFIG(TAG, "  Sensors: %d binary, %d numeric, %d text",
                pending_binary_sensors_.size(), pending_sensors_.size(),
                pending_text_sensors_.size());
}

// =============================================================================
// Configuration setters
// =============================================================================

void TeslaBLEVehicle::set_vin(const char *vin) {
  if (vin == nullptr) {
    ESP_LOGW(TAG, "Attempted to set null VIN - ignoring");
    return;
  }

  vin_ = vin;
  ESP_LOGD(TAG, "VIN set to: %s", vin_.c_str());

  if (vehicle_) {
    vehicle_->set_vin(vin_);
  }
}

void TeslaBLEVehicle::set_role(const std::string &role) {
  ESP_LOGD(TAG, "Setting role: %s", role.c_str());
  role_ = role;
}

void TeslaBLEVehicle::set_charging_amps_max(int amps_max) {
  ESP_LOGD(TAG, "Setting charging amps max: %d", amps_max);

  if (amps_max <= 0) {
    ESP_LOGW(TAG, "Invalid charging amps max value: %d - ignoring", amps_max);
    return;
  }

  configured_charging_amps_max_ = amps_max;

  if (state_manager_) {
    state_manager_->set_charging_amps_max(amps_max);
  }
}

void TeslaBLEVehicle::restore_charging_amps_max_() {
  if (!state_manager_) return;
  auto pref = global_preferences->make_preference<int32_t>(charging_amps_max_pref_hash_());
  int32_t stored = 0;
  if (pref.load(&stored) && stored > 0 && stored <= 80) {
    ESP_LOGI(TAG, "Restored charging amps max from NVS: %" PRId32 " A", stored);
    state_manager_->set_charging_amps_max(stored);
    return;
  }
  state_manager_->set_charging_amps_max(configured_charging_amps_max_);
}

uint32_t TeslaBLEVehicle::charging_amps_max_pref_hash_() const {
  return fnv1_hash_extend(fnv1_hash("tesla_ble_vehicle.charging_amps_max"), vin_);
}

uint32_t TeslaBLEVehicle::ble_mac_pref_hash_() const {
  return fnv1_hash_extend(fnv1_hash("tesla_ble_vehicle.ble_mac"), vin_);
}

static void format_ble_mac(uint64_t address, char *buf) {
  snprintf(buf, 18, "%02X:%02X:%02X:%02X:%02X:%02X", (unsigned) ((address >> 40) & 0xFF),
           (unsigned) ((address >> 32) & 0xFF), (unsigned) ((address >> 24) & 0xFF),
           (unsigned) ((address >> 16) & 0xFF), (unsigned) ((address >> 8) & 0xFF), (unsigned) (address & 0xFF));
}

bool TeslaBLEVehicle::has_ble_address() const {
  return ble_client_ != nullptr && ble_client_->get_address() != 0;
}

// MAC priority: ble_mac_address > NVS > search.
void TeslaBLEVehicle::restore_ble_mac_() {
  if (ble_client_ == nullptr) return;
  auto pref = global_preferences->make_preference<uint64_t>(ble_mac_pref_hash_());
  uint64_t stored = 0;
  const bool have_stored = pref.load(&stored) && stored != 0;
  char mac[18];

  if (mac_from_config_) {
    // YAML wins. Keep NVS in step, so ble_mac_address can later be removed
    // without a search (written only when it differs).
    const uint64_t configured = ble_client_->get_address();
    if (configured != 0 && (!have_stored || stored != configured)) {
      uint64_t value = configured;
      pref.save(&value);
    }
    discovery_ = Discovery::DISC_CONFIGURED;
  } else if (have_stored) {
    format_ble_mac(stored, mac);
    ESP_LOGI(TAG, "[%s] Using saved BLE MAC %s", log_name(), mac);
    ble_client_->set_address(stored);
    discovery_ = Discovery::DISC_FOUND;
  } else {
    start_discovery_("no MAC configured or saved");
    return;
  }
  publish_discovery_();
}

void TeslaBLEVehicle::publish_discovery_() {
  if (!state_manager_) return;
  const char *text = "";
  switch (discovery_) {
    case Discovery::DISC_SEARCHING: text = "Searching"; break;
    case Discovery::DISC_FOUND: text = "Found"; break;
    case Discovery::DISC_NOT_FOUND: text = "Not found"; break;
    case Discovery::DISC_CONFIGURED: text = "Configured"; break;
    case Discovery::DISC_NONE: break;
  }
  std::string mac_text;
  if (has_ble_address()) {
    char mac[18];
    format_ble_mac(ble_client_->get_address(), mac);
    mac_text = mac;
  }
  state_manager_->update_discovery(text, mac_text);
}

void TeslaBLEVehicle::find_car() {
  LogScope log_scope(this);
  if (advert_name_.empty()) {
    ESP_LOGW(TAG, "[%s] Cannot search: no valid VIN", log_name());
    return;
  }
  start_discovery_("Find Car pressed");
}

void TeslaBLEVehicle::start_discovery_(const char *why) {
  discovery_ = Discovery::DISC_SEARCHING;
  discovery_until_ms_ = 0;  // timed once the scanner runs (update_discovery_)
  ESP_LOGW(TAG, "[%s] Searching for advert %s (%s)", log_name(), advert_name_.c_str(), why);
  publish_discovery_();
}

void TeslaBLEVehicle::finish_discovery_(Discovery result) {
  discovery_ = result;
  discovery_until_ms_ = 0;
  if (result == Discovery::DISC_NOT_FOUND) {
    ESP_LOGW(TAG, "[%s] Not found within %u s - check the VIN, or press Find Car with the car nearby",
             log_name(), (unsigned) (DISCOVERY_WINDOW_MS / 1000));
  }
  publish_discovery_();
}

void TeslaBLEVehicle::update_discovery_(uint32_t now) {
  if (now - discovery_checked_ms_ < 1000) return;
  discovery_checked_ms_ = now;
  auto *tracker = esp32_ble_tracker::global_esp32_ble_tracker;
  if (tracker == nullptr) return;

  bool searching = false;
  for (auto *v : link_vehicles_) {
    if (v->discovery_ != Discovery::DISC_SEARCHING) continue;
    if (v->discovery_until_ms_ == 0) {
      // Start the clock once the scanner is up, in the mode we asked for.
      if (tracker->scan_running() && tracker->get_scan_active())
        v->discovery_until_ms_ = (now + DISCOVERY_WINDOW_MS) | 1;
    } else if (static_cast<int32_t>(now - v->discovery_until_ms_) >= 0) {
      LogScope log_scope(v);
      // Not seen in time. A saved MAC (Find Car on a known car) is kept.
      v->finish_discovery_(v->mac_from_config_ ? Discovery::DISC_CONFIGURED : Discovery::DISC_NOT_FOUND);
      continue;
    }
    searching = true;
  }

  bool want_active;
  if (searching && !tracker->get_scan_active()) {
    discovery_forced_active_scan_ = true;
    want_active = true;
    ESP_LOGI(TAG, "Active BLE scan while searching for a car");
  } else if (!searching && discovery_forced_active_scan_) {
    discovery_forced_active_scan_ = false;
    want_active = false;
    ESP_LOGI(TAG, "Search over - back to passive BLE scan");
  } else {
    return;
  }
  // Same sequence as bluetooth_proxy: the continuous scan restarts with the
  // new mode once the stop has completed.
  tracker->set_scan_active(want_active);
  tracker->stop_scan();
  tracker->set_scan_continuous(true);
}

void TeslaBLEVehicle::on_advert_name_seen(uint64_t address) {
  if (ble_client_ == nullptr || address == 0) return;
  const uint64_t current = ble_client_->get_address();
  char mac[18];
  format_ble_mac(address, mac);

  if (mac_from_config_) {
    // Never override YAML; point at a likely typo or swapped cars, once.
    if (address != current && !mac_mismatch_warned_) {
      mac_mismatch_warned_ = true;
      char configured[18];
      format_ble_mac(current, configured);
      ESP_LOGW(TAG, "[%s] This car's advert %s comes from %s, but ble_mac_address is %s - typo or swapped cars?",
               log_name(), advert_name_.c_str(), mac, configured);
    }
    if (discovery_ == Discovery::DISC_SEARCHING) finish_discovery_(Discovery::DISC_CONFIGURED);
    return;
  }

  // Outside a search, only a car with no MAC at all takes one (e.g. the user
  // scans actively anyway). A search may replace a saved MAC.
  if (current != 0 && discovery_ != Discovery::DISC_SEARCHING) return;
  if (current == address) {
    if (discovery_ == Discovery::DISC_SEARCHING) {
      ESP_LOGW(TAG, "[%s] Found car: BLE MAC %s (unchanged)", log_name(), mac);
      finish_discovery_(Discovery::DISC_FOUND);
    }
    return;
  }
  // Changing the address under a live or opening link would confuse the
  // client; the next advert (the search is still running) retries.
  if (current != 0 && ble_client_->state() != espbt::ClientState::IDLE) return;
  adopt_address_(address);
}

void TeslaBLEVehicle::adopt_address_(uint64_t address) {
  char mac[18];
  format_ble_mac(address, mac);
  const uint64_t current = ble_client_->get_address();
  if (current == 0) {
    ESP_LOGW(TAG, "[%s] Found car: BLE MAC %s (advert %s)", log_name(), mac, advert_name_.c_str());
  } else {
    char old_mac[18];
    format_ble_mac(current, old_mac);
    ESP_LOGW(TAG, "[%s] Found car: BLE MAC %s (was %s)", log_name(), mac, old_mac);
  }
  ble_client_->set_address(address);
  auto pref = global_preferences->make_preference<uint64_t>(ble_mac_pref_hash_());
  pref.save(&address);
  finish_discovery_(Discovery::DISC_FOUND);
}

void TeslaBLEVehicle::save_charging_amps_max_(int max) {
  if (max <= 0 || max > 80) return;
  auto pref = global_preferences->make_preference<int32_t>(charging_amps_max_pref_hash_());
  const int32_t value = max;
  if (pref.save(&value)) {
    ESP_LOGD(TAG, "Persisted charging amps max %d A", max);
  }
}

namespace {
struct ChargeSessionPref {
  int32_t phases;
  float power_kw;
};
}  // namespace

uint32_t TeslaBLEVehicle::charge_session_pref_hash_() const {
  return fnv1_hash_extend(fnv1_hash("tesla_ble_vehicle.charge_session"), vin_);
}

void TeslaBLEVehicle::restore_charge_session_() {
  if (!state_manager_) return;
  auto pref = global_preferences->make_preference<ChargeSessionPref>(charge_session_pref_hash_());
  ChargeSessionPref stored{};
  if (pref.load(&stored)) {
    ESP_LOGI(TAG, "Restored last charge session: %" PRId32 " phase(s), %.2f kW", stored.phases, stored.power_kw);
    state_manager_->restore_charge_session(stored.phases, stored.power_kw);
  }
}

void TeslaBLEVehicle::save_charge_session_(int32_t phases, float power_kw) {
  auto pref = global_preferences->make_preference<ChargeSessionPref>(charge_session_pref_hash_());
  const ChargeSessionPref value{phases, power_kw};
  if (pref.save(&value)) {
    ESP_LOGD(TAG, "Persisted charge session: %" PRId32 " phase(s), %.2f kW", phases, power_kw);
  }
}

void TeslaBLEVehicle::set_vcsec_poll_interval(uint32_t interval_ms) {
  ESP_LOGD(TAG, "Setting VCSEC poll interval: %" PRIu32 " ms", interval_ms);
  vcsec_poll_interval_ = interval_ms;
}

void TeslaBLEVehicle::set_infotainment_poll_interval_awake(
    uint32_t interval_ms) {
  ESP_LOGD(TAG, "Setting infotainment poll interval awake: %" PRIu32 " ms",
           interval_ms);
  poll_policy_.set_awake_interval_ms(interval_ms);
}

void TeslaBLEVehicle::set_infotainment_poll_interval_active(
    uint32_t interval_ms) {
  ESP_LOGD(TAG, "Setting infotainment poll interval active: %" PRIu32 " ms",
            interval_ms);
  poll_policy_.set_active_interval_ms(interval_ms);
}

void TeslaBLEVehicle::set_infotainment_sleep_timeout(uint32_t interval_ms) {
  ESP_LOGD(TAG, "Setting infotainment sleep timeout: %" PRIu32 " ms",
           interval_ms);
  poll_policy_.set_sleep_timeout_ms(interval_ms);
}

// =============================================================================
// Generic sensor setters
// =============================================================================

void TeslaBLEVehicle::set_binary_sensor(const std::string &id,
                                        binary_sensor::BinarySensor *sensor) {
  pending_binary_sensors_[id] = sensor;
  if (state_manager_)
    state_manager_->set_binary_sensor(id, sensor);
}

void TeslaBLEVehicle::set_sensor(const std::string &id,
                                 sensor::Sensor *sensor) {
  pending_sensors_[id] = sensor;
  if (state_manager_)
    state_manager_->set_sensor(id, sensor);
}

void TeslaBLEVehicle::set_text_sensor(const std::string &id,
                                      text_sensor::TextSensor *sensor) {
  pending_text_sensors_[id] = sensor;
  if (state_manager_)
    state_manager_->set_text_sensor(id, sensor);
}

// =============================================================================
// Control setters
// =============================================================================

void TeslaBLEVehicle::set_charging_switch(switch_::Switch *sw) {
  pending_charging_switch_ = sw;
  if (state_manager_)
    state_manager_->set_charging_switch(sw);
}

void TeslaBLEVehicle::set_steering_wheel_heat_switch(switch_::Switch *sw) {
  pending_steering_wheel_heat_switch_ = sw;
  if (state_manager_)
    state_manager_->set_steering_wheel_heat_switch(sw);
}

void TeslaBLEVehicle::set_sentry_mode_switch(switch_::Switch *sw) {
  pending_sentry_mode_switch_ = sw;
  if (state_manager_)
    state_manager_->set_sentry_mode_switch(sw);
}

void TeslaBLEVehicle::set_charging_amps_number(number::Number *number) {
  pending_charging_amps_number_ = number;
  if (state_manager_)
    state_manager_->set_charging_amps_number(number);
}

void TeslaBLEVehicle::set_charging_limit_number(number::Number *number) {
  pending_charging_limit_number_ = number;
  if (state_manager_)
    state_manager_->set_charging_limit_number(number);
}

void TeslaBLEVehicle::set_scheduled_charging_switch(switch_::Switch *sw) {
  pending_scheduled_charging_switch_ = sw;
  if (state_manager_)
    state_manager_->set_scheduled_charging_switch(sw);
}

void TeslaBLEVehicle::set_scheduled_charging_time_entity(datetime::TimeEntity *time) {
  pending_scheduled_charging_time_ = time;
  if (state_manager_)
    state_manager_->set_scheduled_charging_time(time);
}

void TeslaBLEVehicle::set_cabin_overheat_select(select::Select *sel) {
  pending_cabin_overheat_select_ = sel;
  if (state_manager_)
    state_manager_->set_cabin_overheat_select(sel);
}

// =============================================================================
// Lock, Cover, and Climate setters
// =============================================================================

void TeslaBLEVehicle::set_doors_lock(lock::Lock *lck) {
  pending_doors_lock_ = lck;
  if (state_manager_)
    state_manager_->set_doors_lock(lck);
}

void TeslaBLEVehicle::set_charge_port_latch_lock(lock::Lock *lck) {
  pending_charge_port_latch_lock_ = lck;
  if (state_manager_)
    state_manager_->set_charge_port_latch_lock(lck);
}

void TeslaBLEVehicle::set_trunk_cover(cover::Cover *cvr) {
  pending_trunk_cover_ = cvr;
  if (state_manager_)
    state_manager_->set_trunk_cover(cvr);
}

void TeslaBLEVehicle::set_frunk_cover(cover::Cover *cvr) {
  pending_frunk_cover_ = cvr;
  if (state_manager_)
    state_manager_->set_frunk_cover(cvr);
}

void TeslaBLEVehicle::set_windows_cover(cover::Cover *cvr) {
  pending_windows_cover_ = cvr;
  if (state_manager_)
    state_manager_->set_windows_cover(cvr);
}

void TeslaBLEVehicle::set_charge_port_door_cover(cover::Cover *cvr) {
  pending_charge_port_door_cover_ = cvr;
  if (state_manager_)
    state_manager_->set_charge_port_door_cover(cvr);
}

void TeslaBLEVehicle::set_climate(climate::Climate *clm) {
  pending_climate_ = clm;
  if (state_manager_)
    state_manager_->set_climate(clm);
}

// =============================================================================
// Button setters
// =============================================================================

void TeslaBLEVehicle::set_wake_button(button::Button *button) {
  TeslaWakeButton *wake_button = static_cast<TeslaWakeButton *>(button);
  if (wake_button)
    wake_button->set_parent(this);
}

void TeslaBLEVehicle::set_pair_button(button::Button *button) {
  TeslaPairButton *pair_button = static_cast<TeslaPairButton *>(button);
  if (pair_button)
    pair_button->set_parent(this);
}

void TeslaBLEVehicle::set_regenerate_key_button(button::Button *button) {
  TeslaRegenerateKeyButton *regen_button =
      static_cast<TeslaRegenerateKeyButton *>(button);
  if (regen_button)
    regen_button->set_parent(this);
}

void TeslaBLEVehicle::set_force_update_button(button::Button *button) {
  TeslaForceUpdateButton *update_button =
      static_cast<TeslaForceUpdateButton *>(button);
  if (update_button)
    update_button->set_parent(this);
}

// =============================================================================
// Command tracking (v5.1.0 OperationResult + phase callbacks)
// =============================================================================

void TeslaBLEVehicle::handle_command_result(const std::string &name,
                                            TeslaBLE::OperationResult result) {
  last_user_command_ms_ = millis();
  user_command_seen_ = true;
  std::string value = name;

  const auto outcome = result.is_success()
                           ? CommandOutcome::SUCCESS
                           : result.is_skipped() ? CommandOutcome::SKIPPED : CommandOutcome::FAILED;
  if (outcome == CommandOutcome::SUCCESS) {
    value += " → Success";
  } else if (outcome == CommandOutcome::SKIPPED) {
    value += " → Skipped";
  } else {
    value += " → Failed";
    if (result.error()) {
      value += ": ";
      value += result.error()->message();
    }
    ESP_LOGW(TAG, "Command failed: %s", value.c_str());
  }
  apply_command_warning(*this, outcome, [this]() {
    this->cancel_timeout(COMMAND_WARNING_TIMEOUT);
  });

  if (last_command_sensor_)
    last_command_sensor_->publish_state(value);
}

void TeslaBLEVehicle::send_command_with_tracking(
    UniversalMessage_Domain domain,
    const std::string &name,
    std::function<int(TeslaBLE::Client *, uint8_t *, size_t *)> builder,
    TeslaBLE::WakePolicy wake_policy, std::function<void(bool)> on_result) {
  send_command_tracked_(domain, name, std::move(builder), wake_policy, std::move(on_result), 1);
}

bool TeslaBLEVehicle::should_retry_command_(const std::string &name,
                                            const TeslaBLE::OperationResult &result,
                                            uint8_t retries_left) {
  if (result.is_success() || result.is_skipped() || retries_left == 0)
    return false;
  const TeslaBLE::CommandError *error = result.error();
  // Temporary errors are link or session resets (connection lost, session
  // stale), e.g. the infotainment session timing out right after a wake.
  // The car is awake by the next turn, so one more try usually succeeds.
  if (error == nullptr || !error->is_temporary())
    return false;
  ESP_LOGW(TAG, "[%s] '%s' failed (%s) - retrying once", log_name(), name.c_str(),
           error->message().c_str());
  return true;
}

void TeslaBLEVehicle::send_command_tracked_(
    UniversalMessage_Domain domain, const std::string &name,
    std::function<int(TeslaBLE::Client *, uint8_t *, size_t *)> builder,
    TeslaBLE::WakePolicy wake_policy, std::function<void(bool)> on_result, uint8_t retries_left,
    bool woken) {
  LogScope log_scope(this);
  if (!vehicle_) {
    ESP_LOGE(TAG, "Cannot send command '%s': vehicle not initialized", name.c_str());
    return;
  }

  if (!link_ready()) {
    // Not this car's BLE turn (or out of range): send it once connected.
    queue_until_connected_(
        name,
        [this, domain, name, builder, wake_policy, on_result, retries_left, woken]() {
          send_command_tracked_(domain, name, builder, wake_policy, on_result, retries_left, woken);
        },
        [this, name, on_result]() {
          if (last_command_sensor_)
            last_command_sensor_->publish_state(name + " → Failed: car not reachable");
          if (on_result) on_result(false);
        });
    return;
  }

  // Infotainment command for a sleeping car: wake it as a separate step,
  // give infotainment time to come up, then send the command.
  if (!woken && domain == UniversalMessage_Domain_DOMAIN_INFOTAINMENT &&
      wake_policy == TeslaBLE::WakePolicy::WAKE_IF_NEEDED && state_manager_ &&
      state_manager_->is_asleep()) {
    ESP_LOGI(TAG, "[%s] '%s': car asleep - waking it first", log_name(), name.c_str());
    if (user_commands_in_flight_ < 255) ++user_commands_in_flight_;
    vehicle_->send_command_result(
        UniversalMessage_Domain_DOMAIN_VEHICLE_SECURITY, "Wake",
        [](TeslaBLE::Client *client, uint8_t *buff, size_t *len) {
          return client->build_vcsec_action_message(VCSEC_RKEAction_E_RKE_ACTION_WAKE_VEHICLE, buff, len);
        },
        [this, domain, name, builder, wake_policy, on_result, retries_left](TeslaBLE::OperationResult result) {
          if (!result.is_success()) {
            if (user_commands_in_flight_ > 0) --user_commands_in_flight_;
            if (should_retry_command_(name, result, retries_left)) {
              this->defer([this, domain, name, builder, wake_policy, on_result, retries_left]() {
                send_command_tracked_(domain, name, builder, wake_policy, on_result, retries_left - 1);
              });
              return;
            }
            handle_command_result(name, std::move(result));
            if (on_result) on_result(false);
            return;
          }
          ESP_LOGI(TAG, "[%s] Awake - sending '%s' in %u s", log_name(), name.c_str(),
                   (unsigned) (WAKE_SETTLE_MS / 1000));
          // Still counted as in flight, so the car keeps its BLE turn.
          this->set_timeout(WAKE_SETTLE_MS, [this, domain, name, builder, wake_policy, on_result, retries_left]() {
            if (user_commands_in_flight_ > 0) --user_commands_in_flight_;
            send_command_tracked_(domain, name, builder, wake_policy, on_result, retries_left, true);
          });
        },
        TeslaBLE::WakePolicy::WAKE_IF_NEEDED);
    return;
  }

  // Re-issues this command later (next loop, or next turn if disconnected).
  auto retry = [this, domain, name, builder, wake_policy, on_result](uint8_t left) {
    this->defer([this, domain, name, builder, wake_policy, on_result, left]() {
      send_command_tracked_(domain, name, builder, wake_policy, on_result, left);
    });
  };

  if (domain == UniversalMessage_Domain_DOMAIN_INFOTAINMENT) {
    if (user_commands_in_flight_ < 255) ++user_commands_in_flight_;
    enqueue_infotainment_work_(
        [this, domain, name, builder, wake_policy, on_result, retries_left, retry]() {
          vehicle_->send_command_result(
              domain, name, builder,
              [this, name, on_result, retries_left, retry](TeslaBLE::OperationResult result) {
                if (user_commands_in_flight_ > 0) --user_commands_in_flight_;
                defer_release_infotainment_slot_();
                if (should_retry_command_(name, result, retries_left)) {
                  retry(retries_left - 1);
                  return;
                }
                const bool succeeded = result.is_success();
                handle_command_result(name, std::move(result));
                if (on_result) on_result(succeeded);
              },
              wake_policy);
        },
        true,
        // Dropped from the queue before it ran (link lost): send it again.
        [retry, retries_left]() { retry(retries_left); });
    return;
  }

  if (user_commands_in_flight_ < 255) ++user_commands_in_flight_;
  vehicle_->send_command_result(
      domain, name, builder,
      [this, domain, name, on_result, retries_left, retry](TeslaBLE::OperationResult result) {
        if (user_commands_in_flight_ > 0) --user_commands_in_flight_;
        if (domain == UniversalMessage_Domain_DOMAIN_INFOTAINMENT)
          release_infotainment_slot_();
        if (should_retry_command_(name, result, retries_left)) {
          retry(retries_left - 1);
          return;
        }
        const bool succeeded = result.is_success();
        handle_command_result(name, std::move(result));
        if (on_result) on_result(succeeded);
      },
      wake_policy);
}

void TeslaBLEVehicle::schedule_state_refresh_(ControlStateRefresh refresh) {
  if (!vehicle_ || refresh == ControlStateRefresh::NONE) return;

  const char *timeout_name = nullptr;
  switch (refresh) {
    case ControlStateRefresh::CHARGE_STATE:
      timeout_name = "charge-state-refresh";
      break;
    case ControlStateRefresh::CLIMATE_STATE:
      timeout_name = "climate-state-refresh";
      break;
    case ControlStateRefresh::CLOSURES_STATE:
      timeout_name = "closures-state-refresh";
      break;
    case ControlStateRefresh::MEDIA_STATE:
      timeout_name = "media-state-refresh";
      break;
    case ControlStateRefresh::NONE:
      return;
  }

  this->set_timeout(timeout_name, 1500, [this, refresh]() {
    LogScope log_scope(this);
    if (!link_ready()) return;
    switch (refresh) {
      case ControlStateRefresh::CHARGE_STATE:
        vehicle_->charge_state_poll(TeslaBLE::WakePolicy::NO_WAKE_SKIP);
        break;
      case ControlStateRefresh::CLIMATE_STATE:
        vehicle_->climate_state_poll(TeslaBLE::WakePolicy::NO_WAKE_SKIP);
        break;
      case ControlStateRefresh::CLOSURES_STATE:
        vehicle_->closures_state_poll(TeslaBLE::WakePolicy::NO_WAKE_SKIP);
        break;
      case ControlStateRefresh::MEDIA_STATE:
        vehicle_->media_state_poll(TeslaBLE::WakePolicy::NO_WAKE_SKIP);
        break;
      case ControlStateRefresh::NONE:
        break;
    }
  });
}

// =============================================================================
// Public vehicle actions
// =============================================================================

int TeslaBLEVehicle::wake_vehicle() {
  ESP_LOGD(TAG, "Wake vehicle requested");

  if (!vehicle_)
    return -1;

  if (state_manager_ && !state_manager_->is_asleep()) {
    ESP_LOGI(TAG, "Vehicle already awake - sending VCSEC poll instead");
    send_command_with_tracking(
        UniversalMessage_Domain_DOMAIN_VEHICLE_SECURITY, "VCSEC Poll",
        [](TeslaBLE::Client *client, uint8_t *buff, size_t *len) {
          return client->build_vcsec_information_request_message(
              VCSEC_InformationRequestType_INFORMATION_REQUEST_TYPE_GET_STATUS, buff, len);
        },
        TeslaBLE::WakePolicy::NO_WAKE_SKIP);
    return 0;
  }

  ESP_LOGI(TAG, "Sending wake command");
  send_command_with_tracking(
      UniversalMessage_Domain_DOMAIN_VEHICLE_SECURITY, "Wake",
      [](TeslaBLE::Client *client, uint8_t *buff, size_t *len) {
        return client->build_vcsec_action_message(VCSEC_RKEAction_E_RKE_ACTION_WAKE_VEHICLE, buff, len);
      },
      TeslaBLE::WakePolicy::WAKE_IF_NEEDED);
  return 0;
}

int TeslaBLEVehicle::start_pairing() {
  LogScope log_scope(this);
  ESP_LOGI(TAG, "Pairing requested");

  if (!vehicle_) {
    ESP_LOGE(TAG, "Vehicle instance not available");
    return -1;
  }

  if (!link_ready()) {
    queue_until_connected_(
        "Pair", [this]() { start_pairing(); },
        [this]() { ESP_LOGW(TAG, "[%s] Pairing not sent - car not reachable", log_name()); });
    return 0;
  }

  const uint32_t now = millis();
  if (pairing_in_progress_) {
    const uint32_t pairing_age = static_cast<uint32_t>(now - pairing_started_ms_);
    if (pairing_age < PAIRING_REQUEST_GUARD_MS) {
      ESP_LOGI(TAG, "Pairing already requested - present NFC card on reader");
      return 0;
    }
    pairing_in_progress_ = false;
  }

  Keys_Role role_enum = Keys_Role_ROLE_OWNER;
  if (role_ == "DRIVER") {
    role_enum = Keys_Role_ROLE_DRIVER;
  } else if (role_ == "CHARGING_MANAGER") {
    role_enum = Keys_Role_ROLE_CHARGING_MANAGER;
  }

  // tesla-ble v5.2.0 (and current upstream) hardcodes NFC_CARD in
  // Vehicle::pair(). That represents the approving physical key, not the
  // software key being enrolled. Tesla's current BLE pairing flow enrolls
  // software clients as CLOUD_KEY and uses the NFC card/keyfob for approval.
  //
  // Keep the upstream Vehicle implementation for everything else, but build
  // the whitelist request here with the correct form factor.
  std::vector<uint8_t> private_key;
  if (!storage_adapter_ || !storage_adapter_->load("private_key", private_key) ||
      private_key.empty()) {
    ESP_LOGI(TAG, "No private key stored for this vehicle - generating one");
    vehicle_->regenerate_key();
  }

  pairing_in_progress_ = true;
  pairing_started_ms_ = now;

  vehicle_->send_command(
      UniversalMessage_Domain_DOMAIN_VEHICLE_SECURITY, "Whitelist Add Key",
      [role_enum](TeslaBLE::Client *client, uint8_t *buf, size_t *len) {
        return client->build_white_list_message(
            role_enum, VCSEC_KeyFormFactor_KEY_FORM_FACTOR_CLOUD_KEY, buf, len);
      });

  return 0;
}

int TeslaBLEVehicle::regenerate_key() {
  LogScope log_scope(this);
  ESP_LOGI(TAG, "Key regeneration requested");

  if (!vehicle_) {
    ESP_LOGE(TAG, "Vehicle instance not available");
    return -1;
  }

  vehicle_->regenerate_key();
  return 0;
}

void TeslaBLEVehicle::force_update() {
  LogScope log_scope(this);
  uint32_t now = millis();
  if (vehicle_ && !link_ready()) {
    // Run the forced update itself once connected: the normal on-connect
    // polling never wakes the car, but a forced update is an explicit request.
    queue_until_connected_(
        "Force update", [this]() { force_update(); },
        [this]() { ESP_LOGW(TAG, "[%s] Force update not sent - car not reachable", log_name()); });
    return;
  }
  if (!poll_policy_.should_poll(now, poll_policy_.active_interval_ms())) {
    ESP_LOGD(TAG, "Force update requested too soon (within active polling "
                  "interval) - ignoring");
    return;
  }

  ESP_LOGI(TAG, "Force update requested");
  poll_policy_.on_poll(now);

  if (vehicle_) {
    vehicle_->vcsec_poll();
    enqueue_poll_batch_(TeslaBLE::WakePolicy::WAKE_IF_NEEDED, 500);
  }
}

int TeslaBLEVehicle::set_charging_state(bool charging) {
  ESP_LOGI(TAG, "Set charging state: %s", charging ? "ON" : "OFF");

  if (!vehicle_) {
    ESP_LOGE(TAG, "Vehicle instance not available");
    return -1;
  }

  send_command_with_tracking(
      UniversalMessage_Domain_DOMAIN_INFOTAINMENT,
      charging ? "Start Charging" : "Stop Charging",
      [charging](TeslaBLE::Client *client, uint8_t *buff, size_t *len) {
        return client->build_car_server_vehicle_action_message(
            buff, len, CarServer_VehicleAction_chargingStartStopAction_tag, &charging);
      }, TeslaBLE::WakePolicy::WAKE_IF_NEEDED,
      [this, charging](bool succeeded) {
        const auto decision = control_state_decision(ControlStateCommand::CHARGING_STATE, succeeded);
        if (decision.publish_requested_state && state_manager_) state_manager_->update_charging_control_state(charging);
        schedule_state_refresh_(decision.refresh);
      });
  return 0;
}

int TeslaBLEVehicle::set_charging_amps(int amps) {
  ESP_LOGI(TAG, "Set charging amps: %d", amps);

  if (amps < 0) {
    ESP_LOGW(TAG, "Invalid charging amps: %d", amps);
    return 0;
  }

  int max_amps = state_manager_->get_charging_amps_max();
  if (amps > max_amps) {
    ESP_LOGW(TAG, "Requested amps (%d) exceeds maximum (%d), clamping", amps,
             max_amps);
    amps = max_amps;
  }

  int clamped = amps;
  send_command_with_tracking(
      UniversalMessage_Domain_DOMAIN_INFOTAINMENT, "Set Charging Amps",
      [clamped](TeslaBLE::Client *client, uint8_t *buff, size_t *len) {
        return client->build_car_server_vehicle_action_message(
            buff, len, CarServer_VehicleAction_setChargingAmpsAction_tag, &clamped);
      }, TeslaBLE::WakePolicy::WAKE_IF_NEEDED,
      [this, clamped](bool succeeded) {
        const auto decision = control_state_decision(ControlStateCommand::SET_AMPS, succeeded);
        if (decision.publish_requested_state && state_manager_) state_manager_->update_charging_amps(static_cast<float>(clamped));
        if (decision.republish_confirmed_number_state && state_manager_) state_manager_->republish_charging_amps();
        schedule_state_refresh_(decision.refresh);
      });
  return clamped;
}

int TeslaBLEVehicle::set_charging_limit(int limit) {
  ESP_LOGI(TAG, "Set charging limit: %d%%", limit);

  if (limit < MIN_CHARGING_LIMIT || limit > MAX_CHARGING_LIMIT) {
    ESP_LOGW(TAG, "Invalid charging limit: %d%%", limit);
    return -1;
  }

  send_command_with_tracking(
      UniversalMessage_Domain_DOMAIN_INFOTAINMENT, "Set Charging Limit",
      [limit](TeslaBLE::Client *client, uint8_t *buff, size_t *len) {
        return client->build_car_server_vehicle_action_message(
            buff, len, CarServer_VehicleAction_chargingSetLimitAction_tag, &limit);
      }, TeslaBLE::WakePolicy::WAKE_IF_NEEDED,
      [this, limit](bool succeeded) {
        const auto decision = control_state_decision(ControlStateCommand::SET_LIMIT, succeeded);
        if (decision.publish_requested_state && state_manager_) state_manager_->update_charging_limit(static_cast<float>(limit));
        if (decision.republish_confirmed_number_state && state_manager_) state_manager_->republish_charging_limit();
        schedule_state_refresh_(decision.refresh);
      });
  return 0;
}

// =============================================================================
// Closure controls (VCSEC)
// =============================================================================

void TeslaBLEVehicle::lock_vehicle() {
  ESP_LOGI(TAG, "Lock vehicle requested");
  send_command_with_tracking(
      UniversalMessage_Domain_DOMAIN_VEHICLE_SECURITY, "Lock",
      [](TeslaBLE::Client *client, uint8_t *buff, size_t *len) {
        return client->build_vcsec_action_message(VCSEC_RKEAction_E_RKE_ACTION_LOCK, buff, len);
      },
      TeslaBLE::WakePolicy::WAKE_IF_NEEDED,
      [this](bool succeeded) { settle_lock_(false, succeeded); });
}

void TeslaBLEVehicle::unlock_vehicle() {
  ESP_LOGI(TAG, "Unlock vehicle requested");
  send_command_with_tracking(
      UniversalMessage_Domain_DOMAIN_VEHICLE_SECURITY, "Unlock",
      [](TeslaBLE::Client *client, uint8_t *buff, size_t *len) {
        return client->build_vcsec_action_message(VCSEC_RKEAction_E_RKE_ACTION_UNLOCK, buff, len);
      },
      TeslaBLE::WakePolicy::WAKE_IF_NEEDED,
      [this](bool succeeded) { settle_lock_(false, succeeded); });
}

// After a lock / unlock command the entity shows LOCKING / UNLOCKING until
// the car reports the new state. Read the state again so it settles, and if
// the command failed or nothing is reported, go back to the last real state.
void TeslaBLEVehicle::settle_lock_(bool charge_port, bool succeeded) {
  if (!state_manager_) return;
  auto revert = [this, charge_port]() {
    if (!state_manager_) return;
    if (charge_port) state_manager_->republish_charge_port_latch();
    else state_manager_->republish_doors_lock();
  };
  if (!succeeded) {
    revert();
    return;
  }
  if (charge_port) {
    // The latch / door move takes a few seconds: read now and again later
    schedule_state_refresh_(ControlStateRefresh::CHARGE_STATE);
    this->set_timeout("latch-settle", 8000, [this]() {
      LogScope log_scope(this);
      if (link_ready()) vehicle_->charge_state_poll(TeslaBLE::WakePolicy::NO_WAKE_SKIP);
    });
    this->set_timeout("latch-fallback", 20000, revert);
  } else {
    // VCSEC reports the lock state on every status poll
    this->set_timeout("doors-settle", 2000, [this]() {
      LogScope log_scope(this);
      if (link_ready()) vehicle_->vcsec_poll();
    });
    this->set_timeout("doors-fallback", 20000, revert);
  }
}

void TeslaBLEVehicle::open_trunk() {
  ESP_LOGI(TAG, "Open trunk requested");
  send_command_with_tracking(
      UniversalMessage_Domain_DOMAIN_VEHICLE_SECURITY, "Open Trunk",
      [](TeslaBLE::Client *client, uint8_t *buff, size_t *len) {
        VCSEC_ClosureMoveRequest request = VCSEC_ClosureMoveRequest_init_zero;
        request.rearTrunk = VCSEC_ClosureMoveType_E_CLOSURE_MOVE_TYPE_OPEN;
        return client->build_vcsec_closure_message(&request, buff, len);
      });
}

void TeslaBLEVehicle::close_trunk() {
  ESP_LOGI(TAG, "Close trunk requested");
  send_command_with_tracking(
      UniversalMessage_Domain_DOMAIN_VEHICLE_SECURITY, "Close Trunk",
      [](TeslaBLE::Client *client, uint8_t *buff, size_t *len) {
        VCSEC_ClosureMoveRequest request = VCSEC_ClosureMoveRequest_init_zero;
        request.rearTrunk = VCSEC_ClosureMoveType_E_CLOSURE_MOVE_TYPE_CLOSE;
        return client->build_vcsec_closure_message(&request, buff, len);
      });
}

void TeslaBLEVehicle::open_frunk() {
  ESP_LOGI(TAG, "Open frunk requested");
  send_command_with_tracking(
      UniversalMessage_Domain_DOMAIN_VEHICLE_SECURITY, "Open Frunk",
      [](TeslaBLE::Client *client, uint8_t *buff, size_t *len) {
        VCSEC_ClosureMoveRequest request = VCSEC_ClosureMoveRequest_init_zero;
        request.frontTrunk = VCSEC_ClosureMoveType_E_CLOSURE_MOVE_TYPE_OPEN;
        return client->build_vcsec_closure_message(&request, buff, len);
      });
}

void TeslaBLEVehicle::open_charge_port() {
  ESP_LOGI(TAG, "Open charge port requested");
  send_command_with_tracking(
      UniversalMessage_Domain_DOMAIN_VEHICLE_SECURITY, "Open Charge Port",
      [](TeslaBLE::Client *client, uint8_t *buff, size_t *len) {
        VCSEC_ClosureMoveRequest request = VCSEC_ClosureMoveRequest_init_zero;
        request.chargePort = VCSEC_ClosureMoveType_E_CLOSURE_MOVE_TYPE_OPEN;
        return client->build_vcsec_closure_message(&request, buff, len);
      });
}

void TeslaBLEVehicle::close_charge_port() {
  ESP_LOGI(TAG, "Close charge port requested");
  send_command_with_tracking(
      UniversalMessage_Domain_DOMAIN_VEHICLE_SECURITY, "Close Charge Port",
      [](TeslaBLE::Client *client, uint8_t *buff, size_t *len) {
        VCSEC_ClosureMoveRequest request = VCSEC_ClosureMoveRequest_init_zero;
        request.chargePort = VCSEC_ClosureMoveType_E_CLOSURE_MOVE_TYPE_CLOSE;
        return client->build_vcsec_closure_message(&request, buff, len);
      },
      TeslaBLE::WakePolicy::WAKE_IF_NEEDED,
      [this](bool succeeded) { settle_lock_(true, succeeded); });
}

void TeslaBLEVehicle::unlock_charge_port() {
  ESP_LOGI(TAG, "Unlock charge port latch requested");
  send_command_with_tracking(
      UniversalMessage_Domain_DOMAIN_INFOTAINMENT, "Unlock Charge Port",
      [](TeslaBLE::Client *client, uint8_t *buff, size_t *len) {
        return client->build_car_server_vehicle_action_message(
            buff, len, CarServer_VehicleAction_chargePortDoorOpen_tag, nullptr);
      },
      TeslaBLE::WakePolicy::WAKE_IF_NEEDED,
      [this](bool succeeded) { settle_lock_(true, succeeded); });
}

void TeslaBLEVehicle::unlatch_driver_door() {
  ESP_LOGI(TAG, "Unlatch driver door requested");
  send_command_with_tracking(
      UniversalMessage_Domain_DOMAIN_VEHICLE_SECURITY, "Unlatch Driver Door",
      [](TeslaBLE::Client *client, uint8_t *buff, size_t *len) {
        VCSEC_ClosureMoveRequest request = VCSEC_ClosureMoveRequest_init_zero;
        request.frontDriverDoor = VCSEC_ClosureMoveType_E_CLOSURE_MOVE_TYPE_OPEN;
        return client->build_vcsec_closure_message(&request, buff, len);
      });
}

// =============================================================================
// HVAC controls (Infotainment)
// =============================================================================

void TeslaBLEVehicle::set_climate_on(bool enable) {
  ESP_LOGI(TAG, "Climate %s requested", enable ? "ON" : "OFF");
  send_command_with_tracking(
      UniversalMessage_Domain_DOMAIN_INFOTAINMENT,
      enable ? "Climate On" : "Climate Off",
      [enable](TeslaBLE::Client *client, uint8_t *buff, size_t *len) {
        return client->build_car_server_vehicle_action_message(
            buff, len, CarServer_VehicleAction_hvacAutoAction_tag, &enable);
      });
}

void TeslaBLEVehicle::set_climate_temp(float temp) {
  ESP_LOGI(TAG, "Climate temperature %.1f°C requested", temp);
  send_command_with_tracking(
      UniversalMessage_Domain_DOMAIN_INFOTAINMENT, "Set Climate Temp",
      [temp](TeslaBLE::Client *client, uint8_t *buff, size_t *len) {
        return client->build_car_server_vehicle_action_message(
            buff, len, CarServer_VehicleAction_hvacTemperatureAdjustmentAction_tag, &temp);
      });
}

void TeslaBLEVehicle::set_climate_keeper(int mode) {
  const char *mode_names[] = {"Off", "On", "Dog", "Camp"};
  ESP_LOGI(TAG, "Climate keeper %s requested",
           (mode >= 0 && mode <= 3) ? mode_names[mode] : "Unknown");
  send_command_with_tracking(
      UniversalMessage_Domain_DOMAIN_INFOTAINMENT, "Climate Keeper",
      [mode](TeslaBLE::Client *client, uint8_t *buff, size_t *len) {
        return client->build_car_server_vehicle_action_message(
            buff, len, CarServer_VehicleAction_hvacClimateKeeperAction_tag, &mode);
      });
}

void TeslaBLEVehicle::set_bioweapon_mode(bool enable) {
  ESP_LOGI(TAG, "Bioweapon mode %s requested", enable ? "ON" : "OFF");
  send_command_with_tracking(
      UniversalMessage_Domain_DOMAIN_INFOTAINMENT,
      enable ? "Bioweapon On" : "Bioweapon Off",
      [enable](TeslaBLE::Client *client, uint8_t *buff, size_t *len) {
        return client->build_car_server_vehicle_action_message(
            buff, len, CarServer_VehicleAction_hvacBioweaponModeAction_tag, &enable);
      });
}

void TeslaBLEVehicle::set_preconditioning_max(bool enable) {
  ESP_LOGI(TAG, "Preconditioning max (defrost) %s requested",
           enable ? "ON" : "OFF");
  send_command_with_tracking(
      UniversalMessage_Domain_DOMAIN_INFOTAINMENT,
      enable ? "Defrost On" : "Defrost Off",
      [enable](TeslaBLE::Client *client, uint8_t *buff, size_t *len) {
        return client->build_car_server_vehicle_action_message(
            buff, len, CarServer_VehicleAction_hvacSetPreconditioningMaxAction_tag, &enable);
      });
}

void TeslaBLEVehicle::set_scheduled_charging(bool enabled) {
  int minutes = state_manager_ ? state_manager_->scheduled_charging_minutes() : -1;
  if (enabled && minutes < 0) {
    ESP_LOGW(TAG, "[%s] Scheduled charging: no start time known yet - set the time instead", log_name());
    if (state_manager_) state_manager_->republish_scheduled_charging();
    return;
  }
  send_scheduled_charging_(enabled, minutes < 0 ? 0 : minutes);
}

void TeslaBLEVehicle::set_scheduled_charging_time(int minutes) {
  if (minutes < 0 || minutes >= 24 * 60) {
    ESP_LOGW(TAG, "Invalid scheduled charging time: %d min", minutes);
    return;
  }
  send_scheduled_charging_(true, minutes);
}

void TeslaBLEVehicle::set_scheduled_departure(bool enabled) {
  Departure d = departure_;
  if (enabled && d.time < 0) {
    ESP_LOGW(TAG, "[%s] Scheduled departure: no departure time known yet - set the time instead", log_name());
    publish_departure_();
    return;
  }
  d.enabled = enabled;
  send_departure_(d);
}

void TeslaBLEVehicle::set_departure_time(int minutes) {
  if (minutes < 0 || minutes >= 24 * 60) return;
  Departure d = departure_;
  d.enabled = 1;  // setting the time turns departure on, like the charging start time
  d.time = minutes;
  send_departure_(d);
}

void TeslaBLEVehicle::set_departure_preconditioning(int policy) {
  Departure d = departure_;
  d.preconditioning = policy;
  send_departure_(d);
}

void TeslaBLEVehicle::set_departure_off_peak(int policy) {
  Departure d = departure_;
  d.off_peak = policy;
  send_departure_(d);
}

void TeslaBLEVehicle::set_off_peak_end_time(int minutes) {
  if (minutes < 0 || minutes >= 24 * 60) return;
  Departure d = departure_;
  d.off_peak_end = minutes;
  send_departure_(d);
}

void TeslaBLEVehicle::send_departure_(Departure d) {
  // Policy and time changes only apply to an enabled departure schedule
  if (d.enabled != 1 && d.enabled != 0) d.enabled = 1;
  if (d.enabled == 1 && d.time < 0) {
    ESP_LOGW(TAG, "[%s] Scheduled departure: set the departure time first", log_name());
    publish_departure_();
    return;
  }
  CarServer_ScheduledDepartureAction action = CarServer_ScheduledDepartureAction_init_default;
  action.enabled = d.enabled == 1;
  if (action.enabled) {
    action.departure_time = d.time;
    action.off_peak_hours_end_time = d.off_peak_end >= 0 ? d.off_peak_end : 0;
    if (d.preconditioning == state_text::kPolicyAllWeek || d.preconditioning == state_text::kPolicyWeekdays) {
      action.has_preconditioning_times = true;
      action.preconditioning_times.which_times = d.preconditioning;
    }
    if (d.off_peak == state_text::kPolicyAllWeek || d.off_peak == state_text::kPolicyWeekdays) {
      action.has_off_peak_charging_times = true;
      action.off_peak_charging_times.which_times = d.off_peak;
    }
  }

  char name[64];
  if (action.enabled) {
    snprintf(name, sizeof(name), "Scheduled Departure %02d:%02d", d.time / 60, d.time % 60);
  } else {
    snprintf(name, sizeof(name), "Scheduled Departure Off");
  }
  ESP_LOGI(TAG, "%s requested (preconditioning %s, off-peak %s)", name,
           state_text::departure_policy_option(d.preconditioning > 0 ? d.preconditioning : 0),
           state_text::departure_policy_option(d.off_peak > 0 ? d.off_peak : 0));

  send_command_with_tracking(
      UniversalMessage_Domain_DOMAIN_INFOTAINMENT, name,
      [action](TeslaBLE::Client *client, uint8_t *buff, size_t *len) {
        return client->build_car_server_vehicle_action_message(
            buff, len, CarServer_VehicleAction_scheduledDepartureAction_tag, &action);
      },
      TeslaBLE::WakePolicy::WAKE_IF_NEEDED,
      [this](bool succeeded) {
        // Read the schedule back: shows what the car took, or puts the
        // entities back if it did not
        schedule_state_refresh_(ControlStateRefresh::CHARGE_STATE);
        if (!succeeded) publish_departure_();
      });
}

void TeslaBLEVehicle::publish_departure_() {
  const Departure &d = departure_;
  if (departure_switch_ != nullptr && d.enabled >= 0) departure_switch_->publish_state(d.enabled == 1);
  if (departure_time_entity_ != nullptr && d.time >= 0)
    static_cast<TeslaDepartureTime *>(departure_time_entity_)->update_time(d.time);
  if (off_peak_end_entity_ != nullptr && d.off_peak_end >= 0)
    static_cast<TeslaOffPeakEndTime *>(off_peak_end_entity_)->update_time(d.off_peak_end);
  if (departure_precondition_select_ != nullptr && d.preconditioning >= 0)
    departure_precondition_select_->publish_state(state_text::departure_policy_option(d.preconditioning));
  if (departure_off_peak_select_ != nullptr && d.off_peak >= 0)
    departure_off_peak_select_->publish_state(state_text::departure_policy_option(d.off_peak));
}

void TeslaBLEVehicle::send_scheduled_charging_(bool enabled, int minutes) {
  CarServer_ScheduledChargingAction action = CarServer_ScheduledChargingAction_init_default;
  action.enabled = enabled;
  action.charging_time = minutes;

  char name[40];
  if (enabled) {
    snprintf(name, sizeof(name), "Scheduled Charging %02d:%02d", minutes / 60, minutes % 60);
  } else {
    snprintf(name, sizeof(name), "Scheduled Charging Off");
  }
  ESP_LOGI(TAG, "%s requested", name);

  send_command_with_tracking(
      UniversalMessage_Domain_DOMAIN_INFOTAINMENT, name,
      [action](TeslaBLE::Client *client, uint8_t *buff, size_t *len) {
        return client->build_car_server_vehicle_action_message(
            buff, len, CarServer_VehicleAction_scheduledChargingAction_tag, &action);
      },
      TeslaBLE::WakePolicy::WAKE_IF_NEEDED,
      [this](bool succeeded) {
        // Re-read the car's schedule either way: on failure this puts the
        // switch / time back to what the car really has.
        schedule_state_refresh_(ControlStateRefresh::CHARGE_STATE);
      });
}

void TeslaBLEVehicle::set_cabin_overheat_choice(int mode, int level) {
  const int current_mode = state_manager_ ? state_manager_->cop_mode() : -1;
  const int current_level = state_manager_ ? state_manager_->cop_level() : 0;
  bool sent = false;
  if (mode != current_mode) {
    set_cabin_overheat_protection(mode);
    sent = true;
  }
  if (mode == state_text::kCopOn && level != current_level) {
    set_cabin_overheat_temp(level);
    sent = true;
  }
  if (!sent && state_manager_) state_manager_->republish_cabin_overheat();
}

void TeslaBLEVehicle::set_cabin_overheat_temp(int level) {
  const char *option = state_text::cop_temp_option(level);
  if (option == nullptr) {
    ESP_LOGW(TAG, "Invalid cabin overheat protection temperature level: %d", level);
    return;
  }
  ESP_LOGI(TAG, "Cabin overheat protection temperature %s requested", option);
  const int32_t value = level;
  send_command_with_tracking(
      UniversalMessage_Domain_DOMAIN_INFOTAINMENT, std::string("Overheat Temp ") + option,
      [value](TeslaBLE::Client *client, uint8_t *buff, size_t *len) {
        return client->build_car_server_vehicle_action_message(
            buff, len, CarServer_VehicleAction_setCopTempAction_tag, &value);
      },
      TeslaBLE::WakePolicy::WAKE_IF_NEEDED,
      [this](bool succeeded) {
        // Read back the climate state: shows the new level, or puts the
        // select back if the car did not take it
        schedule_state_refresh_(ControlStateRefresh::CLIMATE_STATE);
      });
}

void TeslaBLEVehicle::send_assumed_switch_(const char *name_on, const char *name_off, int32_t action_tag,
                                           bool enable, switch_::Switch *sw) {
  const char *name = enable ? name_on : name_off;
  ESP_LOGI(TAG, "%s requested", name);
  send_command_with_tracking(
      UniversalMessage_Domain_DOMAIN_INFOTAINMENT, name,
      [action_tag, enable](TeslaBLE::Client *client, uint8_t *buff, size_t *len) {
        return client->build_car_server_vehicle_action_message(buff, len, action_tag, &enable);
      },
      TeslaBLE::WakePolicy::WAKE_IF_NEEDED,
      [sw, enable](bool succeeded) {
        // The car does not report these modes: show what was set, or keep the
        // previous state if the command failed
        if (sw == nullptr) return;
        sw->publish_state(succeeded ? enable : sw->state);
      });
}

void TeslaBLEVehicle::set_low_power_mode(bool enable, switch_::Switch *sw) {
  send_assumed_switch_("Low Power Mode On", "Low Power Mode Off",
                       CarServer_VehicleAction_setLowPowerModeAction_tag, enable, sw);
}

void TeslaBLEVehicle::set_keep_accessory_power(bool enable, switch_::Switch *sw) {
  send_assumed_switch_("Keep Accessory Power On", "Keep Accessory Power Off",
                       CarServer_VehicleAction_setKeepAccessoryPowerModeAction_tag, enable, sw);
}

void TeslaBLEVehicle::set_guest_mode(bool enable, switch_::Switch *sw) {
  send_assumed_switch_("Guest Mode On", "Guest Mode Off", CarServer_VehicleAction_guestModeAction_tag, enable, sw);
}

// =============================================================================
// Media
// =============================================================================

static media_player::MediaPlayerState to_media_player_state(state_text::MediaPlay play) {
  switch (play) {
    case state_text::MediaPlay::PLAYING:
      return media_player::MEDIA_PLAYER_STATE_PLAYING;
    case state_text::MediaPlay::PAUSED:
      return media_player::MEDIA_PLAYER_STATE_PAUSED;
    case state_text::MediaPlay::IDLE:
      return media_player::MEDIA_PLAYER_STATE_IDLE;
    case state_text::MediaPlay::OFF:
      break;
  }
  return media_player::MEDIA_PLAYER_STATE_OFF;
}

void TeslaBLEVehicle::handle_media_state_(const CarServer_MediaState &media,
                                          const TeslaBLE::MediaNowPlaying &now_playing) {
  const bool asleep = state_manager_ && state_manager_->is_asleep();
  // What the car sent (-1 = not reported), to tell a missing value from a mapping problem
  ESP_LOGI(TAG, "[%s] Media reported: status=%d volume=%.2f max=%.2f source=%d title=%d bytes artist=%d bytes",
           log_name(),
           media.which_optional_media_playback_status
               ? static_cast<int>(media.optional_media_playback_status.media_playback_status) : -1,
           media.which_optional_audio_volume ? media.optional_audio_volume.audio_volume : -1.0f,
           media.which_optional_audio_volume_max ? media.optional_audio_volume_max.audio_volume_max : -1.0f,
           media.which_optional_now_playing_source
               ? static_cast<int>(media.optional_now_playing_source.now_playing_source) : -1,
           now_playing.has_title ? static_cast<int>(now_playing.title.size()) : -1,
           now_playing.has_artist ? static_cast<int>(now_playing.artist.size()) : -1);
  if (media.which_optional_audio_volume_max)
    media_volume_max_ = state_text::media_volume_max(media.optional_audio_volume_max.audio_volume_max);

  std::optional<int> status;
  if (media.which_optional_media_playback_status)
    status = static_cast<int>(media.optional_media_playback_status.media_playback_status);
  const auto play = state_text::media_play_state(asleep, status);

  if (media_player_ != nullptr) {
    const auto state = to_media_player_state(play);
    float volume = media_player_->volume;
    if (media.which_optional_audio_volume)
      volume = state_text::media_volume_fraction(media.optional_audio_volume.audio_volume, media_volume_max_);
    if (state != media_player_->state || volume != media_player_->volume) {
      media_player_->state = state;
      media_player_->volume = volume;
      media_player_->publish_state();
    }
  }

  if (state_manager_) {
    std::string source;
    if (media.which_optional_now_playing_source) {
      auto text = state_text::media_source(static_cast<int>(media.optional_now_playing_source.now_playing_source));
      if (text.has_value()) source = *text;
    }
    // Nothing playing: no stale title from the last song
    const bool active = play == state_text::MediaPlay::PLAYING || play == state_text::MediaPlay::PAUSED;
    state_manager_->update_media_text(active ? now_playing.title : "", active ? now_playing.artist : "",
                                      play == state_text::MediaPlay::OFF ? "" : source);
  }
}

void TeslaBLEVehicle::publish_media_off_() {
  if (media_player_ != nullptr && media_player_->state != media_player::MEDIA_PLAYER_STATE_OFF) {
    media_player_->state = media_player::MEDIA_PLAYER_STATE_OFF;
    media_player_->publish_state();
  }
  if (state_manager_)
    state_manager_->update_media_text("", "", "");
}

void TeslaBLEVehicle::send_media_command_(const char *name, int32_t action_tag, CarServer_MediaUpdateVolume volume) {
  if (state_manager_ && state_manager_->is_asleep()) {
    // Media controls are for someone in the car: never wake it for them
    ESP_LOGW(TAG, "[%s] %s: the car is asleep - not sending", log_name(), name);
    return;
  }
  ESP_LOGI(TAG, "[%s] %s requested", log_name(), name);
  send_command_with_tracking(
      UniversalMessage_Domain_DOMAIN_INFOTAINMENT, name,
      [action_tag, volume](TeslaBLE::Client *client, uint8_t *buff, size_t *len) {
        return client->build_car_server_vehicle_action_message(
            buff, len, action_tag,
            action_tag == CarServer_VehicleAction_mediaUpdateVolume_tag ? &volume : nullptr);
      },
      TeslaBLE::WakePolicy::NO_WAKE_SKIP,
      [this](bool succeeded) {
        // Read back what the car now plays (state, volume, title)
        if (succeeded) schedule_state_refresh_(ControlStateRefresh::MEDIA_STATE);
      });
}

void TeslaBLEVehicle::send_media_command_(const char *name, int32_t action_tag) {
  send_media_command_(name, action_tag, CarServer_MediaUpdateVolume_init_default);
}

void TeslaBLEVehicle::media_next_track() {
  send_media_command_("Media Next Track", CarServer_VehicleAction_mediaNextTrack_tag);
}

void TeslaBLEVehicle::media_previous_track() {
  send_media_command_("Media Previous Track", CarServer_VehicleAction_mediaPreviousTrack_tag);
}

void TeslaBLEVehicle::media_control(const media_player::MediaPlayerCall &call) {
  if (call.get_volume().has_value()) {
    CarServer_MediaUpdateVolume volume = CarServer_MediaUpdateVolume_init_default;
    volume.which_media_volume = CarServer_MediaUpdateVolume_volume_absolute_float_tag;
    volume.media_volume.volume_absolute_float =
        state_text::media_volume_absolute(*call.get_volume(), media_volume_max_);
    send_media_command_("Media Volume", CarServer_VehicleAction_mediaUpdateVolume_tag, volume);
  }
  if (!call.get_command().has_value())
    return;

  const bool playing = media_player_ != nullptr && media_player_->state == media_player::MEDIA_PLAYER_STATE_PLAYING;
  CarServer_MediaUpdateVolume step = CarServer_MediaUpdateVolume_init_default;
  step.which_media_volume = CarServer_MediaUpdateVolume_volume_delta_tag;
  switch (*call.get_command()) {
    // The car has one play/pause toggle: send it only when it changes something
    case media_player::MEDIA_PLAYER_COMMAND_PLAY:
      if (!playing) send_media_command_("Media Play", CarServer_VehicleAction_mediaPlayAction_tag);
      break;
    case media_player::MEDIA_PLAYER_COMMAND_PAUSE:
    case media_player::MEDIA_PLAYER_COMMAND_STOP:
      if (playing) send_media_command_("Media Pause", CarServer_VehicleAction_mediaPlayAction_tag);
      break;
    case media_player::MEDIA_PLAYER_COMMAND_TOGGLE:
      send_media_command_("Media Play/Pause", CarServer_VehicleAction_mediaPlayAction_tag);
      break;
    case media_player::MEDIA_PLAYER_COMMAND_NEXT:
      media_next_track();
      break;
    case media_player::MEDIA_PLAYER_COMMAND_PREVIOUS:
      media_previous_track();
      break;
    case media_player::MEDIA_PLAYER_COMMAND_VOLUME_UP:
      step.media_volume.volume_delta = 1;
      send_media_command_("Media Volume Up", CarServer_VehicleAction_mediaUpdateVolume_tag, step);
      break;
    case media_player::MEDIA_PLAYER_COMMAND_VOLUME_DOWN:
      step.media_volume.volume_delta = -1;
      send_media_command_("Media Volume Down", CarServer_VehicleAction_mediaUpdateVolume_tag, step);
      break;
    default:
      ESP_LOGW(TAG, "[%s] Media command %d is not supported by the car", log_name(),
               static_cast<int>(*call.get_command()));
      break;
  }
}

void TeslaBLEVehicle::set_cabin_overheat_protection(int mode) {
  if (mode < 0 || mode > 2) {
    ESP_LOGW(TAG, "Invalid cabin overheat protection mode: %d", mode);
    return;
  }

  CarServer_SetCabinOverheatProtectionAction action =
      CarServer_SetCabinOverheatProtectionAction_init_default;
  action.on = mode != 0;
  action.fan_only = mode == 2;

  const char *mode_name = mode == 0 ? "Off" : (mode == 1 ? "On" : "Fan Only");
  ESP_LOGI(TAG, "Cabin overheat protection %s requested", mode_name);

  send_command_with_tracking(
      UniversalMessage_Domain_DOMAIN_INFOTAINMENT,
      std::string("Cabin Overheat ") + mode_name,
      [action](TeslaBLE::Client *client, uint8_t *buff, size_t *len) {
        return client->build_car_server_vehicle_action_message(
            buff, len, CarServer_VehicleAction_setCabinOverheatProtectionAction_tag, &action);
      },
      TeslaBLE::WakePolicy::WAKE_IF_NEEDED,
      [this](bool succeeded) {
        if (succeeded) schedule_state_refresh_(ControlStateRefresh::CLIMATE_STATE);
      });
}

void TeslaBLEVehicle::set_steering_wheel_heat(bool enable) {
  ESP_LOGI(TAG, "Steering wheel heat %s requested", enable ? "ON" : "OFF");
  send_command_with_tracking(
      UniversalMessage_Domain_DOMAIN_INFOTAINMENT,
      enable ? "Steering Heat On" : "Steering Heat Off",
      [enable](TeslaBLE::Client *client, uint8_t *buff, size_t *len) {
        return client->build_car_server_vehicle_action_message(
            buff, len, CarServer_VehicleAction_hvacSteeringWheelHeaterAction_tag, &enable);
      }, TeslaBLE::WakePolicy::WAKE_IF_NEEDED,
      [this, enable](bool succeeded) {
        const auto decision = control_state_decision(ControlStateCommand::SET_STEERING_WHEEL_HEAT, succeeded);
        if (decision.publish_requested_state && state_manager_) state_manager_->update_steering_wheel_heat(enable);
        schedule_state_refresh_(decision.refresh);
      });
}

// =============================================================================
// Vehicle controls (Infotainment)
// =============================================================================

void TeslaBLEVehicle::flash_lights() {
  ESP_LOGI(TAG, "Flash lights requested");
  send_command_with_tracking(
      UniversalMessage_Domain_DOMAIN_INFOTAINMENT, "Flash Lights",
      [](TeslaBLE::Client *client, uint8_t *buff, size_t *len) {
        return client->build_car_server_vehicle_action_message(
            buff, len, CarServer_VehicleAction_vehicleControlFlashLightsAction_tag, nullptr);
      });
}

void TeslaBLEVehicle::honk_horn() {
  ESP_LOGI(TAG, "Honk horn requested");
  send_command_with_tracking(
      UniversalMessage_Domain_DOMAIN_INFOTAINMENT, "Honk Horn",
      [](TeslaBLE::Client *client, uint8_t *buff, size_t *len) {
        return client->build_car_server_vehicle_action_message(
            buff, len, CarServer_VehicleAction_vehicleControlHonkHornAction_tag, nullptr);
      });
}

void TeslaBLEVehicle::set_sentry_mode(bool enable) {
  if (enable && state_manager_ && !state_manager_->sentry_mode_available()) {
    ESP_LOGW(TAG, "[%s] Sentry mode is not available on this car right now - not sending", log_name());
    state_manager_->republish_sentry_mode();  // put the switch back
    return;
  }
  ESP_LOGI(TAG, "Sentry mode %s requested", enable ? "ON" : "OFF");
  send_command_with_tracking(
      UniversalMessage_Domain_DOMAIN_INFOTAINMENT,
      enable ? "Sentry On" : "Sentry Off",
      [enable](TeslaBLE::Client *client, uint8_t *buff, size_t *len) {
        return client->build_car_server_vehicle_action_message(
            buff, len, CarServer_VehicleAction_vehicleControlSetSentryModeAction_tag, &enable);
      }, TeslaBLE::WakePolicy::WAKE_IF_NEEDED,
      [this, enable](bool succeeded) {
        const auto decision = control_state_decision(ControlStateCommand::SET_SENTRY_MODE, succeeded);
        if (decision.publish_requested_state && state_manager_) state_manager_->update_sentry_mode(enable);
        schedule_state_refresh_(decision.refresh);
      });
}

void TeslaBLEVehicle::vent_windows() {
  ESP_LOGI(TAG, "Vent windows requested");
  int32_t window_action = 0;
  send_command_with_tracking(
      UniversalMessage_Domain_DOMAIN_INFOTAINMENT, "Vent Windows",
      [window_action](TeslaBLE::Client *client, uint8_t *buff, size_t *len) {
        return client->build_car_server_vehicle_action_message(
            buff, len, CarServer_VehicleAction_vehicleControlWindowAction_tag, &window_action);
      });
}

void TeslaBLEVehicle::close_windows() {
  ESP_LOGI(TAG, "Close windows requested");
  int32_t window_action = 1;
  send_command_with_tracking(
      UniversalMessage_Domain_DOMAIN_INFOTAINMENT, "Close Windows",
      [window_action](TeslaBLE::Client *client, uint8_t *buff, size_t *len) {
        return client->build_car_server_vehicle_action_message(
            buff, len, CarServer_VehicleAction_vehicleControlWindowAction_tag, &window_action);
      });
}

void TeslaBLEVehicle::update_ble_rssi(int8_t rssi) {
  if (state_manager_ != nullptr) state_manager_->update_ble_rssi(static_cast<float>(rssi));
}

uint32_t TeslaBLEVehicle::ble_write_gap_ms() const {
  // Small breather between fragments. 60 ms made a 188-byte request take
  // longer than the library's 1 s resend timer. Only one car is connected
  // at a time and congestion is handled per link by the adapter (status 143
  // + ESP_GATTC_CONGEST_EVT), so a short gap is enough.
  return 5;
}

bool TeslaBLEVehicle::is_connected() const {
  return ble_client_ != nullptr && ble_client_->state() == espbt::ClientState::ESTABLISHED;
}

void TeslaBLEClient::log_link_params_if_changed(const char *name) {
  esp_gap_conn_params_t params{};
  if (esp_ble_get_current_conn_params(this->get_remote_bda(), &params) != ESP_OK) return;
  if (params.interval == 0) return;
  if (params.interval == logged_interval_ && params.latency == logged_latency_ &&
      params.timeout == logged_timeout_)
    return;
  logged_interval_ = params.interval;
  logged_latency_ = params.latency;
  logged_timeout_ = params.timeout;
  // Units: interval 1.25 ms, timeout 10 ms.
  ESP_LOGD(TAG,
           "[%s] BLE link params: interval %.2f ms, latency %u, supervision timeout %u ms "
           "(requested %.2f ms / %u ms)",
           name, params.interval * 1.25f, (unsigned) params.latency, (unsigned) params.timeout * 10u,
           link_interval_units_ * 1.25f, (unsigned) link_timeout_units_ * 10u);
}

#ifdef USE_ESP32_BLE_DEVICE
bool TeslaBLEClient::parse_device(const espbt::ESPBTDevice &device) {
  // MAC discovery: the car is recognised by its VIN-derived advert name.
  if (vehicle_ != nullptr && device.address_uint64() != tesla_address_ &&
      vehicle_->advert_name_matches(device.get_name()))
    vehicle_->on_advert_name_seen(device.address_uint64());
  if (vehicle_ != nullptr && tesla_address_ != 0 && device.address_uint64() == tesla_address_)
    vehicle_->note_advert_seen(device.get_rssi());
  if (vehicle_ != nullptr && !vehicle_->link_turn_allows_connect())
    return false;
  return esp32_ble_client::BLEClientBase::parse_device(device);
}
#endif

void TeslaBLEClient::connect() {
  // ESP-IDF uses these for the connection it is about to open. Both cars get
  // the same interval by default, so the controller can interleave the two
  // links instead of starving one of them (seen as rsn 0x8 timeouts).
  this->set_conn_params_(link_interval_units_, link_interval_units_, 0, link_timeout_units_, "tesla");
  esp32_ble_client::BLEClientBase::connect();
}

bool TeslaBLEClient::gattc_event_handler(esp_gattc_cb_event_t event,
                                         esp_gatt_if_t gattc_if,
                                         esp_ble_gattc_cb_param_t *param) {
  if (!esp32_ble_client::BLEClientBase::gattc_event_handler(event, gattc_if, param))
    return false;
  if (event == ESP_GATTC_SEARCH_CMPL_EVT) {
    // Ask again once connected, in case the car negotiated something else.
    this->update_conn_params_(link_interval_units_, link_interval_units_, 0, link_timeout_units_, "tesla");
  } else if (event == ESP_GATTC_DISCONNECT_EVT) {
    logged_interval_ = logged_latency_ = logged_timeout_ = 0;
  }
  if (vehicle_ != nullptr)
    vehicle_->gattc_event_handler(event, gattc_if, param);
  return true;
}

void TeslaBLEClient::gap_event_handler(esp_gap_ble_cb_event_t event,
                                        esp_ble_gap_cb_param_t *param) {
  esp32_ble_client::BLEClientBase::gap_event_handler(event, param);
  if (vehicle_ == nullptr || event != ESP_GAP_BLE_READ_RSSI_COMPLETE_EVT) return;
  if (!this->check_addr(param->read_rssi_cmpl.remote_addr)) return;
  if (param->read_rssi_cmpl.status == ESP_BT_STATUS_SUCCESS) {
    vehicle_->update_ble_rssi(param->read_rssi_cmpl.rssi);
  }
}

// =============================================================================
// BLE event handling
// =============================================================================

void TeslaBLEVehicle::gattc_event_handler(esp_gattc_cb_event_t event,
                                          esp_gatt_if_t gattc_if,
                                          esp_ble_gattc_cb_param_t *param) {
  LogScope log_scope(this);
  ESP_LOGV(TAG, "[%s] GATTC event %d", log_name(), event);

  switch (event) {
  case ESP_GATTC_OPEN_EVT:
    if (param->open.status == ESP_GATT_OK) {
      ESP_LOGI(TAG, "[%s] BLE physical link established", log_name());
    }
    break;

  case ESP_GATTC_CLOSE_EVT:
    ESP_LOGW(TAG, "[%s] BLE connection closed", log_name());
    handle_connection_lost();
    break;

  case ESP_GATTC_DISCONNECT_EVT:
    ESP_LOGW(TAG, "[%s] BLE disconnected", log_name());
    this->read_handle_ = 0;
    this->write_handle_ = 0;
    notify_ready_ = false;
    notify_registration_pending_ = false;
    this->cancel_timeout("tesla-notify-retry");
    // Low-level client owns the connection state transition.
    break;

  case ESP_GATTC_SEARCH_CMPL_EVT: {
    auto *readChar = this->ble_client_->get_characteristic(this->service_uuid_,
                                                        this->read_uuid_);
    if (readChar == nullptr) {
      on_missing_characteristic_("Read");
      break;
    }
    this->read_handle_ = readChar->handle;

    notify_ready_ = false;
    notify_registration_pending_ = false;

    auto *writeChar = this->ble_client_->get_characteristic(this->service_uuid_,
                                                         this->write_uuid_);
    if (writeChar == nullptr) {
      on_missing_characteristic_("Write");
      break;
    }
    this->write_handle_ = writeChar->handle;
    register_notify_();
    break;
  }

  case ESP_GATTC_REG_FOR_NOTIFY_EVT:
    notify_registration_pending_ = false;
    if (param->reg_for_notify.status != ESP_GATT_OK) {
      ESP_LOGW(TAG, "Tesla notify registration failed: %d - retrying",
               param->reg_for_notify.status);
      schedule_notify_retry_();
      break;
    }

    notify_ready_ = true;
    ESP_LOGI(TAG, "[%s] Tesla notifications ready", log_name());
    handle_connection_established();
    break;

  case ESP_GATTC_NOTIFY_EVT: {
    if (param->notify.conn_id != this->ble_client_->get_conn_id())
      break;

    std::vector<unsigned char> data(
        param->notify.value, param->notify.value + param->notify.value_len);

    note_link_activity();
    if (vehicle_)
      vehicle_->on_rx_data(data);
    break;
  }

  case ESP_GATTC_WRITE_CHAR_EVT:
    if (ble_adapter_)
      ble_adapter_->on_write_complete(param->write.status);
    // 143 (congested) is not a failure: the fragment was accepted. The
    // adapter logs real failures itself.
    break;

  case ESP_GATTC_CONGEST_EVT:
    if (param->congest.conn_id != this->ble_client_->get_conn_id())
      break;
    if (ble_adapter_)
      ble_adapter_->on_congest_event(param->congest.congested);
    break;

  default:
    break;
  }
}

void TeslaBLEVehicle::on_missing_characteristic_(const char *which) {
  const auto state = ble_client_ != nullptr ? ble_client_->state() : espbt::ClientState::IDLE;
  if (state == espbt::ClientState::DISCONNECTING || state == espbt::ClientState::IDLE) {
    // Discovery result arriving while we are already disconnecting.
    ESP_LOGD(TAG, "[%s] %s characteristic not found (link closing)", log_name(), which);
    return;
  }
  // With the GATT cache enabled a stale cached service table looks exactly
  // like this. Drop this car's cache entry and reconnect to rediscover.
  ESP_LOGW(TAG, "[%s] %s characteristic not found - clearing GATT cache and reconnecting", log_name(),
           which);
  esp_ble_gattc_cache_clean(ble_client_->get_remote_bda());
  ble_client_->disconnect();
}

void TeslaBLEVehicle::schedule_notify_retry_() {
  this->set_timeout("tesla-notify-retry", NOTIFY_RETRY_MS, [this]() {
    register_notify_();
  });
}

void TeslaBLEVehicle::register_notify_() {
  if (notify_ready_ || notify_registration_pending_ || ble_client_ == nullptr ||
      read_handle_ == 0 || !is_connected()) {
    return;
  }

  const esp_err_t status = ble_client_->register_for_notify(read_handle_);
  if (status == ESP_OK) {
    notify_registration_pending_ = true;
    ESP_LOGD(TAG, "Tesla notify registration requested");
  } else {
    ESP_LOGW(TAG, "Tesla notify registration submit failed: %s - retrying",
             esp_err_to_name(status));
    schedule_notify_retry_();
  }
}

void TeslaBLEVehicle::handle_connection_established() {
  if (!notify_ready_) return;
  if (vehicle_ && !link_ready_) {
    link_ready_ = true;
    if (!vehicle_->is_connected()) {
      vehicle_->set_connected(true);
    } else {
      // Planned hand-over: the library kept its Tesla sessions, so no new
      // VCSEC / infotainment session handshake is needed this turn.
      ESP_LOGD(TAG, "[%s] Reusing Tesla sessions from the previous turn", log_name());
    }
    ever_ready_ = true;
    ready_this_turn_ = true;
    turn_requested_ = false;
    missed_turns_ = 0;
    retry_turn_after_ms_ = 0;
    note_link_activity();
    ESP_LOGI(TAG, "[%s] Connection established - polling VCSEC", log_name());
    // VCSEC never wakes the car. Infotainment waits for its answer (see the
    // vehicle status callback) so a sleeping car is not woken by connecting.
    vehicle_->vcsec_poll();
    last_vcsec_poll_ = millis();
    infotainment_check_pending_ = true;
    this->set_timeout("infotainment-check", INFOTAINMENT_CHECK_TIMEOUT_MS, [this]() {
      if (!infotainment_check_pending_) return;
      infotainment_check_pending_ = false;
      ESP_LOGD(TAG, "[%s] No VCSEC status after connecting - skipping infotainment this turn", log_name());
    });
    flush_pending_commands_();
    start_next_infotainment_work_();
  }

  this->status_clear_warning();
}

void TeslaBLEVehicle::handle_connection_lost() {
  link_ready_ = false;
  if (vehicle_) {
    if (yielding_link_) {
      // Planned hand-over: keep the library "connected" so its Tesla
      // sessions (VCSEC + infotainment) survive until this car's next turn.
      // Nothing is sent meanwhile - every send path checks link_ready().
      // Drop any partial frame from the old link.
      vehicle_->rx_buffer_.clear();
    } else {
      vehicle_->set_connected(false);
    }
  }
  if (ble_adapter_)
    ble_adapter_->clear_queues();

  cancel_command_warning([this]() {
    this->cancel_timeout(COMMAND_WARNING_TIMEOUT);
  });

  this->cancel_timeout("infotainment-batch");
  this->cancel_timeout("tesla-notify-retry");
  this->cancel_timeout("release-infotainment-slot");
  this->cancel_timeout("infotainment-check");
  infotainment_check_pending_ = false;
  notify_ready_ = false;
  notify_registration_pending_ = false;
  poll_batch_in_progress_ = false;
  poll_batch_remaining_ = 0;
  cancel_queued_infotainment_work_();
  release_infotainment_slot_();
  user_commands_in_flight_ = 0;
  // Poll timing survives a disconnect: with cars taking turns this happens
  // every turn, and resetting it would poll (and keep awake) on every turn.
  if (yielding_link_) {
    yielding_link_ = false;
    ESP_LOGD(TAG, "[%s] BLE link handed over", log_name());
    return;
  }
  this->status_set_warning("BLE connection lost");
}

// =============================================================================
// Button and Switch implementations
// =============================================================================
// Note: All button and switch implementations are now generated by macros
// in tesla_ble_vehicle.h (DEFINE_TESLA_BUTTON and DEFINE_TESLA_SWITCH)
// Only Number controls need explicit implementation due to validation logic

void TeslaChargingAmpsNumber::control(float value) {
  if (!parent_)
    return;

  float min_val = this->traits.get_min_value();
  float max_val = this->traits.get_max_value();

  if (value < min_val || value > max_val) {
    ESP_LOGW(TAG, "Charging amps value %.1f out of bounds", value);
    return;
  }

  parent_->set_charging_amps(static_cast<int>(value));
}

void TeslaChargingLimitNumber::control(float value) {
  if (!parent_)
    return;

  float min_val = this->traits.get_min_value();
  float max_val = this->traits.get_max_value();

  if (value < min_val || value > max_val) {
    ESP_LOGW(TAG, "Charging limit value %.1f out of bounds", value);
    return;
  }

  parent_->set_charging_limit(static_cast<int>(value));
}

// =============================================================================
// Lock implementations
// =============================================================================

void TeslaDoorsLock::control(const lock::LockCall &call) {
  if (!parent_)
    return;

  auto state = call.get_state();
  if (state.has_value()) {
    if (state.value() == lock::LOCK_STATE_LOCKED) {
      parent_->lock_vehicle();
      publish_state(lock::LOCK_STATE_LOCKING);
    } else if (state.value() == lock::LOCK_STATE_UNLOCKED) {
      parent_->unlock_vehicle();
      publish_state(lock::LOCK_STATE_UNLOCKING);
    }
  }
}

void TeslaChargePortLatchLock::control(const lock::LockCall &call) {
  if (!parent_)
    return;

  auto state = call.get_state();
  if (state.has_value()) {
    if (state.value() == lock::LOCK_STATE_LOCKED) {
      // Close charge port door (will also lock the latch)
      parent_->close_charge_port();
      publish_state(lock::LOCK_STATE_LOCKING);
    } else if (state.value() == lock::LOCK_STATE_UNLOCKED) {
      // Unlock the charge port latch (releases the cable)
      parent_->unlock_charge_port();
      publish_state(lock::LOCK_STATE_UNLOCKING);
    }
  }
}

// =============================================================================
// Cover implementations
// =============================================================================

cover::CoverTraits TeslaCoverBase::get_traits() {
  auto traits = cover::CoverTraits();
  traits.set_supports_position(false);
  traits.set_supports_tilt(false);
  traits.set_supports_stop(false);
  traits.set_is_assumed_state(false);
  return traits;
}

void TeslaTrunkCover::control(const cover::CoverCall &call) {
  if (!parent_)
    return;

  if (call.get_position().has_value()) {
    float pos = call.get_position().value();
    if (pos == cover::COVER_OPEN) {
      parent_->open_trunk();
    } else if (pos == cover::COVER_CLOSED) {
      parent_->close_trunk();
    }
  }
}

void TeslaFrunkCover::control(const cover::CoverCall &call) {
  if (!parent_)
    return;

  // Frunk can only be opened (no close command)
  if (call.get_position().has_value()) {
    float pos = call.get_position().value();
    if (pos == cover::COVER_OPEN) {
      parent_->open_frunk();
    }
    // Close is not supported for frunk
  }
}

void TeslaWindowsCover::control(const cover::CoverCall &call) {
  if (!parent_)
    return;

  if (call.get_position().has_value()) {
    float pos = call.get_position().value();
    if (pos == cover::COVER_OPEN) {
      parent_->vent_windows();
    } else if (pos == cover::COVER_CLOSED) {
      parent_->close_windows();
    }
  }
}

void TeslaChargePortDoorCover::control(const cover::CoverCall &call) {
  if (!parent_)
    return;

  if (call.get_position().has_value()) {
    float pos = call.get_position().value();
    if (pos == cover::COVER_OPEN) {
      parent_->open_charge_port();
    } else if (pos == cover::COVER_CLOSED) {
      parent_->close_charge_port();
    }
  }
}

// =============================================================================
// Climate implementation
// =============================================================================

TeslaClimate::TeslaClimate() {
  this->set_supported_custom_presets(
      {"Normal", "Defrost", "Keep On", "Dog Mode", "Camp Mode"});
  this->set_supported_custom_fan_modes({"Normal", "Bioweapon Mode"});
}

climate::ClimateTraits TeslaClimate::traits() {
  auto traits = climate::ClimateTraits();
  traits.set_supported_modes(
      {climate::CLIMATE_MODE_OFF, climate::CLIMATE_MODE_HEAT_COOL});
  // Use feature flags for current temperature support
  traits.add_feature_flags(climate::CLIMATE_SUPPORTS_CURRENT_TEMPERATURE);
  traits.set_visual_min_temperature(15.0f);
  traits.set_visual_max_temperature(28.0f);
  traits.set_visual_temperature_step(0.5f);
  return traits;
}

void TeslaClimate::control(const climate::ClimateCall &call) {
  if (!parent_)
    return;

  if (call.get_mode().has_value()) {
    auto mode = call.get_mode().value();
    if (mode == climate::CLIMATE_MODE_OFF) {
      parent_->set_climate_on(false);
    } else if (mode == climate::CLIMATE_MODE_HEAT_COOL) {
      parent_->set_climate_on(true);
    }
  }

  // Handle custom presets
  const auto custom = call.get_custom_preset();
  if (!custom.empty()) {
    if (custom == "Normal") {
      parent_->set_preconditioning_max(false);
      parent_->set_climate_keeper(0);
    } else if (custom == "Defrost") {
      parent_->set_preconditioning_max(true);
    } else if (custom == "Keep On") {
      parent_->set_preconditioning_max(false);
      parent_->set_climate_keeper(1);
    } else if (custom == "Dog Mode") {
      parent_->set_preconditioning_max(false);
      parent_->set_climate_keeper(2);
    } else if (custom == "Camp Mode") {
      parent_->set_preconditioning_max(false);
      parent_->set_climate_keeper(3);
    }
  }

  // Handle custom fan modes
  const auto fan = call.get_custom_fan_mode();
  if (!fan.empty()) {
    if (fan == "Normal") {
      parent_->set_bioweapon_mode(false);
    } else if (fan == "Bioweapon Mode") {
      parent_->set_bioweapon_mode(true);
    }
  }

  if (call.get_target_temperature().has_value()) {
    float temp = call.get_target_temperature().value();
    parent_->set_climate_temp(temp);
    this->target_temperature = temp;
  }

  this->publish_state();
}

void TeslaClimate::update_state(bool is_on, float current_temp,
                                float target_temp, const char *preset,
                                const char *fan_mode) {
  this->mode =
      is_on ? climate::CLIMATE_MODE_HEAT_COOL : climate::CLIMATE_MODE_OFF;

  if (!std::isnan(current_temp)) {
    this->current_temperature = current_temp;
  }

  if (!std::isnan(target_temp)) {
    this->target_temperature = target_temp;
  }

  if (preset != nullptr) {
    this->set_custom_preset_(preset);
  }

  if (fan_mode != nullptr) {
    this->set_custom_fan_mode_(fan_mode);
  }

  this->publish_state();
}

} // namespace tesla_ble_vehicle
} // namespace esphome
