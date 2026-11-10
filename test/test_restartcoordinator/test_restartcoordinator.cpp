#include <unity.h>
#include "RestartCoordinator.h"
void setUp() {}
void tearDown() {}
static void model_io_and_shutdown_retain_exclusion_outside_runtime_lock() {
  StorageLeaseGate storage;RestartCoordinator coordinator(storage);RestartObservation current;
  auto work=coordinator.begin(500,100,current);TEST_ASSERT_TRUE(work.ready());
  StorageLease transfer;TEST_ASSERT_FALSE(coordinator.acquireUpdate(current,transfer));
  TEST_ASSERT_TRUE(storage.permitsModelWrite(work.storage));
  coordinator.modelCommitted({ModelCommitState::saved,ModelCommitStage::none});
  TEST_ASSERT_TRUE(coordinator.completeAdmission(work,150,current).admitted);
  TEST_ASSERT_TRUE(coordinator.release(work));
  TEST_ASSERT_FALSE(coordinator.acquireUpdate(current,transfer)); // timer still reserves admission
  TEST_ASSERT_FALSE(coordinator.prepareFinal(649,current).ready());
  auto final=coordinator.prepareFinal(650,current);TEST_ASSERT_TRUE(final.ready());
  TEST_ASSERT_TRUE(coordinator.completeFinal(final,700,current));
  TEST_ASSERT_TRUE(storage.busy()); // shutdown occurs before releasing this lease
  TEST_ASSERT_FALSE(coordinator.acquireUpdate(current,transfer));
  TEST_ASSERT_TRUE(coordinator.release(final));
  TEST_ASSERT_FALSE(coordinator.prepareFinal(800,current).ready());
}
static void model_failure_recovery_never_revives_a_cancelled_restart() {
  StorageLeaseGate storage;RestartCoordinator coordinator(storage);RestartObservation current;
  auto work=coordinator.begin(500,100,current);TEST_ASSERT_TRUE(work.ready());
  TEST_ASSERT_TRUE(coordinator.completeAdmission(work,100,current).admitted);TEST_ASSERT_TRUE(coordinator.release(work));
  coordinator.modelCommitted({ModelCommitState::uncertain,ModelCommitStage::verifyRead});
  coordinator.modelCommitted({ModelCommitState::saved,ModelCommitStage::none});
  TEST_ASSERT_FALSE(coordinator.snapshot().pending());
  TEST_ASSERT_FALSE(coordinator.prepareFinal(600,current).ready());
  TEST_ASSERT_FALSE(coordinator.requiresModelRecovery());
}
static void failed_final_commit_keeps_storage_until_cleanup_and_never_retries() {
  StorageLeaseGate storage;RestartCoordinator coordinator(storage);RestartObservation current;
  auto work=coordinator.begin(500,100,current);TEST_ASSERT_TRUE(coordinator.completeAdmission(work,100,current).admitted);TEST_ASSERT_TRUE(coordinator.release(work));
  auto final=coordinator.prepareFinal(600,current);TEST_ASSERT_TRUE(final.ready());
  coordinator.modelCommitted({ModelCommitState::failed,ModelCommitStage::write});
  TEST_ASSERT_FALSE(coordinator.completeFinal(final,650,current,RestartReason::modelFailed));
  StorageLease transfer;TEST_ASSERT_FALSE(coordinator.acquireUpdate(current,transfer));
  TEST_ASSERT_TRUE(coordinator.release(final));
  coordinator.modelCommitted({ModelCommitState::saved,ModelCommitStage::none});
  TEST_ASSERT_FALSE(coordinator.prepareFinal(700,current).ready());
}
static void manual_exclusion_denies_restart_without_wait_or_io_and_stale_tokens_cannot_release() {
  StorageLeaseGate storage;RestartCoordinator coordinator(storage);RestartObservation current;
  coordinator.observe(100,current);
  StorageLease transfer;TEST_ASSERT_TRUE(coordinator.acquireUpdate(current,transfer));
  TEST_ASSERT_FALSE(storage.permitsModelWrite(transfer));
  TEST_ASSERT_FALSE(coordinator.begin(500,100,current).ready());
  TEST_ASSERT_EQUAL(RestartSubmission::refused,coordinator.submit(500));
  auto forged=transfer;forged.use=StorageUse::modelWrite;
  TEST_ASSERT_FALSE(storage.release(forged));TEST_ASSERT_TRUE(storage.busy());
  TEST_ASSERT_TRUE(storage.release(transfer));
  StorageLease replacement;TEST_ASSERT_TRUE(coordinator.acquireUpdate(current,replacement));
  TEST_ASSERT_FALSE(storage.release(transfer));TEST_ASSERT_TRUE(storage.owns(replacement));
  TEST_ASSERT_TRUE(storage.release(replacement));
}
static void cancellation_never_releases_inflight_io_or_admits_a_replacement_from_old_completion() {
  StorageLeaseGate storage;RestartCoordinator coordinator(storage);RestartObservation current;
  auto work=coordinator.begin(500,100,current);TEST_ASSERT_TRUE(work.ready());
  coordinator.cancel();
  TEST_ASSERT_EQUAL(RestartSubmission::refused,coordinator.submit(500));
  TEST_ASSERT_FALSE(coordinator.completeAdmission(work,200,current).admitted);
  TEST_ASSERT_TRUE(storage.busy());TEST_ASSERT_TRUE(coordinator.release(work));
  auto replacement=coordinator.begin(500,300,current);TEST_ASSERT_TRUE(replacement.ready());
  TEST_ASSERT_FALSE(coordinator.release(work));
  TEST_ASSERT_FALSE(coordinator.completeAdmission(work,300,current).admitted);
  TEST_ASSERT_TRUE(coordinator.completeAdmission(replacement,300,current).admitted);
  TEST_ASSERT_TRUE(coordinator.release(replacement));
}
static void async_old_settings_epoch_and_later_queued_work_cancel_without_retry() {
  StorageLeaseGate storage;RestartCoordinator coordinator(storage);RestartObservation current;
  coordinator.observe(100,current);TEST_ASSERT_EQUAL(RestartSubmission::queued,coordinator.submit(500));
  ++current.settingsEpoch; // failed staged save followed by successful reconcile
  TEST_ASSERT_FALSE(coordinator.prepareQueued(200,current).ready());
  TEST_ASSERT_FALSE(coordinator.snapshot().pending());
  TEST_ASSERT_FALSE(coordinator.prepareQueued(300,current).ready());
  auto work=coordinator.begin(500,300,current);TEST_ASSERT_TRUE(work.ready());
  TEST_ASSERT_TRUE(coordinator.completeAdmission(work,300,current).admitted);TEST_ASSERT_TRUE(coordinator.release(work));
  current.operationsPending=true;TEST_ASSERT_FALSE(coordinator.prepareFinal(800,current).ready());
  current.operationsPending=false;TEST_ASSERT_FALSE(coordinator.prepareFinal(900,current).ready());
}
static void rollback_refuses_io_work_before_acquiring_storage_and_never_resurrects() {
  StorageLeaseGate storage;RestartCoordinator coordinator(storage);RestartObservation current;
  coordinator.observe(1000,current);
  TEST_ASSERT_FALSE(coordinator.begin(500,999,current).ready());
  TEST_ASSERT_FALSE(storage.busy());TEST_ASSERT_FALSE(coordinator.snapshot().pending());
  TEST_ASSERT_EQUAL(RestartReason::clock,coordinator.snapshot().reason);
  TEST_ASSERT_FALSE(coordinator.begin(500,999,current).ready());
  TEST_ASSERT_EQUAL(RestartSubmission::refused,coordinator.submit(500));
  // Reaching the high-water mark alone cannot recreate the old request.
  TEST_ASSERT_FALSE(coordinator.prepareQueued(1000,current).ready());
  auto fresh=coordinator.begin(500,1000,current);TEST_ASSERT_TRUE(fresh.ready());
  TEST_ASSERT_TRUE(coordinator.completeAdmission(fresh,1000,current).admitted);TEST_ASSERT_TRUE(coordinator.release(fresh));
  TEST_ASSERT_FALSE(coordinator.prepareFinal(999,current).ready());
  TEST_ASSERT_FALSE(storage.busy());TEST_ASSERT_FALSE(coordinator.prepareFinal(1600,current).ready());
}
static void rollback_refuses_async_admission_before_any_model_io() {
  StorageLeaseGate storage;RestartCoordinator coordinator(storage);RestartObservation current;
  coordinator.observe(1000,current);TEST_ASSERT_EQUAL(RestartSubmission::queued,coordinator.submit(500));
  TEST_ASSERT_FALSE(coordinator.prepareQueued(999,current).ready());TEST_ASSERT_FALSE(storage.busy());
  TEST_ASSERT_FALSE(coordinator.snapshot().pending());
  TEST_ASSERT_FALSE(coordinator.prepareQueued(1600,current).ready());
  auto fresh=coordinator.begin(500,1600,current);TEST_ASSERT_TRUE(fresh.ready());
  TEST_ASSERT_TRUE(coordinator.release(fresh));coordinator.cancel();
}
static void distinct_configuration_and_completed_operation_histories_refuse_old_requests() {
  for(unsigned field=0;field<2;++field) {
    StorageLeaseGate storage;RestartCoordinator coordinator(storage);RestartObservation current;
    coordinator.observe(100,current);TEST_ASSERT_EQUAL(RestartSubmission::queued,coordinator.submit(500));
    if(field==0)++current.modelConfigurationRevision;else ++current.operationEpoch;
    TEST_ASSERT_FALSE(coordinator.prepareQueued(200,current).ready());TEST_ASSERT_FALSE(storage.busy());
    auto work=coordinator.begin(500,300,current);TEST_ASSERT_TRUE(work.ready());
    TEST_ASSERT_TRUE(coordinator.completeAdmission(work,300,current).admitted);TEST_ASSERT_TRUE(coordinator.release(work));
    if(field==0)++current.modelConfigurationRevision;else ++current.operationEpoch;
    TEST_ASSERT_FALSE(coordinator.prepareFinal(800,current).ready());
    TEST_ASSERT_FALSE(coordinator.prepareFinal(900,current).ready());
  }
}
static void exclusive_update_refuses_missing_history_bindings_without_a_lease() {
  for(unsigned field=0;field<2;++field) {
    StorageLeaseGate storage;RestartCoordinator coordinator(storage);RestartObservation current;
    if(field==0)current.modelConfigurationRevision=0;else current.operationEpoch=0;
    StorageLease token;TEST_ASSERT_FALSE(coordinator.updatePrecheck(current));
    TEST_ASSERT_FALSE(coordinator.acquireUpdate(current,token));
    TEST_ASSERT_FALSE(storage.busy());TEST_ASSERT_EQUAL_UINT64(0,token.generation);
    current.modelConfigurationRevision=current.operationEpoch=1;
    TEST_ASSERT_TRUE(coordinator.updatePrecheck(current));
    TEST_ASSERT_TRUE(coordinator.acquireUpdate(current,token));TEST_ASSERT_FALSE(coordinator.updatePrecheck(current));TEST_ASSERT_TRUE(storage.release(token));
  }
}
int main() {
  UNITY_BEGIN();
  RUN_TEST(exclusive_update_refuses_missing_history_bindings_without_a_lease);
  RUN_TEST(distinct_configuration_and_completed_operation_histories_refuse_old_requests);
  RUN_TEST(rollback_refuses_io_work_before_acquiring_storage_and_never_resurrects);
  RUN_TEST(rollback_refuses_async_admission_before_any_model_io);
  RUN_TEST(manual_exclusion_denies_restart_without_wait_or_io_and_stale_tokens_cannot_release);
  RUN_TEST(cancellation_never_releases_inflight_io_or_admits_a_replacement_from_old_completion);
  RUN_TEST(async_old_settings_epoch_and_later_queued_work_cancel_without_retry);
  RUN_TEST(model_io_and_shutdown_retain_exclusion_outside_runtime_lock);
  RUN_TEST(model_failure_recovery_never_revives_a_cancelled_restart);
  RUN_TEST(failed_final_commit_keeps_storage_until_cleanup_and_never_retries);
  return UNITY_END();
}
