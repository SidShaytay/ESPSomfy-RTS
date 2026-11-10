#include <unity.h>
#include "LocalTargetContext.h"
using namespace LocalUi;
void setUp() {} void tearDown() {}
static TargetContext context() {
  TargetContext c; c.ref={EntityKind::group,9}; c.identity=500; c.revision=3;
  c.modelRevision=4; c.restoreRevision=5; c.radioRevision=6; c.memberCount=2;
  c.members[0]={2,200,7}; c.members[1]={8,800,9}; return c;
}
static void exact_member_and_history_changes_are_stale() {
  const auto original=context(); TEST_ASSERT_TRUE(original.valid());
  for(unsigned field=0;field<11;field++) {
    auto changed=original;
    switch(field) {
    case 0: ++changed.ref.id; break; case 1: changed.ref.kind=EntityKind::device; break;
    case 2: ++changed.identity; break; case 3: ++changed.revision; break;
    case 4: ++changed.modelRevision; break; case 5: ++changed.restoreRevision; break;
    case 6: ++changed.radioRevision; break; case 7: --changed.memberCount; break;
    case 8: ++changed.members[1].id; break; case 9: ++changed.members[1].identity; break;
    case 10: ++changed.members[1].revision; break;
    }
    TEST_ASSERT_FALSE(original.same(changed));
  }
  TEST_ASSERT_TRUE(original.same(context()));
}
static void zero_max_sparse_and_duplicate_member_bounds() {
  auto c=context(); c.ref.id=0; TEST_ASSERT_FALSE(c.valid());
  c=context(); c.ref.id=255; TEST_ASSERT_FALSE(c.valid());
  c=context(); c.memberCount=33; TEST_ASSERT_FALSE(c.valid());
  c=context(); c.members[1].id=c.members[0].id; TEST_ASSERT_FALSE(c.valid());
  c=context(); c.members[1].id=1; TEST_ASSERT_FALSE(c.valid());
  c=context(); c.memberCount=32;
  for(uint8_t i=0;i<32;i++) c.members[i]={uint8_t(i+1),uint32_t(100+i),uint64_t(i+1)};
  TEST_ASSERT_TRUE(c.valid()); TEST_ASSERT_TRUE(c.same(c));
  c.memberCount=0; TEST_ASSERT_TRUE(c.valid()); // empty group remains browsable, has no actions
  c.ref={EntityKind::device,7}; TEST_ASSERT_FALSE(c.valid());
  c.memberCount=1; c.members[0]={7,500,3}; TEST_ASSERT_TRUE(c.valid());
}
static void stop_context_uses_stable_keys_and_context_token() {
  const auto c=context();
  const auto stop=c.stopContext(41,12,8);
  TEST_ASSERT_TRUE(stop.group); TEST_ASSERT_EQUAL(9,stop.id); TEST_ASSERT_EQUAL(500,stop.identity);
  TEST_ASSERT_EQUAL(2,stop.members); TEST_ASSERT_EQUAL(2,stop.memberIds[0]);
  TEST_ASSERT_EQUAL(800,stop.memberIdentities[1]);
  TEST_ASSERT_EQUAL(41,stop.configuration); TEST_ASSERT_EQUAL(12,stop.securityGeneration);
}
int main() {
  UNITY_BEGIN(); RUN_TEST(exact_member_and_history_changes_are_stale);
  RUN_TEST(zero_max_sparse_and_duplicate_member_bounds);
  RUN_TEST(stop_context_uses_stable_keys_and_context_token); return UNITY_END();
}
