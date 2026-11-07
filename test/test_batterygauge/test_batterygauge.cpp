#include <unity.h>
#include "BatteryGauge.h"
#include <vector>
using namespace BatteryGauge;
static uint16_t values[5];
static int failAt;
static std::vector<uint8_t> reads;
void setUp() {
  values[0] = 0x20; // INITCOMP
  values[1] = 0x08; // BATTPRES
  values[2] = 74;
  values[3] = 3920;
  values[4] = 0xff9c; // -100mA
  failAt = -1;
  reads.clear();
}
void tearDown() {}
static bool readWord(uint8_t reg, uint16_t &out) {
  const uint8_t allowed[] = {0x3a, 0x0a, 0x2c, 0x08, 0x0c};
  unsigned i = 0;
  while(i < 5 && allowed[i] != reg) ++i;
  TEST_ASSERT_LESS_THAN(5, i);
  reads.push_back(reg);
  if(int(i) == failAt) return false;
  out = values[i];
  return true;
}
static void cycle(Monitor &m, uint32_t start) {
  for(int i = 0; i < 5; ++i) m.tick(start + i * 10, readWord);
}
static void measured_percent_voltage_and_signed_current() {
  Monitor m;
  cycle(m, 100);
  TEST_ASSERT_EQUAL((int)State::ready, (int)m.reading().state);
  TEST_ASSERT_EQUAL(74, m.reading().percent);
  TEST_ASSERT_EQUAL(3920, m.reading().millivolts);
  TEST_ASSERT_EQUAL(-100, m.reading().milliamps);
  TEST_ASSERT_EQUAL(5, reads.size());
  m.tick(150, readWord);
  TEST_ASSERT_EQUAL(5, reads.size());
  values[2] = 100; values[4] = 120;
  cycle(m, 10200);
  TEST_ASSERT_EQUAL(100, m.reading().percent);
  TEST_ASSERT_EQUAL(120, m.reading().milliamps);
}
static void no_battery_uninitialized_and_invalid_are_not_percentages() {
  values[1] = 0;
  Monitor absent; cycle(absent, 0);
  TEST_ASSERT_EQUAL((int)State::noBattery, (int)absent.reading().state);
  values[1] = 8; values[0] = 0;
  Monitor starting; cycle(starting, 0);
  TEST_ASSERT_EQUAL((int)State::initializing, (int)starting.reading().state);
  values[0] = 0x420; // Gauge configuration mode suspends gauging.
  Monitor configuring; cycle(configuring, 0);
  TEST_ASSERT_EQUAL((int)State::initializing, (int)configuring.reading().state);
  values[0] = 0x20; values[2] = 101;
  Monitor invalid; cycle(invalid, 0);
  TEST_ASSERT_EQUAL((int)State::invalid, (int)invalid.reading().state);
  values[2] = 0; values[3] = 3000;
  Monitor empty; cycle(empty, 0);
  TEST_ASSERT_EQUAL((int)State::ready, (int)empty.reading().state);
  TEST_ASSERT_EQUAL(0, empty.reading().percent);
}
static void failure_clears_stale_data_and_retries_without_busy_polling() {
  Monitor m; cycle(m, 100);
  failAt = 0;
  m.tick(10200, readWord);
  TEST_ASSERT_EQUAL((int)State::readFailed, (int)m.reading().state);
  TEST_ASSERT_EQUAL(0, m.reading().percent);
  const auto count = reads.size();
  m.tick(10201, readWord);
  TEST_ASSERT_EQUAL(count, reads.size());
  failAt = -1;
  cycle(m, 20300);
  TEST_ASSERT_EQUAL((int)State::ready, (int)m.reading().state);
}
struct FakeWire {
  unsigned writes = 0, requests = 0, readIndex = 0;
  uint8_t pointer = 0;
  uint8_t status = 0, count = 2;
  bool writeOK = true, stopped = false;
  void beginTransmission(uint8_t address) { TEST_ASSERT_EQUAL_HEX8(0x55, address); }
  size_t write(uint8_t reg) { ++writes; pointer = reg; return writeOK ? 1 : 0; }
  uint8_t endTransmission(bool stop) { stopped = stop; return status; }
  uint8_t requestFrom(uint8_t address, uint8_t requested, bool stop) {
    TEST_ASSERT_EQUAL_HEX8(0x55, address);
    TEST_ASSERT_EQUAL(2, requested);
    TEST_ASSERT_TRUE(stop);
    ++requests;
    return count;
  }
  int available() const { return count; }
  int read() { return readIndex++ ? 0x0f : 0x50; }
};
static void wire_reads_are_little_endian_and_never_write_register_data() {
  FakeWire bus;
  uint16_t value = 0;
  TEST_ASSERT_TRUE(BatteryGauge::readWord(bus, 0x08, value));
  TEST_ASSERT_EQUAL(3920, value);
  TEST_ASSERT_EQUAL(1, bus.writes);
  TEST_ASSERT_FALSE(bus.stopped);
  TEST_ASSERT_EQUAL_HEX8(0x08, bus.pointer);
  FakeWire noBuffer; noBuffer.writeOK = false;
  TEST_ASSERT_FALSE(BatteryGauge::readWord(noBuffer, 0x2c, value));
  TEST_ASSERT_TRUE(noBuffer.stopped);
  TEST_ASSERT_EQUAL(0, noBuffer.requests);
  FakeWire nack; nack.status = 2;
  TEST_ASSERT_FALSE(BatteryGauge::readWord(nack, 0x2c, value));
  TEST_ASSERT_EQUAL(0, nack.requests);
  FakeWire shortRead; shortRead.count = 1;
  TEST_ASSERT_FALSE(BatteryGauge::readWord(shortRead, 0x2c, value));
  TEST_ASSERT_EQUAL(0, shortRead.readIndex);
}
static void polling_is_spread_across_ticks_and_survives_clock_wrap() {
  Monitor m;
  const uint32_t start = UINT32_MAX - 15;
  m.tick(start, readWord);
  m.tick(start + 1, readWord);
  TEST_ASSERT_EQUAL(1, reads.size());
  for(int i = 1; i < 5; ++i) m.tick(start + i * 10, readWord);
  TEST_ASSERT_EQUAL((int)State::ready, (int)m.reading().state);
  TEST_ASSERT_EQUAL(5, reads.size());
}
int main() {
  UNITY_BEGIN();
  RUN_TEST(wire_reads_are_little_endian_and_never_write_register_data);
  RUN_TEST(polling_is_spread_across_ticks_and_survives_clock_wrap);
  RUN_TEST(measured_percent_voltage_and_signed_current);
  RUN_TEST(no_battery_uninitialized_and_invalid_are_not_percentages);
  RUN_TEST(failure_clears_stale_data_and_retries_without_busy_polling);
  return UNITY_END();
}
