#include <unity.h>
#include <initializer_list>
#include "LoopQueue.h"
#include "ProtoSchema.h"

static loop_item_t request(shade_request_type_t type) {
  loop_item_t item;
  item.request.type = type;
  return item;
}
static loop_item_t item(loop_item_type_t type) {
  loop_item_t item;
  item.type = type;
  return item;
}

static const shade_request_type_t RADIO_REQUESTS[] = {
  shade_request_type_t::shade_command, shade_request_type_t::group_command, shade_request_type_t::tilt_command,
  shade_request_type_t::shade_repeat, shade_request_type_t::group_repeat, shade_request_type_t::shade_sensor,
  shade_request_type_t::group_sensor, shade_request_type_t::set_favorite, shade_request_type_t::unpair
};
static const shade_request_type_t OFF_RADIO_REQUESTS[] = {
  shade_request_type_t::set_positions, shade_request_type_t::set_rolling_code
};
static const loop_item_type_t OTHER_ITEMS[] = {
  loop_item_type_t::restore_shades, loop_item_type_t::model_edit, loop_item_type_t::model_commit, loop_item_type_t::proto_read,
  // A frequency scan's end among them, so it never waits for the radio.
  loop_item_type_t::proto_device
};

void setUp() {}
void tearDown() {}

static void test_only_position_and_rolling_code_requests_stay_off_the_radio() {
  int offRadio = 0;
  for(uint8_t t = 0; t <= static_cast<uint8_t>(shade_request_type_t::set_rolling_code); t++) {
    if(!shadeRequestUsesRadio(static_cast<shade_request_type_t>(t))) offRadio++;
  }
  TEST_ASSERT_EQUAL_INT(2, offRadio);
  for(shade_request_type_t t : OFF_RADIO_REQUESTS) TEST_ASSERT_FALSE(loopItemUsesRadio(request(t)));
  for(loop_item_type_t t : OTHER_ITEMS) TEST_ASSERT_FALSE(loopItemUsesRadio(item(t)));
}
static void test_off_radio_items_are_never_held() {
  // Nothing goes on air, so a full or suspended radio doesn't matter.
  for(shade_request_type_t t : OFF_RADIO_REQUESTS) {
    TEST_ASSERT_FALSE(holdLoopItem(request(t), false, false));
    TEST_ASSERT_FALSE(holdLoopItem(request(t), true, false));
  }
  for(loop_item_type_t t : OTHER_ITEMS) {
    TEST_ASSERT_FALSE(holdLoopItem(item(t), false, false));
    TEST_ASSERT_FALSE(holdLoopItem(item(t), true, false));
  }
}
static void test_radio_request_waits_for_room() {
  for(shade_request_type_t t : RADIO_REQUESTS) {
    TEST_ASSERT_TRUE(holdLoopItem(request(t), false, false));
    TEST_ASSERT_FALSE(holdLoopItem(request(t), false, true));
  }
}
static void test_requests_are_not_held_while_suspended() {
  // Nothing goes on air during a frequency scan or OTA anyway.
  for(shade_request_type_t t : RADIO_REQUESTS) {
    if(t == shade_request_type_t::unpair) continue;
    TEST_ASSERT_FALSE(holdLoopItem(request(t), true, false));
    TEST_ASSERT_FALSE(holdLoopItem(request(t), true, true));
  }
}
static void test_unpair_waits_out_a_suspend() {
  // The client was already told the shade is unpaired.
  TEST_ASSERT_TRUE(holdLoopItem(request(shade_request_type_t::unpair), true, false));
  TEST_ASSERT_TRUE(holdLoopItem(request(shade_request_type_t::unpair), true, true));
}

static void test_local_markers_are_small_and_use_the_exact_protocol_flag() {
  static_assert(sizeof(loop_item_t)<=80,"No target context in 32 queue items");
  auto local=item(loop_item_type_t::local_control);local.localToken=9;local.localRadio=true;
  TEST_ASSERT_TRUE(holdLoopItem(local,false,false));TEST_ASSERT_FALSE(holdLoopItem(local,false,true));
  TEST_ASSERT_FALSE(holdLoopItem(local,true,false));
  local.localRadio=false;TEST_ASSERT_FALSE(loopItemUsesRadio(local));TEST_ASSERT_FALSE(holdLoopItem(local,false,false));
  local.localRadio=true;local.localToken=0;TEST_ASSERT_FALSE(loopItemUsesRadio(local));
}
static void only_accepted_mutating_operations_invalidate_restart_history() {
  for(auto type:{loop_item_type_t::shade_request,loop_item_type_t::restore_shades,
      loop_item_type_t::model_edit,loop_item_type_t::model_commit,loop_item_type_t::local_control})
    TEST_ASSERT_TRUE(loopItemMutates(item(type)));
  TEST_ASSERT_FALSE(loopItemMutates(item(loop_item_type_t::proto_read)));
  auto device=item(loop_item_type_t::proto_device);
  for(auto op:{proto_type_t::getSettings,proto_type_t::scanWifi}) {
    device.op=uint8_t(op);TEST_ASSERT_FALSE(loopItemMutates(device));
  }
  for(auto op:{proto_type_t::saveSettings,proto_type_t::beginFrequencyScan,
      proto_type_t::endFrequencyScan,proto_type_t::reboot,proto_type_t::commitModel}) {
    device.op=uint8_t(op);TEST_ASSERT_TRUE(loopItemMutates(device));
  }
  device.op=255;TEST_ASSERT_TRUE(loopItemMutates(device)); // Unknown future work fails conservatively.
}
int main(int argc, char **argv) {
  (void)argc;
  (void)argv;
  UNITY_BEGIN();
  RUN_TEST(only_accepted_mutating_operations_invalidate_restart_history);
  RUN_TEST(test_only_position_and_rolling_code_requests_stay_off_the_radio);
  RUN_TEST(test_off_radio_items_are_never_held);
  RUN_TEST(test_radio_request_waits_for_room);
  RUN_TEST(test_requests_are_not_held_while_suspended);
  RUN_TEST(test_unpair_waits_out_a_suspend);
  RUN_TEST(test_local_markers_are_small_and_use_the_exact_protocol_flag);
  return UNITY_END();
}
