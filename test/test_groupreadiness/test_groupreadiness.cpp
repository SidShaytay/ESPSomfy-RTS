#include "GroupReadiness.h"
#include <unity.h>
struct Model {
  struct Shade {
    bool idle = true;
    bool idleForAdministration() const { return idle; }
  } shade;
  bool exists = true;
  Shade *getShadeById(uint8_t id) {
    return exists && id == 7 ? &shade : nullptr;
  }
};
struct Group {
  int8_t direction = 0;
  somfy_frame_t lastFrame;
  uint8_t linkedShades[32]{};
};
void setUp() {}
void tearDown() {}
static void stale_direction_is_not_motion_when_complete_members_are_idle() {
  Model model;
  Group group;
  group.direction = -1;
  group.lastFrame.valid = true;
  group.lastFrame.cmd = somfy_commands::Up;
  group.linkedShades[31] = 7;
  group.linkedShades[0] = 255;
  const auto idle = readGroupReadiness(model, group);
  TEST_ASSERT_TRUE(idle.trackedIdle());
  TEST_ASSERT_TRUE(idle.movementEvidence);
  TEST_ASSERT_FALSE(idle.unknownMovement());
  model.shade.idle = false;
  const auto active = readGroupReadiness(model, group);
  TEST_ASSERT_TRUE(active.activeMember);
  TEST_ASSERT_FALSE(active.trackedIdle());
}
static void every_physical_slot_and_dangling_member_is_observed() {
  for (uint8_t slot = 0; slot < 32; slot++) {
    Model model;
    Group group;
    group.direction = 1;
    group.linkedShades[slot] = 7;
    TEST_ASSERT_TRUE(readGroupReadiness(model, group).trackedIdle());
    model.exists = false;
    const auto missing = readGroupReadiness(model, group);
    TEST_ASSERT_TRUE(missing.missingMembers);
    TEST_ASSERT_FALSE(missing.trackedMembers);
    TEST_ASSERT_TRUE(missing.unknownMovement());
  }
  Model model;
  Group group;
  group.direction = 1;
  group.linkedShades[0] = 7;
  group.linkedShades[31] = 88;
  const auto mixed = readGroupReadiness(model, group);
  TEST_ASSERT_TRUE(mixed.trackedMembers);
  TEST_ASSERT_TRUE(mixed.missingMembers);
  TEST_ASSERT_TRUE(mixed.unknownMovement());
}
static void
untracked_movement_evidence_is_conservative_and_empty_never_commanded_is_quiet() {
  Model model;
  Group group;
  TEST_ASSERT_FALSE(readGroupReadiness(model, group).unknownMovement());
  for (auto command :
       {somfy_commands::Up, somfy_commands::Down, somfy_commands::My,
        somfy_commands::Toggle, somfy_commands::StepUp,
        somfy_commands::StepDown, somfy_commands::Favorite}) {
    group.lastFrame.valid = true;
    group.lastFrame.cmd = command;
    TEST_ASSERT_TRUE(readGroupReadiness(model, group).unknownMovement());
    group.lastFrame.valid = false;
    TEST_ASSERT_FALSE(readGroupReadiness(model, group).unknownMovement());
  }
  group.direction = -1;
  TEST_ASSERT_TRUE(readGroupReadiness(model, group).unknownMovement());
  group.direction = 0;
  group.lastFrame.valid = true;
  for (auto command :
       {somfy_commands::Stop, somfy_commands::Prog, somfy_commands::Flag,
        somfy_commands::SunFlag, somfy_commands::Sensor}) {
    group.lastFrame.cmd = command;
    TEST_ASSERT_FALSE(readGroupReadiness(model, group).unknownMovement());
  }
}
int main() {
  UNITY_BEGIN();
  RUN_TEST(stale_direction_is_not_motion_when_complete_members_are_idle);
  RUN_TEST(every_physical_slot_and_dangling_member_is_observed);
  RUN_TEST(
      untracked_movement_evidence_is_conservative_and_empty_never_commanded_is_quiet);
  return UNITY_END();
}
