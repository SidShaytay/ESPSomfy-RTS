// A generic board: the profile reserves nothing and does nothing.
#include <unity.h>
#include "BoardProfile.cpp"
#include "ModelEditor.h"

void setUp() {
  stubGpioReset();
  SPI = StubSPI();
}
void tearDown() {}

static void test_there_is_no_integrated_radio() {
  TEST_ASSERT_FALSE(BoardProfile::integratedRadio);
}
static void test_no_pad_is_reserved() {
  for(unsigned pin = 0; pin < 256; pin++) TEST_ASSERT_FALSE(BoardProfile::usesPin((uint8_t)pin));
}
static void test_begin_leaves_the_hardware_alone() {
  BoardProfile::begin();
  TEST_ASSERT_EQUAL_UINT(0, stubGpioLog.size());
  TEST_ASSERT_FALSE(SPI.active);
  TEST_ASSERT_EQUAL_UINT(0, SPI.begins);
}
static void test_any_frequency_is_accepted_without_touching_the_bus() {
  TEST_ASSERT_TRUE(BoardProfile::prepareRadio(868.0f));
  TEST_ASSERT_TRUE(BoardProfile::prepareRadio(433.42f));
  TEST_ASSERT_EQUAL_UINT(0, stubGpioLog.size());
  TEST_ASSERT_EQUAL_UINT(0, SPI.begins);
  TEST_ASSERT_EQUAL_UINT(0, SPI.ends);
}
static bool boardUses(uint8_t pin, void *) { return BoardProfile::usesPin(pin); }
static void test_shade_outputs_may_use_any_pad() {
  pin_users_t radio, none;
  radio.uses = boardUses;
  for(unsigned pin = 0; pin < 256; pin++) {
    const uint8_t p = (uint8_t)pin;
    TEST_ASSERT_EQUAL(0, gpioPinConflict(radio_proto::GP_Relay, p, p == 0 ? 1 : 0, 255, radio, none, none, none));
    TEST_ASSERT_EQUAL(0, gpioPinConflict(radio_proto::GP_Remote, p, p == 0 ? 1 : 0, p == 2 ? 3 : 2, radio, none, none, none));
  }
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_there_is_no_integrated_radio);
  RUN_TEST(test_no_pad_is_reserved);
  RUN_TEST(test_begin_leaves_the_hardware_alone);
  RUN_TEST(test_any_frequency_is_accepted_without_touching_the_bus);
  RUN_TEST(test_shade_outputs_may_use_any_pad);
  return UNITY_END();
}
