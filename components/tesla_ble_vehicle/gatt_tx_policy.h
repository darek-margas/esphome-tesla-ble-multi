#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>

namespace esphome {
namespace tesla_ble_vehicle {

// ESP-IDF status values (esp_gatt_status_t), duplicated so this header stays
// dependency-free for unit tests.
static constexpr int GATT_STATUS_OK = 0x00;
static constexpr int GATT_STATUS_CONGESTED = 0x8f;  // 143

enum class WriteOutcome { SENT, SENT_CONGESTED, FAILED };

// Map an ESP_GATTC_WRITE_CHAR_EVT status for a write-without-response.
//
// ESP_GATT_CONGESTED (143) is NOT a failure: Bluedroid returns it from
// attp_send_msg_to_l2cap() with the comment "ATT congested, message accepted".
// The fragment was handed to L2CAP and will go out; the stack is only asking
// the client to stop sending on this link until ESP_GATTC_CONGEST_EVT reports
// congested=false. Re-sending the fragment duplicates 18 bytes inside a
// length-prefixed Tesla frame and corrupts the message.
inline WriteOutcome classify_write_status(int status) {
  if (status == GATT_STATUS_OK)
    return WriteOutcome::SENT;
  if (status == GATT_STATUS_CONGESTED)
    return WriteOutcome::SENT_CONGESTED;
  return WriteOutcome::FAILED;
}

// Per-link congestion backpressure. A link is blocked from the moment the
// stack reports congestion until ESP_GATTC_CONGEST_EVT clears it. MAX_WAIT_MS
// is a safety net in case the clearing event is never delivered.
class CongestionGate {
 public:
  static constexpr uint32_t MAX_WAIT_MS = 1000;

  void on_congested(uint32_t now_ms) {
    if (!congested_)
      since_ms_ = now_ms;
    congested_ = true;
  }

  // Returns how long the link was congested, or 0 if it was not.
  uint32_t on_uncongested(uint32_t now_ms) {
    if (!congested_)
      return 0;
    congested_ = false;
    return now_ms - since_ms_;
  }

  bool congested() const { return congested_; }

  // True when writes on this link must wait. Unsigned arithmetic stays correct
  // across the millis() wraparound.
  bool blocked(uint32_t now_ms) const { return congested_ && now_ms - since_ms_ < MAX_WAIT_MS; }

  // True when the gate is still marked congested but the safety net expired.
  bool expired(uint32_t now_ms) const { return congested_ && now_ms - since_ms_ >= MAX_WAIT_MS; }

  void reset() { congested_ = false; }

 private:
  bool congested_{false};
  uint32_t since_ms_{0};
};

// Tracks how long each Tesla message takes to leave the ESP32, fragment by
// fragment. The tesla-ble library resends a command when no response arrives
// within 1 s of handing it to the adapter, so a message that needs longer than
// that to transmit is resent before the car has seen it.
class TxMessageTracker {
 public:
  // The library's TRANSPORT_RETRY_INTERVAL.
  static constexpr uint32_t LIBRARY_RESEND_MS = 1000;

  struct Queued {
    uint32_t seq;
    size_t fragments_ahead;  // unsent fragments of earlier messages
    size_t messages_ahead;
  };

  struct Completed {
    uint32_t seq;
    size_t bytes;
    size_t fragments;
    uint32_t duration_ms;  // from queueing to the last fragment accepted
    uint32_t congested;    // fragments accepted with status 143
    uint32_t dropped;      // fragments dropped after repeated failures
  };

  Queued on_message_queued(uint32_t now_ms, size_t bytes, size_t fragments) {
    Queued q{++next_seq_, unsent_fragments_(), messages_.size()};
    messages_.push_back(Message{q.seq, now_ms, bytes, fragments, fragments, 0, 0});
    return q;
  }

  // Head fragment left the queue. sent=false means it was dropped.
  // Returns true and fills out when that finished the head message.
  bool on_fragment_done(uint32_t now_ms, bool sent, bool congested, Completed *out) {
    if (messages_.empty())
      return false;
    Message &m = messages_.front();
    if (!sent)
      ++m.dropped;
    else if (congested)
      ++m.congested;
    if (m.remaining > 0)
      --m.remaining;
    if (m.remaining > 0)
      return false;
    if (out != nullptr)
      *out = Completed{m.seq, m.bytes, m.fragments, now_ms - m.queued_ms, m.congested, m.dropped};
    messages_.pop_front();
    return true;
  }

  size_t pending_messages() const { return messages_.size(); }
  void clear() { messages_.clear(); }

 private:
  struct Message {
    uint32_t seq;
    uint32_t queued_ms;
    size_t bytes;
    size_t fragments;
    size_t remaining;
    uint32_t congested;
    uint32_t dropped;
  };

  size_t unsent_fragments_() const {
    size_t n = 0;
    for (const auto &m : messages_)
      n += m.remaining;
    return n;
  }

  std::deque<Message> messages_;
  uint32_t next_seq_{0};
};

}  // namespace tesla_ble_vehicle
}  // namespace esphome
