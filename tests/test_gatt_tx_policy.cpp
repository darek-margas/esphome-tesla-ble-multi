// Behavioural tests for components/tesla_ble_vehicle/gatt_tx_policy.h.
// No ESPHome or tesla-ble dependency:  make test-cpp  (or just  make test).
//
// Scenarios covered:
//  - status 143 (ESP_GATT_CONGESTED) counts as sent, never as a failure
//  - a congested link waits for the clearing event, with a safety timeout
//  - per-message transmit time and congestion counts are reported
//  - a message queued behind an unsent one reports what is ahead of it

#include "test_helper.h"

#include "gatt_tx_policy.h"

using namespace esphome::tesla_ble_vehicle;

static void test_congested_status_means_sent() {
  CHECK(classify_write_status(0) == WriteOutcome::SENT);
  // "ATT congested, message accepted": resending it would duplicate bytes.
  CHECK(classify_write_status(143) == WriteOutcome::SENT_CONGESTED);
  CHECK(classify_write_status(133) == WriteOutcome::FAILED);
  CHECK(classify_write_status(0x87) == WriteOutcome::FAILED);
}

static void test_congested_link_waits_for_clear_event() {
  CongestionGate g;
  CHECK(!g.blocked(1000));
  g.on_congested(1000);
  CHECK(g.blocked(1000));
  CHECK(g.blocked(1500));
  CHECK(g.on_uncongested(1300) == 300);
  CHECK(!g.blocked(1300));
  CHECK(!g.congested());
  // Clearing a link that was not congested reports nothing.
  CHECK(g.on_uncongested(1400) == 0);
}

static void test_repeated_congestion_keeps_first_timestamp() {
  CongestionGate g;
  g.on_congested(1000);
  g.on_congested(1800);
  // Safety net counts from the first report, so a stream of 143s cannot
  // extend the pause forever.
  CHECK(g.blocked(1999));
  CHECK(!g.blocked(2000));
  CHECK(g.expired(2000));
}

static void test_congestion_safety_net() {
  CongestionGate g;
  g.on_congested(5000);
  CHECK(g.blocked(5000 + CongestionGate::MAX_WAIT_MS - 1));
  CHECK(!g.expired(5000 + CongestionGate::MAX_WAIT_MS - 1));
  CHECK(!g.blocked(5000 + CongestionGate::MAX_WAIT_MS));
  CHECK(g.expired(5000 + CongestionGate::MAX_WAIT_MS));
  g.reset();
  CHECK(!g.expired(9000));
}

static void test_congestion_gate_survives_millis_wraparound() {
  CongestionGate g;
  g.on_congested(0xFFFFFF00u);
  CHECK(g.blocked(0x00000010u));
  CHECK(!g.blocked(0xFFFFFF00u + CongestionGate::MAX_WAIT_MS));
}

static void test_message_timing() {
  TxMessageTracker t;
  auto q = t.on_message_queued(1000, 40, 3);
  CHECK(q.seq == 1);
  CHECK(q.messages_ahead == 0);
  CHECK(q.fragments_ahead == 0);

  TxMessageTracker::Completed done{};
  CHECK(!t.on_fragment_done(1100, true, false, &done));
  CHECK(!t.on_fragment_done(1200, true, true, &done));
  CHECK(t.on_fragment_done(1250, true, false, &done));
  CHECK(done.seq == 1);
  CHECK(done.bytes == 40);
  CHECK(done.fragments == 3);
  CHECK(done.duration_ms == 250);
  CHECK(done.congested == 1);
  CHECK(done.dropped == 0);
  CHECK(t.pending_messages() == 0);
}

static void test_message_queued_behind_unsent_one() {
  TxMessageTracker t;
  t.on_message_queued(1000, 150, 9);
  t.on_fragment_done(1070, true, false, nullptr);
  t.on_fragment_done(1140, true, false, nullptr);
  // The library resends after 1 s while 7 fragments are still unsent.
  auto q = t.on_message_queued(2000, 150, 9);
  CHECK(q.seq == 2);
  CHECK(q.messages_ahead == 1);
  CHECK(q.fragments_ahead == 7);

  TxMessageTracker::Completed done{};
  for (int i = 0; i < 7; ++i) t.on_fragment_done(2100, true, false, &done);
  CHECK(done.seq == 1);
  CHECK(done.duration_ms == 1100);
  CHECK(done.duration_ms >= TxMessageTracker::LIBRARY_RESEND_MS);
  CHECK(t.pending_messages() == 1);
}

static void test_dropped_fragment_is_reported() {
  TxMessageTracker t;
  t.on_message_queued(0, 30, 2);
  TxMessageTracker::Completed done{};
  t.on_fragment_done(10, false, false, &done);
  CHECK(t.on_fragment_done(20, true, false, &done));
  CHECK(done.dropped == 1);
}

static void test_clear_forgets_messages() {
  TxMessageTracker t;
  t.on_message_queued(0, 30, 2);
  t.clear();
  CHECK(t.pending_messages() == 0);
  CHECK(!t.on_fragment_done(10, true, false, nullptr));
  // Sequence numbers keep counting across reconnects.
  CHECK(t.on_message_queued(20, 10, 1).seq == 2);
}

int main() {
  test_congested_status_means_sent();
  test_congested_link_waits_for_clear_event();
  test_repeated_congestion_keeps_first_timestamp();
  test_congestion_safety_net();
  test_congestion_gate_survives_millis_wraparound();
  test_message_timing();
  test_message_queued_behind_unsent_one();
  test_dropped_fragment_is_reported();
  test_clear_forgets_messages();
  return test_summary();
}
