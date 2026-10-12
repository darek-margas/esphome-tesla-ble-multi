// Behavioural tests for the VIN -> advert name match (components/
// tesla_ble_vehicle/advert_name.h). No ESPHome or tesla-ble dependency:
// make test-cpp.
//
// Scenarios covered:
//  - the "S<16 hex>C" name of the car matches, in either case
//  - the car's other beacons (R/D/P) and other cars' names do not
//  - the 2023+ "Tesla <last 6 of VIN>" name matches, in either case
//  - a near miss (other VIN, wrong length, missing space) does not
//  - an invalid VIN matches nothing

#include "test_helper.h"

#include "advert_name.h"

using namespace esphome::tesla_ble_vehicle;

static const std::string VIN = "5YJ3E1EA7KF317000";
static const std::string HASH_NAME = "S1a87a5a75f3df858C";

static void test_hash_name_matches() {
  CHECK(advert_name_matches_vin("S1a87a5a75f3df858C", HASH_NAME, VIN));
  CHECK(advert_name_matches_vin("S1A87A5A75F3DF858C", HASH_NAME, VIN));
  CHECK(advert_name_matches_vin("s1a87a5a75f3df858c", HASH_NAME, VIN));
}

static void test_other_beacons_and_cars_do_not_match() {
  // Same car, other beacons: own MACs, not the one to connect to.
  CHECK(!advert_name_matches_vin("S1a87a5a75f3df858R", HASH_NAME, VIN));
  CHECK(!advert_name_matches_vin("S1a87a5a75f3df858D", HASH_NAME, VIN));
  CHECK(!advert_name_matches_vin("S1a87a5a75f3df858P", HASH_NAME, VIN));
  // Another car
  CHECK(!advert_name_matches_vin("S0000000000000000C", HASH_NAME, VIN));
  CHECK(!advert_name_matches_vin("S1a87a5a75f3df85C", HASH_NAME, VIN));
  CHECK(!advert_name_matches_vin("", HASH_NAME, VIN));
}

static void test_2023_name_matches() {
  CHECK(tesla_advert_name_2023(VIN) == "Tesla 317000");
  CHECK(advert_name_matches_vin("Tesla 317000", HASH_NAME, VIN));
  CHECK(advert_name_matches_vin("TESLA 317000", HASH_NAME, VIN));
  // Letters in the last 6 VIN characters, either case
  const std::string vin_letters = "7SAYGDEE5PA1AB2CD";
  CHECK(tesla_advert_name_2023(vin_letters) == "Tesla 1AB2CD");
  CHECK(advert_name_matches_vin("Tesla 1AB2CD", "", vin_letters));
  CHECK(advert_name_matches_vin("Tesla 1ab2cd", "", vin_letters));
}

static void test_2023_near_misses_do_not_match() {
  CHECK(!advert_name_matches_vin("Tesla 317001", HASH_NAME, VIN));
  CHECK(!advert_name_matches_vin("Tesla 31700", HASH_NAME, VIN));
  CHECK(!advert_name_matches_vin("Tesla 3170000", HASH_NAME, VIN));
  CHECK(!advert_name_matches_vin("Tesla317000", HASH_NAME, VIN));
  CHECK(!advert_name_matches_vin("Model 317000", HASH_NAME, VIN));
}

static void test_invalid_vin_matches_nothing() {
  CHECK(tesla_advert_name_2023("SHORTVIN").empty());
  CHECK(!advert_name_matches_vin("Tesla RTVIN", "", "SHORTVIN"));
  CHECK(!advert_name_matches_vin("Tesla ", "", ""));
}

int main() {
  test_hash_name_matches();
  test_other_beacons_and_cars_do_not_match();
  test_2023_name_matches();
  test_2023_near_misses_do_not_match();
  test_invalid_vin_matches_nothing();
  return test_summary();
}
