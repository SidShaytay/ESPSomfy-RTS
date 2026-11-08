#include <initializer_list>
#include <unity.h>
#include "RestartPolicy.h"
void setUp() {}
void tearDown() {}
static void epoch_change_cannot_be_resurrected_by_successful_reconcile() {
  RestartPolicy policy;RestartObservation current;
  TEST_ASSERT_TRUE(policy.request(500,100,current).admitted);
  ++current.settingsEpoch; // partial save then reconcile; mask is already clear
  TEST_ASSERT_FALSE(policy.take(600,current));
  TEST_ASSERT_FALSE(policy.snapshot().pending());
  TEST_ASSERT_FALSE(policy.take(700,current));
  TEST_ASSERT_TRUE(policy.request(500,700,current).admitted);
  TEST_ASSERT_TRUE(policy.take(1200,current));TEST_ASSERT_FALSE(policy.take(1300,current));
}
static void delayed_async_request_is_bound_to_published_epochs() {
  RestartPolicy policy;RestartObservation current;
  TEST_ASSERT_EQUAL(RestartSubmission::queued,policy.submit(500,current));
  TEST_ASSERT_TRUE(policy.snapshot().pending());++current.modelEpoch;
  TEST_ASSERT_FALSE(policy.admitQueued(100,current,policy.snapshot().generation));
  TEST_ASSERT_FALSE(policy.snapshot().pending());
}
static void clock_rollback_and_later_work_cancel_authority() {
  for(bool rollback:{false,true}) {
    RestartPolicy policy;RestartObservation current;
    TEST_ASSERT_TRUE(policy.request(500,1000,current).admitted);
    if(!rollback) current.operationsPending=true;
    TEST_ASSERT_FALSE(policy.take(rollback ? 900 : 1500,current));
    current.operationsPending=false;
    TEST_ASSERT_FALSE(policy.take(2000,current));
  }
}
static void async_admission_starts_deadline_after_checks_and_stale_owner_cannot_admit_new_intent() {
  RestartPolicy policy;RestartObservation current;
  TEST_ASSERT_EQUAL(RestartSubmission::queued,policy.submit(500,current));
  const auto first=policy.snapshot().generation;
  policy.cancel();TEST_ASSERT_EQUAL(RestartSubmission::queued,policy.submit(500,current));
  TEST_ASSERT_FALSE(policy.admitQueued(100,current,first));
  TEST_ASSERT_TRUE(policy.snapshot().pending());
  TEST_ASSERT_TRUE(policy.admitQueued(100,current,policy.snapshot().generation));
  TEST_ASSERT_FALSE(policy.take(599,current));TEST_ASSERT_TRUE(policy.take(600,current));
}
static void invalid_or_unsafe_requests_never_create_authority() {
  for(RestartReason refusal:{RestartReason::settingsUncertain,RestartReason::modelFailed,RestartReason::modelUncertain,RestartReason::exhausted}) {
    RestartPolicy policy;RestartObservation current;current.refusal=refusal;
    TEST_ASSERT_FALSE(policy.request(500,100,current).admitted);
    TEST_ASSERT_FALSE(policy.snapshot().pending());
    TEST_ASSERT_EQUAL(RestartSubmission::refused,policy.submit(500,current));
  }
  for(uint32_t delay:{0u,60001u,UINT32_MAX}) {
    RestartPolicy policy;RestartObservation current;
    TEST_ASSERT_FALSE(policy.request(delay,100,current).admitted);
    TEST_ASSERT_EQUAL(RestartSubmission::refused,policy.submit(delay,current));
  }
  RestartPolicy policy;RestartObservation current;current.settingsEpoch=0;
  TEST_ASSERT_FALSE(policy.request(500,100,current).admitted);
  current.settingsEpoch=1;
  TEST_ASSERT_FALSE(policy.request(500,UINT64_MAX,current).admitted);
}
static void failure_or_cancellation_stays_terminal_until_new_explicit_request() {
  for(RestartReason reason:{RestartReason::cancelled,RestartReason::modelFailed,RestartReason::modelUncertain}) {
    RestartPolicy policy;RestartObservation current;
    TEST_ASSERT_TRUE(policy.request(500,100,current).admitted);policy.cancel(reason);
    TEST_ASSERT_FALSE(policy.take(600,current));TEST_ASSERT_EQUAL(reason,policy.snapshot().reason);
    TEST_ASSERT_TRUE(policy.request(500,700,current).admitted);
    TEST_ASSERT_TRUE(policy.take(1200,current));
  }
}
int main() {
  UNITY_BEGIN();
  RUN_TEST(async_admission_starts_deadline_after_checks_and_stale_owner_cannot_admit_new_intent);
  RUN_TEST(invalid_or_unsafe_requests_never_create_authority);
  RUN_TEST(failure_or_cancellation_stays_terminal_until_new_explicit_request);
  RUN_TEST(epoch_change_cannot_be_resurrected_by_successful_reconcile);
  RUN_TEST(delayed_async_request_is_bound_to_published_epochs);
  RUN_TEST(clock_rollback_and_later_work_cancel_authority);
  return UNITY_END();
}
