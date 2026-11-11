#include "LocalJsonAllocator.h"
#include "LocalWifiScan.h"
#include <stdlib.h>
#include <string.h>
#include <unity.h>
using namespace LocalUi;
struct State : WifiScan::Backend {
  SecuritySnapshot auth;
  WifiScanFeed source;
  uint64_t time = 0, revision = 1;
  unsigned touches = 0;
  State() {
    auth.allowed = true;
    auth.generation = auth.configurationGeneration = 1;
  }
  SecuritySnapshot authorization() override { return auth; }
  uint64_t networkRevision() override { return revision; }
  uint64_t now() override { return time; }
  void activity() override { ++touches; }
  WifiScanFeed &feed() override { return source; }
};
struct Reader : WifiScanReader {
  uint16_t total = 18;
  WifiScanNetwork row;
  mutable unsigned reads = 0;
  Reader() {
    strcpy(row.ssid, "network");
    row.bssid[5] = 7;
  }
  uint16_t count() const override { return total; }
  bool read(uint16_t i, WifiScanNetwork &out) const override {
    ++reads;
    out = row;
    out.bssid[5] = i;
    return true;
  }
};
void setUp() {}
void tearDown() {}
static void explicit_start_default_cancel_results_review_only_handoff() {
  State b;
  JsonAllocator memory(4096, malloc, free);
  WifiScan::Form f(b, memory);
  EntityAdmin::FormView v;
  TEST_ASSERT_TRUE(f.open(0));
  f.render(v);
  TEST_ASSERT_FALSE(b.source.pending());
  TEST_ASSERT_EQUAL(0, v.selected);
  f.activate(1);
  TEST_ASSERT_TRUE(f.closed());
  TEST_ASSERT_EQUAL(0, memory.used());
  f.open(2);
  f.turn(1, 3);
  f.activate(4);
  TEST_ASSERT_TRUE(b.source.pending());
  const auto token = b.source.token();
  Reader reader;
  b.time = 5;
  b.source.finish(token, true, reader);
  TEST_ASSERT_EQUAL(16, reader.reads);
  strcpy(reader.row.ssid, "changed SDK buffer");
  f.render(v);
  TEST_ASSERT_EQUAL(17, v.total);
  TEST_ASSERT_NOT_NULL(strstr(v.hint, "16"));
  f.activate(6);
  f.render(v);
  TEST_ASSERT_EQUAL(0, v.selected);
  TEST_ASSERT_EQUAL_STRING("network", v.rows[2].value);
  TEST_ASSERT_NOT_NULL(strstr(v.rows[2].label, "00:00:00:00:00:00"));
  char ssid[33];
  SecuritySnapshot auth;
  uint64_t revision;
  TEST_ASSERT_FALSE(f.takeSSID(ssid, 7, auth, revision));
  f.turn(1, 8);
  f.activate(9);
  TEST_ASSERT_TRUE(f.takeSSID(ssid, 10, auth, revision));
  TEST_ASSERT_EQUAL_STRING("network", ssid);
  TEST_ASSERT_EQUAL(1, revision);
  TEST_ASSERT_FALSE(f.takeSSID(ssid, 11, auth, revision));
  f.discard();
  TEST_ASSERT_EQUAL(0, memory.used());
}
static void pending_timeout_cancel_and_stale_callback_never_copy_results() {
  State b;
  JsonAllocator memory(4096, malloc, free);
  WifiScan::Form f(b, memory);
  f.open(0);
  f.turn(1, 1);
  f.activate(2);
  auto token = b.source.token();
  f.tick(30002);
  TEST_ASSERT_TRUE(f.closed());
  TEST_ASSERT_FALSE(b.source.pending());
  TEST_ASSERT_EQUAL(0, memory.used());
  Reader reader;
  b.time = 30003;
  b.source.finish(token, true, reader);
  TEST_ASSERT_TRUE(f.closed());
  f.open(30004);
  f.turn(1, 30005);
  f.activate(30006);
  token = b.source.token();
  f.back(30007);
  TEST_ASSERT_FALSE(b.source.pending());
  b.source.finish(token, true, reader);
  TEST_ASSERT_EQUAL(0, memory.used());
}
static void identity_network_clock_allocation_and_hidden_network_fail_closed() {
  State b;
  JsonAllocator memory(4096, malloc, free);
  WifiScan::Form f(b, memory);
  EntityAdmin::FormView v;
  f.open(0);
  ++b.revision;
  f.tick(1);
  TEST_ASSERT_TRUE(f.closed());
  TEST_ASSERT_EQUAL(0, memory.used());
  f.open(2);
  ++b.auth.generation;
  f.tick(3);
  TEST_ASSERT_TRUE(f.closed());
  f.open(4);
  f.tick(3);
  TEST_ASSERT_TRUE(f.closed());
  JsonAllocator noMemory(1, malloc, free);
  WifiScan::Form refused(b, noMemory);
  TEST_ASSERT_FALSE(refused.open(5));
  f.open(6);
  f.turn(1, 7);
  f.activate(8);
  Reader reader;
  reader.total = 1;
  reader.row.ssid[0] = 0;
  b.time = 9;
  b.source.finish(b.source.token(), true, reader);
  f.activate(10);
  f.render(v);
  TEST_ASSERT_NOT_NULL(strstr(v.hint, "manual"));
  char ssid[33];
  SecuritySnapshot auth;
  uint64_t revision;
  TEST_ASSERT_FALSE(f.takeSSID(ssid, 11, auth, revision));
  f.back(12);
  f.turn(1, 13);
  f.activate(14);
  TEST_ASSERT_TRUE(f.takeSSID(ssid, 15, auth, revision));
  TEST_ASSERT_EQUAL_STRING("", ssid);
}
static void busy_receipt_loss_and_late_completion_never_retain_listener() {
  State b;
  JsonAllocator memory(4096, malloc, free);
  WifiScan::Form f(b, memory);
  auto ignored = [](void *, uint64_t, bool, const WifiScanReader &) {};
  auto foreign = b.source.request(ignored, &b);
  f.open(0);
  f.turn(1, 1);
  f.activate(2);
  TEST_ASSERT_EQUAL(foreign, b.source.token());
  f.discard();
  TEST_ASSERT_TRUE(b.source.pending());
  b.source.cancel(foreign);
  f.open(3);
  f.turn(1, 4);
  f.activate(5);
  auto token = b.source.token();
  b.source.cancel(token);
  f.tick(6);
  TEST_ASSERT_TRUE(f.closed());
  TEST_ASSERT_EQUAL(0, memory.used());
  f.open(7);
  f.turn(1, 8);
  f.activate(9);
  token = b.source.token();
  b.time = 30009;
  Reader reader;
  b.source.finish(token, true, reader);
  TEST_ASSERT_TRUE(f.closed());
  TEST_ASSERT_EQUAL(0, reader.reads);
  TEST_ASSERT_EQUAL(0, memory.used());
  f.open(30010);
  f.turn(1, 30011);
  f.activate(30012);
  token = b.source.token();
  b.time = 30013;
  memset(reader.row.ssid, 'X', sizeof(reader.row.ssid));
  b.source.finish(token, true, reader);
  TEST_ASSERT_TRUE(f.closed());
  TEST_ASSERT_EQUAL(0, memory.used());
}
static void review_preserves_channel_rssi_and_honest_security_metadata() {
  State b;
  JsonAllocator memory(4096, malloc, free);
  WifiScan::Form f(b, memory);
  Reader reader;
  reader.total = 1;
  const int16_t strengths[] = {-32, -96, -32768};
  const uint8_t channels[] = {1, 14, 255}, auth[] = {0, 3, 255};
  const char *expected[] = {"Ch1 -32dBm Open", "Ch14 -96dBm Secured",
                            "Ch255 -32768dBm Unknown"};
  for (unsigned i = 0; i < 3; i++) {
    const uint64_t now = i * 10;
    f.open(now);
    f.turn(1, now + 1);
    f.activate(now + 2);
    reader.row.rssi = strengths[i];
    reader.row.channel = channels[i];
    reader.row.authentication = auth[i];
    b.time = now + 3;
    b.source.finish(b.source.token(), true, reader);
    f.activate(now + 4);
    EntityAdmin::FormView view;
    f.render(view);
    TEST_ASSERT_EQUAL_STRING(expected[i], view.rows[0].value);
    TEST_ASSERT_NOT_NULL(strstr(view.rows[2].label, "00:00:00:00:00:00"));
    f.discard();
  }
}
static void list_prioritizes_ssid_and_pages_four_readable_rows() {
  State b;JsonAllocator memory(4096,malloc,free);WifiScan::Form f(b,memory);Reader reader;
  reader.row.rssi=-55;reader.row.authentication=3;
  f.open(0);f.turn(1,1);f.activate(2);b.time=3;b.source.finish(b.source.token(),true,reader);
  EntityAdmin::FormView v;f.render(v);
  TEST_ASSERT_EQUAL(4,v.rowCount);TEST_ASSERT_EQUAL_STRING("network",v.rows[0].value);
  TEST_ASSERT_EQUAL_STRING("-55dBm Secured",v.rows[0].label);
  TEST_ASSERT_NULL(strstr(v.rows[0].value,"00:00"));
  for(unsigned i=0;i<4;i++)f.turn(1,4+i);
  f.render(v);TEST_ASSERT_EQUAL(4,v.offset);TEST_ASSERT_EQUAL(4,v.rowCount);TEST_ASSERT_TRUE(v.rows[0].selected);
  f.activate(8);f.render(v);TEST_ASSERT_EQUAL_STRING("network",v.rows[2].value);
  TEST_ASSERT_NOT_NULL(strstr(v.rows[2].label,"00:00:00:00:00:04"));
}
static void list_labels_hidden_and_manual_choices_honestly() {
  State b;JsonAllocator memory(4096,malloc,free);WifiScan::Form f(b,memory);Reader reader;reader.total=1;
  reader.row.ssid[0]=0;reader.row.authentication=255;reader.row.rssi=-96;
  f.open(0);f.turn(1,1);f.activate(2);b.time=3;b.source.finish(b.source.token(),true,reader);
  EntityAdmin::FormView v;f.render(v);
  TEST_ASSERT_EQUAL_STRING("Hidden network",v.rows[0].value);
  TEST_ASSERT_EQUAL_STRING("-96dBm Unknown",v.rows[0].label);
  TEST_ASSERT_EQUAL_STRING("Enter SSID manually",v.rows[1].label);
  TEST_ASSERT_EQUAL_STRING("",v.rows[1].value);
}
static void full_length_ssid_survives_list_review_and_handoff() {
  State b;JsonAllocator memory(4096,malloc,free);WifiScan::Form f(b,memory);Reader reader;reader.total=1;
  strcpy(reader.row.ssid,"12345678901234567890123456789012");
  f.open(0);f.turn(1,1);f.activate(2);b.time=3;b.source.finish(b.source.token(),true,reader);
  EntityAdmin::FormView v;f.render(v);TEST_ASSERT_EQUAL_STRING(reader.row.ssid,v.rows[0].value);
  f.activate(4);f.render(v);TEST_ASSERT_EQUAL_STRING(reader.row.ssid,v.rows[2].value);
  f.turn(1,5);f.activate(6);char ssid[33];SecuritySnapshot auth;uint64_t revision;
  TEST_ASSERT_TRUE(f.takeSSID(ssid,7,auth,revision));TEST_ASSERT_EQUAL_STRING(reader.row.ssid,ssid);
}
int main() {
  UNITY_BEGIN();
  RUN_TEST(full_length_ssid_survives_list_review_and_handoff);
  RUN_TEST(list_prioritizes_ssid_and_pages_four_readable_rows);
  RUN_TEST(list_labels_hidden_and_manual_choices_honestly);
  RUN_TEST(review_preserves_channel_rssi_and_honest_security_metadata);
  RUN_TEST(busy_receipt_loss_and_late_completion_never_retain_listener);
  RUN_TEST(explicit_start_default_cancel_results_review_only_handoff);
  RUN_TEST(pending_timeout_cancel_and_stale_callback_never_copy_results);
  RUN_TEST(identity_network_clock_allocation_and_hidden_network_fail_closed);
  return UNITY_END();
}
