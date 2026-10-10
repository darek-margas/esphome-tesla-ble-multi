#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace esphome {
namespace tesla_ble_vehicle {

// Last heard (millis(), 0 = never) within window_ms of now; safe across the
// millis() wrap. Turns use a short window (a car that is away must not take
// the link); the Present sensor a longer one, so it does not flip to away
// while the scanner misses a few adverts during the other car's turn.
inline bool heard_within(uint32_t now, uint32_t last_heard_ms, uint32_t window_ms) {
  return last_heard_ms != 0 && now - last_heard_ms < window_ms;
}

// One Tesla BLE link at a time.
//
// On the original ESP32, two simultaneous Tesla connections starve each
// other: the first-opened link (HCI handle 0) stops being served and times
// out (rsn 0x8) while the other runs fine, whichever car holds it and
// whatever the connection parameters. Each car alone is reliable, so the
// cars take turns: the owner of the turn may connect; when another car has
// work waiting and the owner has gone quiet, the owner disconnects and the
// next waiting car (round-robin) gets the turn.
//
// With a single car configured nobody else ever waits, so the owner keeps
// its link forever - identical to the old behaviour.
//
// Pure logic with no ESPHome or tesla-ble dependency so it can be
// unit-tested in isolation (see tests/test_link_scheduler.cpp).
class LinkScheduler {
 public:
  // After the link is ready, the owner keeps it at least this long, so the
  // polls that start on connect get going before a yield is considered.
  static constexpr uint32_t MIN_TURN_MS = 2000;
  // The owner yields once its link has been quiet this long.
  static constexpr uint32_t IDLE_YIELD_MS = 1000;
  // Hard cap on a turn while another car waits (a long wake or retry loop
  // must not starve the other car).
  static constexpr uint32_t MAX_TURN_MS = 60000;
  // The owner gets this long to become ready (car out of range or asleep in
  // a way that never answers) before the turn moves on.
  static constexpr uint32_t CONNECT_TIMEOUT_MS = 30000;
  // A yielded link normally closes within seconds (ESPHome forces a link
  // stuck closing to idle after 10 s). Past this, see stalled_release().
  static constexpr uint32_t RELEASE_TIMEOUT_MS = 30000;

  struct Input {
    bool wants{false};      // has work waiting (poll due, queued command, never connected)
    bool ready{false};      // link up and the Tesla session layer connected
    bool busy{false};       // commands or polls in flight
    bool link_idle{true};   // BLE client fully disconnected (no connection, none in progress)
    uint32_t last_activity_ms{0};  // last BLE traffic on this link
  };

  static constexpr int NONE = -1;

  explicit LinkScheduler(size_t count = 0) { set_count(count); }

  void set_count(size_t count) {
    count_ = count;
    owner_ = NONE;
    releasing_ = NONE;
    last_owner_ = count == 0 ? NONE : static_cast<int>(count) - 1;
  }
  size_t count() const { return count_; }

  int owner() const { return owner_; }
  bool may_connect(int slot) const { return releasing_ == NONE && slot == owner_; }

  // The slot whose link has been closing for RELEASE_TIMEOUT_MS without going
  // down, or NONE. A disconnect requested while a connection is still opening
  // only takes effect when the stack reports the open; if that report never
  // comes, nobody else gets a turn, so the caller must recover the link (and
  // call release_recovery_started()).
  int stalled_release(uint32_t now_ms) const {
    if (releasing_ == NONE || now_ms - release_started_ms_ < RELEASE_TIMEOUT_MS)
      return NONE;
    return releasing_;
  }
  // Recovery was started: allow it another RELEASE_TIMEOUT_MS before
  // stalled_release() reports the slot again.
  void release_recovery_started(uint32_t now_ms) { release_started_ms_ = now_ms; }

  // Call regularly. inputs must hold count() entries. Returns the slot that
  // must disconnect now (its turn is over), or NONE.
  int tick(uint32_t now_ms, const std::vector<Input> &inputs) {
    if (count_ == 0 || inputs.size() < count_)
      return NONE;

    // Wait for the previous owner's link to be fully down before anyone
    // else connects: two links at once is exactly what we avoid.
    if (releasing_ != NONE) {
      if (!inputs[releasing_].link_idle)
        return NONE;
      releasing_ = NONE;
    }

    if (owner_ == NONE) {
      const int next = next_waiting_(inputs, last_owner_);
      if (next == NONE)
        return NONE;
      start_turn_(next, now_ms);
      return NONE;
    }

    const Input &own = inputs[owner_];
    if (own.ready && ready_since_ms_ == 0)
      ready_since_ms_ = now_ms == 0 ? 1 : now_ms;

    if (!others_waiting_(inputs))
      return NONE;

    bool yield = false;
    if (ready_since_ms_ == 0) {
      yield = now_ms - turn_start_ms_ >= CONNECT_TIMEOUT_MS;
    } else {
      const uint32_t held = now_ms - ready_since_ms_;
      const bool quiet = !own.busy && now_ms - own.last_activity_ms >= IDLE_YIELD_MS;
      yield = held >= MAX_TURN_MS || (held >= MIN_TURN_MS && quiet);
    }
    if (!yield)
      return NONE;

    const int released = owner_;
    last_owner_ = owner_;
    owner_ = NONE;
    releasing_ = inputs[released].link_idle ? NONE : released;
    release_started_ms_ = now_ms;
    return released;
  }

 private:
  void start_turn_(int slot, uint32_t now_ms) {
    owner_ = slot;
    turn_start_ms_ = now_ms;
    ready_since_ms_ = 0;
  }

  bool others_waiting_(const std::vector<Input> &inputs) const {
    for (size_t i = 0; i < count_; ++i) {
      if (static_cast<int>(i) != owner_ && inputs[i].wants)
        return true;
    }
    return false;
  }

  // Round-robin: the first waiting slot after `after`.
  int next_waiting_(const std::vector<Input> &inputs, int after) const {
    for (size_t step = 1; step <= count_; ++step) {
      const int slot = static_cast<int>((after + step) % count_);
      if (inputs[slot].wants)
        return slot;
    }
    return NONE;
  }

  size_t count_{0};
  int owner_{NONE};
  int releasing_{NONE};
  int last_owner_{NONE};
  uint32_t turn_start_ms_{0};
  uint32_t ready_since_ms_{0};
  uint32_t release_started_ms_{0};
};

}  // namespace tesla_ble_vehicle
}  // namespace esphome
