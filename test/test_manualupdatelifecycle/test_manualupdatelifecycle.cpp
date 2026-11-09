#include <unity.h>
#include "ManualUpdateLifecycle.h"
void setUp() {} void tearDown() {}
static GitUpdateOwnerToken token(uint64_t n,GitUpdateExternalOwner kind=GitUpdateExternalOwner::manual) { return {n,kind}; }
static void expiry_cannot_abort_an_inflight_operation_or_revive_late_callbacks() {
  ManualUpdateLifecycle policy;
  const auto first=token(1),next=token(2);
  TEST_ASSERT_TRUE(policy.begin(first,ManualUpdateKind::firmware,0));
  TEST_ASSERT_TRUE(policy.claim(first,1));TEST_ASSERT_TRUE(policy.resource(first,true));
  TEST_ASSERT_FALSE(policy.cleanup(30001).token.generation); // Retired, operation still active.
  TEST_ASSERT_FALSE(policy.continuing(first));
  TEST_ASSERT_TRUE(policy.finish(first,30002,true)); // Progress cannot revive expiry.
  const auto cleanup=policy.cleanup(30002);
  TEST_ASSERT_EQUAL(1,cleanup.token.generation);TEST_ASSERT_TRUE(cleanup.resourceOpen);
  TEST_ASSERT_FALSE(policy.claim(first,30002));
  TEST_ASSERT_FALSE(policy.close(first)); // Caller still owes owned abort.
  TEST_ASSERT_TRUE(policy.resource(first,false));TEST_ASSERT_TRUE(policy.close(first));
  TEST_ASSERT_TRUE(policy.begin(next,ManualUpdateKind::firmware,30003));
  TEST_ASSERT_FALSE(policy.retire(first));TEST_ASSERT_FALSE(policy.resource(first,false));
  TEST_ASSERT_FALSE(policy.close(first));TEST_ASSERT_FALSE(policy.claim(first,30004));
  TEST_ASSERT_TRUE(policy.continuing(next));
}
static void token_kind_and_queued_restore_ownership_are_exact() {
  ManualUpdateLifecycle policy;auto own=token(3,GitUpdateExternalOwner::restore),wrong=own;
  TEST_ASSERT_TRUE(policy.begin(own,ManualUpdateKind::restore,0));
  wrong.kind=GitUpdateExternalOwner::manual;
  TEST_ASSERT_FALSE(policy.claim(wrong,1));
  TEST_ASSERT_TRUE(policy.claim(own,1));TEST_ASSERT_TRUE(policy.resource(own,true));
  TEST_ASSERT_TRUE(policy.queue(own));TEST_ASSERT_TRUE(policy.finish(own,1,true));
  TEST_ASSERT_FALSE(policy.retire(own)); // Normal request disconnect cannot revoke a transferred file.
  TEST_ASSERT_FALSE(policy.claim(own,2)); // Request callbacks cannot mutate a transferred file.
  TEST_ASSERT_TRUE(policy.claim(own,2,true));
  TEST_ASSERT_TRUE(policy.resource(own,false));TEST_ASSERT_TRUE(policy.close(own));
}
static void timeout_boundary_progress_rollback_and_abandoned_restore_are_bounded() {
  ManualUpdateLifecycle policy;const auto own=token(4);
  TEST_ASSERT_TRUE(policy.begin(own,ManualUpdateKind::application,10));
  TEST_ASSERT_TRUE(policy.claim(own,30009));TEST_ASSERT_TRUE(policy.finish(own,30009,true));
  TEST_ASSERT_FALSE(policy.cleanup(60008).token.generation);
  auto cleanup=policy.cleanup(60009);TEST_ASSERT_EQUAL(4,cleanup.token.generation);
  TEST_ASSERT_TRUE(policy.close(own));
  TEST_ASSERT_TRUE(policy.begin(token(5),ManualUpdateKind::application,100));
  TEST_ASSERT_FALSE(policy.claim(token(5),99));
  TEST_ASSERT_EQUAL(5,policy.cleanup(100).token.generation);TEST_ASSERT_TRUE(policy.close(token(5)));
  const auto restore=token(6,GitUpdateExternalOwner::restore);
  TEST_ASSERT_TRUE(policy.begin(restore,ManualUpdateKind::restore,200));
  TEST_ASSERT_TRUE(policy.claim(restore,201));TEST_ASSERT_TRUE(policy.resource(restore,true));
  TEST_ASSERT_TRUE(policy.queue(restore));TEST_ASSERT_TRUE(policy.finish(restore,201,true));
  TEST_ASSERT_EQUAL(6,policy.cleanup(30201).token.generation);
  TEST_ASSERT_FALSE(policy.claim(restore,30201,true));
}
static void successful_ota_keeps_same_owner_through_restart_publication() {
  ManualUpdateLifecycle policy;const auto own=token(9);
  TEST_ASSERT_TRUE(policy.begin(own,ManualUpdateKind::firmware,0));
  TEST_ASSERT_TRUE(policy.claim(own,1));TEST_ASSERT_FALSE(policy.holdRestart(own));
  TEST_ASSERT_TRUE(policy.resource(own,true));
  TEST_ASSERT_FALSE(policy.holdRestart(own)); // Owned SDK end is still required.
  TEST_ASSERT_TRUE(policy.resource(own,false));TEST_ASSERT_TRUE(policy.holdRestart(own));
  TEST_ASSERT_TRUE(policy.finish(own,1,true));
  TEST_ASSERT_TRUE(policy.active(own));
  TEST_ASSERT_FALSE(policy.retire(own)); // Completion/disconnect cannot release the owner.
  TEST_ASSERT_FALSE(policy.claim(own,2));TEST_ASSERT_FALSE(policy.claim(own,2,true));
  TEST_ASSERT_FALSE(policy.cleanup(30000).token.generation);
  TEST_ASSERT_EQUAL(9,policy.cleanup(30001).token.generation); // Bounded even if restart never runs.
  TEST_ASSERT_TRUE(policy.close(own));
}
static void recovery_is_owned_bounded_and_cannot_revive_expired_io() {
  ManualUpdateLifecycle policy;auto own=token(20);
  TEST_ASSERT_TRUE(policy.begin(own,ManualUpdateKind::application,0));
  TEST_ASSERT_TRUE(policy.claim(own,1));TEST_ASSERT_TRUE(policy.resource(own,true));
  TEST_ASSERT_TRUE(policy.retire(own)); // Fresh disconnect: cleanup may hand one recovery to loop.
  TEST_ASSERT_TRUE(policy.resource(own,false));TEST_ASSERT_TRUE(policy.holdRecovery(own,2));
  TEST_ASSERT_TRUE(policy.finish(own,2,false));TEST_ASSERT_FALSE(policy.claim(own,3));
  TEST_ASSERT_TRUE(policy.claimRecovery(own,3));TEST_ASSERT_TRUE(policy.close(own));
  own=token(21);TEST_ASSERT_TRUE(policy.begin(own,ManualUpdateKind::application,0));
  TEST_ASSERT_TRUE(policy.claim(own,1));TEST_ASSERT_TRUE(policy.resource(own,true));
  TEST_ASSERT_FALSE(policy.continuing(own,30000));TEST_ASSERT_TRUE(policy.resource(own,false));
  TEST_ASSERT_FALSE(policy.holdRecovery(own,30000));TEST_ASSERT_TRUE(policy.close(own));
}
int main() { UNITY_BEGIN();RUN_TEST(recovery_is_owned_bounded_and_cannot_revive_expired_io);RUN_TEST(successful_ota_keeps_same_owner_through_restart_publication);RUN_TEST(expiry_cannot_abort_an_inflight_operation_or_revive_late_callbacks);RUN_TEST(token_kind_and_queued_restore_ownership_are_exact);RUN_TEST(timeout_boundary_progress_rollback_and_abandoned_restore_are_bounded);return UNITY_END(); }
