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
static void application_unmounted_resource_is_not_an_owned_sdk_and_blocks_release() {
  ManualUpdateLifecycle policy;const auto own=token(30);
  TEST_ASSERT_TRUE(policy.begin(own,ManualUpdateKind::application,0));TEST_ASSERT_TRUE(policy.claim(own,1));
  TEST_ASSERT_TRUE(policy.filesystem(own,true));TEST_ASSERT_FALSE(policy.close(own));
  TEST_ASSERT_TRUE(policy.retire(own));TEST_ASSERT_TRUE(policy.finish(own,2,false));
  const auto cleanup=policy.cleanup(2);TEST_ASSERT_TRUE(cleanup.filesystemUnavailable);TEST_ASSERT_FALSE(cleanup.resourceOpen);
  TEST_ASSERT_FALSE(policy.close(own));TEST_ASSERT_FALSE(policy.filesystem(token(31),false));
  TEST_ASSERT_TRUE(policy.filesystem(own,false));TEST_ASSERT_TRUE(policy.close(own));
}
static void application_cannot_restart_or_claim_recovery_while_its_sdk_is_open() {
  ManualUpdateLifecycle policy;const auto own=token(32);
  TEST_ASSERT_TRUE(policy.begin(own,ManualUpdateKind::application,0));TEST_ASSERT_TRUE(policy.claim(own,1));
  TEST_ASSERT_TRUE(policy.filesystem(own,true));TEST_ASSERT_TRUE(policy.resource(own,true));
  TEST_ASSERT_FALSE(policy.filesystem(own,false));TEST_ASSERT_FALSE(policy.close(own));
  TEST_ASSERT_TRUE(policy.resource(own,false));TEST_ASSERT_TRUE(policy.holdRestart(own));TEST_ASSERT_TRUE(policy.finish(own,2,true));
  TEST_ASSERT_FALSE(policy.restartReady(own,3));TEST_ASSERT_FALSE(policy.handoffRestart(own,3));
  TEST_ASSERT_TRUE(policy.claimRecovery(own,3));TEST_ASSERT_TRUE(policy.filesystem(own,false));
  TEST_ASSERT_TRUE(policy.finish(own,3,true));TEST_ASSERT_TRUE(policy.restartReady(own,4));TEST_ASSERT_TRUE(policy.handoffRestart(own,4));
}
static void prepared_application_binds_source_and_separates_prepare_from_receive() {
  ManualUpdateLifecycle policy;const auto own=token(40);const ManualApplicationSource source{77,9,4096};
  TEST_ASSERT_TRUE(policy.begin(own,ManualUpdateKind::application,0));TEST_ASSERT_TRUE(policy.prepareApplication(own,source,0));
  TEST_ASSERT_FALSE(policy.claim(own,1));TEST_ASSERT_TRUE(policy.claimPreparation(own,source,1));
  TEST_ASSERT_FALSE(policy.resource(own,true));TEST_ASSERT_FALSE(policy.applicationPrepared(own));
  TEST_ASSERT_TRUE(policy.filesystem(own,true));TEST_ASSERT_TRUE(policy.applicationPrepared(own));
  TEST_ASSERT_TRUE(policy.finish(own,2,true));TEST_ASSERT_FALSE(policy.retire(own));
  auto stale=source;++stale.securityRevision;TEST_ASSERT_FALSE(policy.claimPreparedApplication(own,stale,3));
  stale=source;++stale.nonce;TEST_ASSERT_FALSE(policy.claimPreparedApplication(own,stale,3));
  stale=source;++stale.bytes;TEST_ASSERT_FALSE(policy.claimPreparedApplication(own,stale,3));
  TEST_ASSERT_TRUE(policy.claimPreparedApplication(own,source,3));TEST_ASSERT_TRUE(policy.resource(own,true));
  TEST_ASSERT_FALSE(policy.claimPreparedApplication(own,source,4));TEST_ASSERT_TRUE(policy.retire(own));
  TEST_ASSERT_TRUE(policy.resource(own,false));TEST_ASSERT_TRUE(policy.filesystem(own,false));TEST_ASSERT_TRUE(policy.close(own));
}
static void abandoned_preparation_never_releases_an_unmounted_filesystem() {
  for(unsigned clock=0;clock<2;++clock){
    ManualUpdateLifecycle policy;const auto own=token(41);const ManualApplicationSource source{78,10,4096};
    TEST_ASSERT_TRUE(policy.begin(own,ManualUpdateKind::application,100));TEST_ASSERT_TRUE(policy.prepareApplication(own,source,100));
    TEST_ASSERT_TRUE(policy.claimPreparation(own,source,101));TEST_ASSERT_TRUE(policy.filesystem(own,true));
    TEST_ASSERT_TRUE(policy.applicationPrepared(own));TEST_ASSERT_TRUE(policy.finish(own,102,true));
    TEST_ASSERT_FALSE(policy.claimPreparedApplication(own,source,clock?99:30102));
    const auto cleanup=policy.cleanup(30103);TEST_ASSERT_TRUE(cleanup.filesystemUnavailable);TEST_ASSERT_FALSE(cleanup.resourceOpen);
    TEST_ASSERT_FALSE(policy.close(own));TEST_ASSERT_TRUE(policy.filesystem(own,false));TEST_ASSERT_TRUE(policy.close(own));
    TEST_ASSERT_FALSE(policy.claimPreparedApplication(own,source,30104));
  }
}
static void expired_application_can_only_enter_reducing_filesystem_recovery() {
  ManualUpdateLifecycle policy;const auto own=token(42);const ManualApplicationSource source{79,11,4096};
  TEST_ASSERT_TRUE(policy.begin(own,ManualUpdateKind::application,0));TEST_ASSERT_TRUE(policy.prepareApplication(own,source,0));
  TEST_ASSERT_TRUE(policy.claimPreparation(own,source,1));TEST_ASSERT_TRUE(policy.filesystem(own,true));
  auto wrong=source;++wrong.nonce;TEST_ASSERT_FALSE(policy.cancelPreparation(own,wrong));
  TEST_ASSERT_TRUE(policy.cancelPreparation(own,source));TEST_ASSERT_FALSE(policy.applicationPrepared(own));
  TEST_ASSERT_TRUE(policy.finish(own,2,false));TEST_ASSERT_TRUE(policy.cleanup(30002).filesystemUnavailable);
  TEST_ASSERT_TRUE(policy.holdRecovery(own,30002));TEST_ASSERT_TRUE(policy.finish(own,30002,false));
  TEST_ASSERT_FALSE(policy.continuing(own));TEST_ASSERT_FALSE(policy.cleanup(30003).token.generation);
  TEST_ASSERT_FALSE(policy.claim(own,30003));TEST_ASSERT_TRUE(policy.claimRecovery(own,30003));
  TEST_ASSERT_FALSE(policy.resource(own,true));TEST_ASSERT_FALSE(policy.holdRestart(own));
  TEST_ASSERT_TRUE(policy.filesystem(own,false));TEST_ASSERT_TRUE(policy.close(own));
}
static void application_restart_waits_for_verified_recovery_and_never_revives_expiry() {
  for(bool expire:{false,true}) {
    ManualUpdateLifecycle policy;const auto own=token(50);
    TEST_ASSERT_TRUE(policy.begin(own,ManualUpdateKind::application,0));TEST_ASSERT_TRUE(policy.claim(own,0));
    TEST_ASSERT_TRUE(policy.filesystem(own,true));TEST_ASSERT_TRUE(policy.resource(own,true));TEST_ASSERT_TRUE(policy.resource(own,false));
    TEST_ASSERT_TRUE(policy.holdRecovery(own,1));TEST_ASSERT_TRUE(policy.finish(own,1,true));
    TEST_ASSERT_TRUE(policy.claimRecovery(own,expire?30001:2));TEST_ASSERT_FALSE(policy.holdRestart(own));
    TEST_ASSERT_TRUE(policy.filesystem(own,false));
    TEST_ASSERT_EQUAL(!expire,policy.holdRestart(own));
    if(expire)TEST_ASSERT_TRUE(policy.close(own));
  }
}
int main() { UNITY_BEGIN();RUN_TEST(application_restart_waits_for_verified_recovery_and_never_revives_expiry);RUN_TEST(expired_application_can_only_enter_reducing_filesystem_recovery);RUN_TEST(prepared_application_binds_source_and_separates_prepare_from_receive);RUN_TEST(abandoned_preparation_never_releases_an_unmounted_filesystem);RUN_TEST(application_unmounted_resource_is_not_an_owned_sdk_and_blocks_release);RUN_TEST(application_cannot_restart_or_claim_recovery_while_its_sdk_is_open);RUN_TEST(recovery_is_owned_bounded_and_cannot_revive_expired_io);RUN_TEST(successful_ota_keeps_same_owner_through_restart_publication);RUN_TEST(expiry_cannot_abort_an_inflight_operation_or_revive_late_callbacks);RUN_TEST(token_kind_and_queued_restore_ownership_are_exact);RUN_TEST(timeout_boundary_progress_rollback_and_abandoned_restore_are_bounded);return UNITY_END(); }
