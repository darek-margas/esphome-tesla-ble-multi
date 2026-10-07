#pragma once

#include <cstdint>
#include <string>

namespace esphome {
namespace tesla_ble_vehicle {

// Whether this car accepts our BLE key, for the per-car "Key" diagnostic
// sensor. Pairing otherwise gives no feedback in Home Assistant: the request
// is sent, the user taps the card and confirms on the car screen, and the
// only sign of success used to be data appearing.
//
// Driven by:
//  - on_boot(key_stored): before the first answer from the car
//  - on_pair_requested() / on_key_regenerated(): user actions
//  - on_result(): the outcome of any authenticated command (polls included).
//    A success proves the car accepts the key; "key not on whitelist" proves
//    it does not; anything else (car asleep, link lost) says nothing.
//  - tick(): ends a pairing wait that was never approved
//
// Pure logic with no ESPHome or tesla-ble dependency so it can be
// unit-tested in isolation (see tests/test_key_status_policy.cpp).
class KeyStatusPolicy {
 public:
  enum class Status : uint8_t {
    NO_KEY,         // no private key stored for this car
    NOT_VERIFIED,   // key stored, the car has not answered since boot
    WAITING,        // pairing sent: tap the key card, confirm on the screen
    PAIRED,         // an authenticated command succeeded
    NOT_PAIRED,     // the car says the key is not on its whitelist
  };

  enum class Result : uint8_t {
    SUCCESS,       // authenticated command completed
    NOT_PAIRED,    // car answered KEY_NOT_ON_WHITELIST
    OTHER,         // failed or skipped for another reason (no information)
  };

  // How long a pairing request waits for the card + screen approval.
  explicit KeyStatusPolicy(uint32_t approval_window_ms = 180000) : approval_window_ms_(approval_window_ms) {}

  Status status() const { return status_; }

  // Returns true when the status changed (publish then).
  bool on_boot(bool key_stored) { return set_(key_stored ? Status::NOT_VERIFIED : Status::NO_KEY); }

  bool on_pair_requested(uint32_t now_ms) {
    waiting_since_ms_ = now_ms;
    return set_(Status::WAITING);
  }

  // A new key is not on any whitelist until it is paired.
  bool on_key_regenerated() { return set_(Status::NOT_PAIRED); }

  bool on_result(Result result) {
    switch (result) {
      case Result::SUCCESS:
        return set_(Status::PAIRED);
      case Result::NOT_PAIRED:
        // Until the user approves, the car keeps rejecting the key: that is
        // the expected answer while we wait, not a failure.
        if (status_ == Status::WAITING) return false;
        return set_(Status::NOT_PAIRED);
      case Result::OTHER:
        break;
    }
    return false;
  }

  bool tick(uint32_t now_ms) {
    if (status_ != Status::WAITING) return false;
    if (static_cast<uint32_t>(now_ms - waiting_since_ms_) < approval_window_ms_) return false;
    return set_(Status::NOT_PAIRED);
  }

  static const char *text(Status status) {
    switch (status) {
      case Status::NO_KEY:
        return "No key";
      case Status::NOT_VERIFIED:
        return "Not verified";
      case Status::WAITING:
        return "Waiting for approval";
      case Status::PAIRED:
        return "Paired";
      case Status::NOT_PAIRED:
        return "Not paired";
    }
    return "";
  }

  // tesla-ble reports a rejected key as CommandError::key_not_paired(), whose
  // message reads "<domain> key not on whitelist - pairing required". It has
  // no error code, so match the text.
  static Result classify_error(const std::string &message) {
    return message.find("not on whitelist") != std::string::npos ? Result::NOT_PAIRED : Result::OTHER;
  }

 private:
  bool set_(Status status) {
    if (status == status_) return false;
    status_ = status;
    return true;
  }

  uint32_t approval_window_ms_;
  uint32_t waiting_since_ms_{0};
  Status status_{Status::NOT_VERIFIED};
};

}  // namespace tesla_ble_vehicle
}  // namespace esphome
