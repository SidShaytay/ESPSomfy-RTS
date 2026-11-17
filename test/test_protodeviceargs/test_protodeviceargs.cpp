#include <unity.h>
#include <ArduinoJson.h>
#include "ProtoDeviceArgs.h"
static const char *run(uint8_t type, const char *json, proto_device_args_t &args) {
  JsonDocument doc;
  deserializeJson(doc, json);
  return ProtoDeviceArgs::parse(type, doc.as<JsonObjectConst>(), args);
}
static const uint8_t update = uint8_t(proto_type_t::gitUpdate), battery = uint8_t(proto_type_t::battery);
void setUp() {}
void tearDown() {}
void test_update_actions() {
  proto_device_args_t a;
  TEST_ASSERT_NULL(run(update, "{}", a));
  TEST_ASSERT_EQUAL(0, a.action);
  TEST_ASSERT_NULL(run(update, "{\"u0\":1}", a));
  TEST_ASSERT_EQUAL(1, a.action);
  TEST_ASSERT_NULL(run(update, "{\"u0\":2,\"u1\":\"18446744073709551615\"}", a));
  TEST_ASSERT_EQUAL(2, a.action);
  TEST_ASSERT_TRUE(a.id == UINT64_MAX);
  TEST_ASSERT_NULL(run(update, "{\"u0\":3,\"u2\":\"9007199254740993\"}", a));
  TEST_ASSERT_TRUE(a.id == 9007199254740993ull);
}
void test_update_refusals() {
  proto_device_args_t a;
  const char *bad[] = {"{\"u0\":4}", "{\"u0\":-1}", "{\"u0\":\"1\"}", "{\"u0\":2}", "{\"u0\":3}",
    "{\"u0\":2,\"u1\":\"0\"}", "{\"u0\":2,\"u1\":\"01\"}", "{\"u0\":2,\"u1\":\"18446744073709551616\"}",
    "{\"u0\":2,\"u1\":12}", "{\"u0\":1,\"u1\":\"5\"}", "{\"u0\":3,\"u1\":\"5\"}", "{\"zz\":1}"};
  for(const char *json : bad) TEST_ASSERT_NOT_NULL_MESSAGE(run(update, json, a), json);
}
void test_battery() {
  proto_device_args_t a;
  TEST_ASSERT_NULL(run(battery, "{}", a));
  TEST_ASSERT_EQUAL(-1, a.value);
  TEST_ASSERT_NULL(run(battery, "{\"b4\":2}", a));
  TEST_ASSERT_EQUAL(2, a.value);
  TEST_ASSERT_NULL(run(battery, "{\"b4\":4}", a));
  TEST_ASSERT_NOT_NULL(run(battery, "{\"b4\":5}", a));
  TEST_ASSERT_NOT_NULL(run(battery, "{\"b4\":-1}", a));
  TEST_ASSERT_NOT_NULL(run(battery, "{\"b0\":1}", a));
}
void test_status_polls_do_not_mutate() {
  loop_item_t item;
  item.type = loop_item_type_t::proto_device;
  item.op = update;
  item.setDevice({0, 0, -1});
  TEST_ASSERT_FALSE(loopItemMutates(item));
  item.setDevice({0, 3, -1});
  TEST_ASSERT_FALSE(loopItemMutates(item));
  item.setDevice({0, 2, -1});
  TEST_ASSERT_TRUE(loopItemMutates(item));
  item.op = battery;
  item.setDevice({0, -1, -1});
  TEST_ASSERT_FALSE(loopItemMutates(item));
  item.setDevice({0, -1, 1});
  TEST_ASSERT_TRUE(loopItemMutates(item));
}
int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_update_actions);
  RUN_TEST(test_update_refusals);
  RUN_TEST(test_battery);
  RUN_TEST(test_status_polls_do_not_mutate);
  return UNITY_END();
}
