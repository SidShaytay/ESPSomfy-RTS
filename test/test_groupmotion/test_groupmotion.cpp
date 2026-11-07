#include <unity.h>
#include "GroupMotion.h"
static GroupMemberMotion member(float current) { GroupMemberMotion m; m.current = current; return m; }
static void compatible_members_share_wire_direction() {
  auto a=member(0), b=member(20);
  TEST_ASSERT_EQUAL_INT(1, groupMoveDirection(a, 50));
  TEST_ASSERT_TRUE(groupMoveCompatible(a, 50, 1));
  TEST_ASSERT_TRUE(groupMoveCompatible(b, 50, 1));
}
static void correct_and_opposite_members_prevent_native_start() {
  TEST_ASSERT_FALSE(groupMoveCompatible(member(50),50,1));
  TEST_ASSERT_FALSE(groupMoveCompatible(member(80),50,1));
}
static void flipped_semantics_are_applied_to_wire_direction() {
  auto m=member(0); m.invertTarget=true;
  TEST_ASSERT_EQUAL_INT(1, groupMoveDirection(m,20));
  m.flipCommands=true;
  TEST_ASSERT_EQUAL_INT(-1, groupMoveDirection(m,20));
  TEST_ASSERT_EQUAL_FLOAT(80, groupStoredTarget(m,20));
}
static void endpoints_force_even_when_estimate_is_at_target() {
  TEST_ASSERT_EQUAL_INT(-1, groupMoveDirection(member(0),0));
  TEST_ASSERT_EQUAL_INT(1, groupMoveDirection(member(100),100));
}
static void unsafe_motor_modes_and_motion_prevent_native_start() {
  auto m=member(0); m.idle=false; TEST_ASSERT_FALSE(groupMoveCompatible(m,50,1));
  m.idle=true; m.supported=false; TEST_ASSERT_FALSE(groupMoveCompatible(m,50,1));
}
int main() { UNITY_BEGIN(); RUN_TEST(compatible_members_share_wire_direction); RUN_TEST(correct_and_opposite_members_prevent_native_start); RUN_TEST(flipped_semantics_are_applied_to_wire_direction); RUN_TEST(endpoints_force_even_when_estimate_is_at_target); RUN_TEST(unsafe_motor_modes_and_motion_prevent_native_start); return UNITY_END(); }
