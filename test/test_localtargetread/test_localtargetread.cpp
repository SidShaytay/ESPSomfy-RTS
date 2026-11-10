#include "LocalTargetRead.h"
#include <unity.h>
using namespace LocalUi;
struct Remote {
  uint8_t id = 0;
  uint32_t identity = 0;
  uint64_t configurationRevision = 1;
  int position = 0;
  bool moving = false;
  uint16_t rollingCode = 0;
  uint32_t getRemoteAddress() { return identity; }
};
struct Group : Remote {
  uint8_t linkedShades[32] = {};
};
struct Model {
  uint64_t configurationRevision = 1, restoreRevision = 1;
  struct {
    uint64_t configurationRevision = 1;
  } transceiver;
  Remote shades[32] = {{2, 100}, {7, 200}, {19, 300}};
  Group group;
  Model() {
    for (unsigned i = 3; i < 32; i++) {
      shades[i].id = 100 + i;
      shades[i].identity = 1000 + i;
    }
    group.id = 4;
    group.identity = 400;
  }
  Remote *getShadeById(uint8_t id) {
    for (auto &s : shades)
      if (s.id == id)
        return &s;
    return nullptr;
  }
  Group *getGroupById(uint8_t id) { return id == group.id ? &group : nullptr; }
};
void setUp() {}
void tearDown() {}
static void
raw_capture_preserves_all_physical_slots_without_control_admission() {
  Model m;
  m.group.linkedShades[0] = 7;
  m.group.linkedShades[1] = 7;
  m.group.linkedShades[2] = 8;
  m.group.linkedShades[31] = 255;
  RawTargetContext raw;
  TEST_ASSERT_TRUE(readRawTargetContext(m, {EntityKind::group, 4}, raw));
  TEST_ASSERT_TRUE(raw.targetExists);
  TEST_ASSERT_EQUAL(32, raw.slotCount);
  TEST_ASSERT_TRUE(raw.members[0].exists);
  TEST_ASSERT_EQUAL(200, raw.members[0].identity);
  TEST_ASSERT_EQUAL(7, raw.members[1].id);
  TEST_ASSERT_TRUE(raw.members[1].exists);
  TEST_ASSERT_EQUAL(8, raw.members[2].id);
  TEST_ASSERT_FALSE(raw.members[2].exists);
  TEST_ASSERT_EQUAL(255, raw.members[31].id);
  TEST_ASSERT_FALSE(raw.members[31].exists);
  TargetContext control;
  TEST_ASSERT_FALSE(makeControlTargetContext(raw, control));
  TEST_ASSERT_FALSE(control.valid());
}
static void
strict_control_conversion_keeps_sorted_order_and_rejects_dangling_or_duplicate() {
  Model m;
  m.group.linkedShades[0] = 19;
  m.group.linkedShades[2] = 2;
  m.group.linkedShades[7] = 7;
  RawTargetContext raw;
  TargetContext control;
  TEST_ASSERT_TRUE(readRawTargetContext(m, {EntityKind::group, 4}, raw));
  TEST_ASSERT_TRUE(makeControlTargetContext(raw, control));
  TEST_ASSERT_EQUAL(3, control.memberCount);
  TEST_ASSERT_EQUAL(2, control.members[0].id);
  TEST_ASSERT_EQUAL(7, control.members[1].id);
  TEST_ASSERT_EQUAL(19, control.members[2].id);
  m.group.linkedShades[31] = 7;
  readRawTargetContext(m, {EntityKind::group, 4}, raw);
  TEST_ASSERT_FALSE(makeControlTargetContext(raw, control));
  m.group.linkedShades[31] = 8;
  readRawTargetContext(m, {EntityKind::group, 4}, raw);
  TEST_ASSERT_FALSE(makeControlTargetContext(raw, control));
}
static void
empty_groups_and_missing_target_tombstones_have_distinct_exact_contexts() {
  Model m;
  RawTargetContext original, changed;
  TargetContext control;
  TEST_ASSERT_TRUE(readRawTargetContext(m, {EntityKind::group, 4}, original));
  TEST_ASSERT_TRUE(makeControlTargetContext(original, control));
  TEST_ASSERT_EQUAL(0, control.memberCount);
  m.group.id = 255;
  TEST_ASSERT_TRUE(readRawTargetContext(m, {EntityKind::group, 4}, changed));
  TEST_ASSERT_FALSE(changed.targetExists);
  TEST_ASSERT_FALSE(original.same(changed));
  TEST_ASSERT_FALSE(makeControlTargetContext(changed, control));
  original = changed;
  m.group.id = 4;
  ++m.configurationRevision;
  readRawTargetContext(m, {EntityKind::group, 4}, changed);
  TEST_ASSERT_FALSE(original.same(changed));
}
static void
exact_capture_detects_slot_identity_existence_and_history_changes() {
  Model m;
  m.group.linkedShades[0] = 2;
  RawTargetContext original, changed;
  readRawTargetContext(m, {EntityKind::group, 4}, original);
  readRawTargetContext(m, {EntityKind::group, 4}, changed);
  TEST_ASSERT_TRUE(original.same(changed));
  ++m.shades[0].configurationRevision;
  readRawTargetContext(m, {EntityKind::group, 4}, changed);
  TEST_ASSERT_FALSE(original.same(changed));
  --m.shades[0].configurationRevision;
  ++m.shades[0].identity;
  readRawTargetContext(m, {EntityKind::group, 4}, changed);
  TEST_ASSERT_FALSE(original.same(changed));
  --m.shades[0].identity;
  m.group.linkedShades[0] = 0;
  m.group.linkedShades[1] = 2;
  readRawTargetContext(m, {EntityKind::group, 4}, changed);
  TEST_ASSERT_FALSE(original.same(changed));
  m.group.linkedShades[0] = 2;
  m.group.linkedShades[1] = 0;
  ++m.restoreRevision;
  readRawTargetContext(m, {EntityKind::group, 4}, changed);
  TEST_ASSERT_FALSE(original.same(changed));
  --m.restoreRevision;
  ++m.transceiver.configurationRevision;
  readRawTargetContext(m, {EntityKind::group, 4}, changed);
  TEST_ASSERT_FALSE(original.same(changed));
}
static void device_capture_and_invalid_reference_are_bounded() {
  Model m;
  RawTargetContext raw;
  TargetContext control;
  TEST_ASSERT_TRUE(readRawTargetContext(m, {EntityKind::device, 7}, raw));
  TEST_ASSERT_EQUAL(1, raw.slotCount);
  TEST_ASSERT_TRUE(makeControlTargetContext(raw, control));
  TEST_ASSERT_EQUAL(200, control.identity);
  TEST_ASSERT_FALSE(readRawTargetContext(m, {EntityKind::device, 0}, raw));
  TEST_ASSERT_FALSE(readRawTargetContext(m, {EntityKind::device, 255}, raw));
  TEST_ASSERT_FALSE(readRawTargetContext(m, {EntityKind(99), 4}, raw));
}
static void all_32_members_and_existence_bits_are_part_of_exact_capture() {
  Model m;
  for (unsigned i = 0; i < 32; i++)
    m.group.linkedShades[i] = m.shades[i].id;
  RawTargetContext a, b;
  TargetContext control;
  TEST_ASSERT_TRUE(readRawTargetContext(m, {EntityKind::group, 4}, a));
  TEST_ASSERT_EQUAL(32, a.slotCount);
  TEST_ASSERT_EQUAL(131, a.members[31].id);
  TEST_ASSERT_TRUE(makeControlTargetContext(a, control));
  TEST_ASSERT_EQUAL(32, control.memberCount);
  b = a;
  b.members[31].exists = false;
  TEST_ASSERT_FALSE(a.same(b));
  b = a;
  b.targetExists = false;
  TEST_ASSERT_FALSE(a.same(b));
  ++m.shades[0].position;
  m.shades[0].moving = true;
  ++m.shades[0].rollingCode;
  readRawTargetContext(m, {EntityKind::group, 4}, b);
  TEST_ASSERT_TRUE(a.same(b));
}
int main() {
  UNITY_BEGIN();
  RUN_TEST(all_32_members_and_existence_bits_are_part_of_exact_capture);
  RUN_TEST(raw_capture_preserves_all_physical_slots_without_control_admission);
  RUN_TEST(
      strict_control_conversion_keeps_sorted_order_and_rejects_dangling_or_duplicate);
  RUN_TEST(
      empty_groups_and_missing_target_tombstones_have_distinct_exact_contexts);
  RUN_TEST(exact_capture_detects_slot_identity_existence_and_history_changes);
  RUN_TEST(device_capture_and_invalid_reference_are_bounded);
  return UNITY_END();
}
