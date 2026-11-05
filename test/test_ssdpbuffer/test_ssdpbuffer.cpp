#include <unity.h>
#include "SSDPRequestLine.h"

void setUp() {}
void tearDown() {}

static void test_request_line_suffix_stops_before_terminator_and_canary() {
  struct {
    char buffer[64];
    char canary;
  } storage = {{0}, 'X'};
  uint8_t pos = 0;

  for(int i = 0; i < 128; i++) {
    appendSsdpRequestLineChar(storage.buffer, pos, 'a');
  }

  TEST_ASSERT_EQUAL_UINT8(63, pos);
  TEST_ASSERT_EQUAL_CHAR('X', storage.canary);
  TEST_ASSERT_EQUAL_CHAR('\0', storage.buffer[63]);
  for(int i = 0; i < 63; i++) TEST_ASSERT_EQUAL_CHAR('a', storage.buffer[i]);
}

int main(int argc, char **argv) {
  (void)argc;
  (void)argv;
  UNITY_BEGIN();
  RUN_TEST(test_request_line_suffix_stops_before_terminator_and_canary);
  return UNITY_END();
}
