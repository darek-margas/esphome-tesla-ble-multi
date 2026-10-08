#include <string>

#include "key_status_policy.h"
#include "test_helper.h"

using namespace esphome::tesla_ble_vehicle;
using Status = KeyStatusPolicy::Status;
using Result = KeyStatusPolicy::Result;

static void test_boot() {
  KeyStatusPolicy no_key;
  no_key.on_boot(false);
  CHECK(no_key.status() == Status::NO_KEY);

  KeyStatusPolicy stored;
  stored.on_boot(true);
  CHECK(stored.status() == Status::NOT_VERIFIED);
  // Nothing the car says without an answer changes it.
  CHECK(!stored.on_result(Result::OTHER));
  CHECK(stored.status() == Status::NOT_VERIFIED);
}

static void test_verified_by_the_car() {
  KeyStatusPolicy p;
  p.on_boot(true);
  CHECK(p.on_result(Result::SUCCESS));
  CHECK(p.status() == Status::PAIRED);
  CHECK(!p.on_result(Result::SUCCESS));  // no change, nothing to publish

  // Key removed on the car (e.g. from the Locks screen).
  CHECK(p.on_result(Result::NOT_PAIRED));
  CHECK(p.status() == Status::NOT_PAIRED);
}

static void test_pairing_approved() {
  KeyStatusPolicy p(180000);
  p.on_boot(false);
  CHECK(p.on_pair_requested(1000));
  CHECK(p.status() == Status::WAITING);

  // Polls rejected while the user walks to the car: still waiting.
  CHECK(!p.on_result(Result::NOT_PAIRED));
  CHECK(p.status() == Status::WAITING);
  CHECK(!p.tick(60000));

  // Approved on the screen: the next authenticated poll succeeds.
  CHECK(p.on_result(Result::SUCCESS));
  CHECK(p.status() == Status::PAIRED);
  CHECK(!p.tick(500000));  // the window no longer matters
  CHECK(p.status() == Status::PAIRED);
}

static void test_pairing_never_approved() {
  KeyStatusPolicy p(180000);
  p.on_boot(false);
  p.on_pair_requested(1000);
  CHECK(!p.tick(180999));
  CHECK(p.tick(181000));
  CHECK(p.status() == Status::NOT_PAIRED);
}

static void test_pairing_window_across_millis_wrap() {
  KeyStatusPolicy p(180000);
  const uint32_t start = 0xFFFFFFFFu - 1000;
  p.on_pair_requested(start);
  CHECK(!p.tick(start + 179000));  // wraps past zero
  CHECK(p.tick(start + 180000));
}

static void test_regenerated_key() {
  KeyStatusPolicy p;
  p.on_boot(true);
  p.on_result(Result::SUCCESS);
  CHECK(p.on_key_regenerated());
  CHECK(p.status() == Status::NOT_PAIRED);
}

static void test_classify_error() {
  // Text of tesla-ble's CommandError::key_not_paired().
  CHECK(KeyStatusPolicy::classify_error("VCSEC key not on whitelist - pairing required") == Result::NOT_PAIRED);
  CHECK(KeyStatusPolicy::classify_error("INFOTAINMENT key not on whitelist - pairing required") ==
        Result::NOT_PAIRED);
  CHECK(KeyStatusPolicy::classify_error("Max retries exceeded for Charge State Poll") == Result::OTHER);
  CHECK(KeyStatusPolicy::classify_error("") == Result::OTHER);
}

static void test_text() {
  CHECK(std::string(KeyStatusPolicy::text(Status::NO_KEY)) == "No key");
  CHECK(std::string(KeyStatusPolicy::text(Status::NOT_VERIFIED)) == "Not verified");
  CHECK(std::string(KeyStatusPolicy::text(Status::WAITING)) == "Waiting for approval");
  CHECK(std::string(KeyStatusPolicy::text(Status::PAIRED)) == "Paired");
  CHECK(std::string(KeyStatusPolicy::text(Status::NOT_PAIRED)) == "Not paired");
}

int main() {
  test_boot();
  test_verified_by_the_car();
  test_pairing_approved();
  test_pairing_never_approved();
  test_pairing_window_across_millis_wrap();
  test_regenerated_key();
  test_classify_error();
  test_text();
  return test_summary();
}
