#include <unity.h>
#include <map>
#include <vector>
#include "BatteryCharger.h"
using namespace BatteryCharger;
static std::map<uint8_t, uint8_t> regs;
static std::vector<std::pair<uint8_t, uint8_t>> writes;
static bool failWrites;
static bool readByte(uint8_t reg, uint8_t &value) {
  TEST_ASSERT_TRUE(reg == regVoltage || reg == regTimer);
  value = regs[reg];
  return true;
}
static bool writeByte(uint8_t reg, uint8_t value) {
  TEST_ASSERT_TRUE(reg == regVoltage || reg == regTimer);
  writes.push_back({reg, value});
  if(failWrites) return false;
  regs[reg] = value;
  return true;
}
void setUp() {
  regs = {{regVoltage, 0x5e}, {regTimer, 0x9d}}; // datasheet reset values
  writes.clear();
  failWrites = false;
}
void tearDown() {}
void test_codes_match_datasheet() {
  TEST_ASSERT_EQUAL(4208, millivolts(full));
  TEST_ASSERT_EQUAL(4128, millivolts(percent90));
  TEST_ASSERT_EQUAL(4064, millivolts(percent80));
  TEST_ASSERT_EQUAL(3968, millivolts(percent70));
  TEST_ASSERT_EQUAL(3840, millivolts(percent60)); // the charger's floor
  TEST_ASSERT_EQUAL(0, millivolts(limitCount));
  TEST_ASSERT_EQUAL_HEX8(0x5e & vregMask, vregCode[standard] << 2);
}
void test_standard_writes_nothing() {
  Limiter l;
  l.tick(0, readByte, writeByte);
  l.tick(60000, readByte, writeByte);
  TEST_ASSERT_EQUAL(0, writes.size());
  TEST_ASSERT_EQUAL(State::idle, l.state());
}
void test_limit_disables_watchdog_first_and_keeps_other_bits() {
  Limiter l;
  l.want(percent60);
  l.tick(0, readByte, writeByte);
  TEST_ASSERT_EQUAL(2, writes.size());
  TEST_ASSERT_EQUAL(regTimer, writes[0].first);
  TEST_ASSERT_EQUAL_HEX8(0x8d, writes[0].second);            // only WATCHDOG cleared
  TEST_ASSERT_EQUAL(regVoltage, writes[1].first);
  TEST_ASSERT_EQUAL_HEX8((0x00 << 2) | 0x02, writes[1].second); // BATLOWV/VRECHG kept
  TEST_ASSERT_EQUAL(State::applied, l.state());
}
void test_rechecks_and_reapplies_after_charger_reset() {
  Limiter l;
  l.want(percent90);
  l.tick(0, readByte, writeByte);
  writes.clear();
  l.tick(1000, readByte, writeByte);
  TEST_ASSERT_EQUAL(0, writes.size()); // not yet due
  regs = {{regVoltage, 0x5e}, {regTimer, 0x9d}}; // charger lost power
  l.tick(recheckMs, readByte, writeByte);
  TEST_ASSERT_EQUAL(2, writes.size());
  TEST_ASSERT_EQUAL_HEX8(0x12 << 2, regs[regVoltage] & vregMask);
}
void test_back_to_standard_restores_defaults() {
  Limiter l;
  l.want(percent60);
  l.tick(0, readByte, writeByte);
  l.want(standard);
  writes.clear();
  l.tick(10, readByte, writeByte);
  TEST_ASSERT_EQUAL_HEX8(0x5e, regs[regVoltage]);
  TEST_ASSERT_EQUAL_HEX8(0x9d, regs[regTimer]);
  TEST_ASSERT_EQUAL(State::idle, l.state());
  writes.clear();
  l.tick(100000, readByte, writeByte);
  TEST_ASSERT_EQUAL(0, writes.size());
}
void test_failed_write_reports_failure_without_spinning() {
  failWrites = true;
  Limiter l;
  l.want(percent60);
  l.tick(0, readByte, writeByte);
  TEST_ASSERT_EQUAL(State::failed, l.state());
  const auto count = writes.size();
  l.tick(5, readByte, writeByte);
  TEST_ASSERT_EQUAL(count, writes.size());
}
void test_no_bus() {
  Limiter l;
  l.want(percent90);
  l.tick(0, nullptr, nullptr);
  TEST_ASSERT_EQUAL(State::failed, l.state());
  l.want(9);
  TEST_ASSERT_EQUAL(percent90, l.limit());
}
int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_codes_match_datasheet);
  RUN_TEST(test_standard_writes_nothing);
  RUN_TEST(test_limit_disables_watchdog_first_and_keeps_other_bits);
  RUN_TEST(test_rechecks_and_reapplies_after_charger_reset);
  RUN_TEST(test_back_to_standard_restores_defaults);
  RUN_TEST(test_failed_write_reports_failure_without_spinning);
  RUN_TEST(test_no_bus);
  return UNITY_END();
}
