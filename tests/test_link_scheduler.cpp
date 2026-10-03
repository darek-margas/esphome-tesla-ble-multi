// Behavioural tests for LinkScheduler (components/tesla_ble_vehicle/
// link_scheduler.h). No ESPHome or tesla-ble dependency:  make test-cpp.
//
// Two Tesla links at once starve the first-opened one on the ESP32, so the
// cars take turns. Scenarios covered:
//  - a single car keeps its link forever (old behaviour)
//  - the first waiting car gets the first turn
//  - the owner yields only when another car waits and its link went quiet
//  - a busy owner keeps the link, up to the hard cap
//  - the next car connects only after the previous link is fully down
//  - an owner that never becomes ready loses the turn after a timeout
//  - turns rotate round-robin

#include "test_helper.h"

#include "link_scheduler.h"

using namespace esphome::tesla_ble_vehicle;
using In = LinkScheduler::Input;

static std::vector<In> two(In a, In b) { return {a, b}; }

static In wanting() {
  In i;
  i.wants = true;
  return i;
}

static In ready_quiet(uint32_t last_activity) {
  In i;
  i.ready = true;
  i.link_idle = false;
  i.last_activity_ms = last_activity;
  return i;
}

static void test_single_car_never_yields() {
  LinkScheduler s(1);
  std::vector<In> in{wanting()};
  CHECK(s.tick(0, in) == LinkScheduler::NONE);
  CHECK(s.owner() == 0);
  CHECK(s.may_connect(0));
  in[0] = ready_quiet(0);
  for (uint32_t t = 1000; t < 300000; t += 1000)
    CHECK(s.tick(t, in) == LinkScheduler::NONE);
  CHECK(s.owner() == 0);
}

static void test_first_waiting_car_gets_first_turn() {
  LinkScheduler s(2);
  CHECK(s.tick(0, two(In{}, In{})) == LinkScheduler::NONE);
  CHECK(s.owner() == LinkScheduler::NONE);
  CHECK(!s.may_connect(0));
  CHECK(!s.may_connect(1));
  s.tick(10, two(In{}, wanting()));
  CHECK(s.owner() == 1);
  CHECK(s.may_connect(1));
  CHECK(!s.may_connect(0));
}

static void test_owner_keeps_link_when_nobody_waits() {
  LinkScheduler s(2);
  s.tick(0, two(wanting(), In{}));
  CHECK(s.owner() == 0);
  for (uint32_t t = 1000; t < 120000; t += 1000)
    CHECK(s.tick(t, two(ready_quiet(0), In{})) == LinkScheduler::NONE);
  CHECK(s.owner() == 0);
}

static void test_owner_yields_when_quiet_and_other_waits() {
  LinkScheduler s(2);
  s.tick(0, two(wanting(), wanting()));
  CHECK(s.owner() == 0);
  // Ready at t=2000, last traffic at 4500.
  s.tick(2000, two(ready_quiet(1900), wanting()));
  // Minimum turn not over yet (ready for < 3 s).
  CHECK(s.tick(4900, two(ready_quiet(4500), wanting())) == LinkScheduler::NONE);
  // Turn long enough but traffic too recent.
  CHECK(s.tick(5500, two(ready_quiet(4500), wanting())) == LinkScheduler::NONE);
  // Quiet for 1.5 s: yield.
  CHECK(s.tick(6000, two(ready_quiet(4500), wanting())) == 0);
  CHECK(s.owner() == LinkScheduler::NONE);
}

static void test_busy_owner_keeps_link_up_to_cap() {
  LinkScheduler s(2);
  s.tick(0, two(wanting(), wanting()));
  In busy = ready_quiet(0);
  busy.busy = true;
  s.tick(1000, two(busy, wanting()));
  CHECK(s.tick(30000, two(busy, wanting())) == LinkScheduler::NONE);
  CHECK(s.tick(60999, two(busy, wanting())) == LinkScheduler::NONE);
  CHECK(s.tick(61000, two(busy, wanting())) == 0);
}

static void test_next_car_waits_for_link_down() {
  LinkScheduler s(2);
  s.tick(0, two(wanting(), wanting()));
  s.tick(1000, two(ready_quiet(0), wanting()));
  CHECK(s.tick(5000, two(ready_quiet(0), wanting())) == 0);
  // Car 0 is still disconnecting: nobody may connect.
  CHECK(!s.may_connect(0));
  CHECK(!s.may_connect(1));
  CHECK(s.tick(5100, two(ready_quiet(0), wanting())) == LinkScheduler::NONE);
  CHECK(!s.may_connect(1));
  // Link down: car 1's turn.
  s.tick(5200, two(In{}, wanting()));
  CHECK(s.owner() == 1);
  CHECK(s.may_connect(1));
}

static void test_owner_that_never_connects_loses_turn() {
  LinkScheduler s(2);
  s.tick(0, two(wanting(), wanting()));
  CHECK(s.owner() == 0);
  In connecting;
  connecting.link_idle = false;
  CHECK(s.tick(29999, two(connecting, wanting())) == LinkScheduler::NONE);
  CHECK(s.tick(30000, two(connecting, wanting())) == 0);
}

static void test_unreachable_owner_without_link_hands_over_immediately() {
  LinkScheduler s(2);
  s.tick(0, two(wanting(), wanting()));
  // Never discovered: link stays idle, nothing to wait for.
  CHECK(s.tick(30000, two(wanting(), wanting())) == 0);
  s.tick(30001, two(wanting(), wanting()));
  CHECK(s.owner() == 1);
}

static void test_turns_rotate_round_robin() {
  LinkScheduler s(3);
  s.tick(0, {wanting(), wanting(), wanting()});
  CHECK(s.owner() == 0);
  s.tick(1000, {ready_quiet(0), wanting(), wanting()});
  CHECK(s.tick(5000, {ready_quiet(0), wanting(), wanting()}) == 0);
  s.tick(5001, {wanting(), wanting(), wanting()});
  CHECK(s.owner() == 1);
  s.tick(6000, {wanting(), ready_quiet(0), wanting()});
  CHECK(s.tick(10000, {wanting(), ready_quiet(0), wanting()}) == 1);
  s.tick(10001, {wanting(), wanting(), wanting()});
  CHECK(s.owner() == 2);
}

static void test_empty_scheduler_is_inert() {
  LinkScheduler s(0);
  CHECK(s.tick(0, {}) == LinkScheduler::NONE);
  CHECK(!s.may_connect(0));
}

int main() {
  test_single_car_never_yields();
  test_first_waiting_car_gets_first_turn();
  test_owner_keeps_link_when_nobody_waits();
  test_owner_yields_when_quiet_and_other_waits();
  test_busy_owner_keeps_link_up_to_cap();
  test_next_car_waits_for_link_down();
  test_owner_that_never_connects_loses_turn();
  test_unreachable_owner_without_link_hands_over_immediately();
  test_turns_rotate_round_robin();
  test_empty_scheduler_is_inert();
  return test_summary();
}
