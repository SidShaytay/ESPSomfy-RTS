#include <unity.h>
#include <nvs.h>
#include "CounterEdit.h"
#include "RadioScheduler.h"
void setUp() { FakeNvs::reset(); }
void tearDown() { TEST_ASSERT_TRUE(FakeNvs::handles.empty()); }
static constexpr uint32_t a=0x123456,b=0x654321;
static void verified_writer_checks_set_commit_and_readback() {
  TEST_ASSERT_TRUE(persistExplicitCounter(a,54321).saved());
  TEST_ASSERT_EQUAL(1,FakeNvs::sets);
  TEST_ASSERT_EQUAL(1,FakeNvs::commits);
  TEST_ASSERT_EQUAL(1,FakeNvs::reads);
  auto bytes=FakeNvs::values.at("ShadeCodes/_1193046").bytes;
  uint16_t value=0;memcpy(&value,bytes.data(),sizeof(value));
  TEST_ASSERT_EQUAL(54321,value);
}
static void prewrite_failure_never_touches_storage() {
  FakeNvs::failOpen=1;
  TEST_ASSERT_EQUAL(CounterEditState::failed,persistExplicitCounter(a,50).state);
  TEST_ASSERT_EQUAL(0,FakeNvs::sets);
  TEST_ASSERT_EQUAL(CounterEditState::failed,persistExplicitCounter(0,50).state);
  TEST_ASSERT_EQUAL(CounterEditState::failed,persistExplicitCounter(0x1000000,50).state);
  TEST_ASSERT_EQUAL(1,FakeNvs::opens);
}
static void attempted_write_failures_are_uncertain_and_never_replayed() {
  for(unsigned phase=0;phase<4;++phase) {
    FakeNvs::reset();
    TEST_ASSERT_TRUE(persistExplicitCounter(a,100).saved());
    const auto prior=FakeNvs::values.at("ShadeCodes/_1193046").bytes;
    if(phase==0) FakeNvs::failSet=FakeNvs::sets+1;
    if(phase==1) FakeNvs::failCommit=FakeNvs::commits+1;
    if(phase==2) FakeNvs::failRead=FakeNvs::reads+1;
    if(phase==3) FakeNvs::corruptRead=FakeNvs::reads+1;
    const auto result=persistExplicitCounter(a,200);
    TEST_ASSERT_EQUAL(CounterEditState::uncertain,result.state);
    TEST_ASSERT_FALSE(result.saved());
    TEST_ASSERT_EQUAL(2,FakeNvs::sets); // No recovery write of the old value.
    if(phase==0) TEST_ASSERT_EQUAL_MEMORY(prior.data(),FakeNvs::values.at("ShadeCodes/_1193046").bytes.data(),2);
    else TEST_ASSERT_TRUE(prior!=FakeNvs::values.at("ShadeCodes/_1193046").bytes);
    TEST_ASSERT_TRUE(FakeNvs::handles.empty());
  }
}
static void reservation_quarantines_only_its_identity_and_never_clears() {
  CounterEditSafety safety;
  TEST_ASSERT_EQUAL(CounterEditAdmission::invalid,safety.reserve(0,true,false,false));
  TEST_ASSERT_EQUAL(CounterEditAdmission::busy,safety.reserve(a,false,false,false));
  TEST_ASSERT_EQUAL(CounterEditAdmission::busy,safety.reserve(a,true,true,false));
  TEST_ASSERT_EQUAL(CounterEditAdmission::busy,safety.reserve(a,true,false,true));
  TEST_ASSERT_FALSE(safety.blocks(a));
  TEST_ASSERT_EQUAL(CounterEditAdmission::admitted,safety.reserve(a,true,false,false));
  TEST_ASSERT_TRUE(safety.blocks(a));TEST_ASSERT_FALSE(safety.blocks(b));
  TEST_ASSERT_EQUAL(CounterEditAdmission::busy,safety.reserve(b,true,false,false));
  safety.finish({CounterEditState::failed},{});
  TEST_ASSERT_FALSE(safety.blocks(a));
  TEST_ASSERT_EQUAL(CounterEditAdmission::admitted,safety.reserve(a,true,false,false));
  safety.finish({CounterEditState::uncertain},{});
  TEST_ASSERT_TRUE(safety.uncertain());
  TEST_ASSERT_TRUE(safety.blocks(a));TEST_ASSERT_FALSE(safety.blocks(b));
  TEST_ASSERT_EQUAL(CounterEditAdmission::blocked,safety.reserve(b,true,false,false));
  safety.finish({CounterEditState::saved},{ModelCommitState::saved}); // No later completion clears quarantine.
  TEST_ASSERT_TRUE(safety.uncertain());TEST_ASSERT_TRUE(safety.blocks(a));
}
static void scheduler_waiting_taken_and_repeater_work_refuse_edit_admission() {
  RadioScheduler scheduler;CounterEditSafety safety;somfy_tx_job_t job;
  job.frame.remoteAddress=a;
  TEST_ASSERT_NOT_EQUAL(0,scheduler.submit(job,false,0,0));
  TEST_ASSERT_TRUE(scheduler.hasPendingFor(a));TEST_ASSERT_FALSE(scheduler.hasPendingFor(b));
  TEST_ASSERT_EQUAL(CounterEditAdmission::busy,safety.reserve(a,true,scheduler.hasPendingFor(a),false));
  uint32_t wait=0;TEST_ASSERT_TRUE(scheduler.next(0,job,wait));
  TEST_ASSERT_TRUE(scheduler.hasPendingFor(a));
  TEST_ASSERT_TRUE(scheduler.start(job.ticket,0));scheduler.flush(1);
  TEST_ASSERT_TRUE(scheduler.hasPendingFor(a)); // A flush is not an on-air completion.
  scheduler.ended(job.ticket,false,2);
  TEST_ASSERT_FALSE(scheduler.hasPendingFor(a));
  job={};job.repeater=true;
  TEST_ASSERT_NOT_EQUAL(0,scheduler.submit(job,false,0,3));
  TEST_ASSERT_TRUE(scheduler.hasRepeaterWork());
  TEST_ASSERT_EQUAL(CounterEditAdmission::busy,safety.reserve(a,true,scheduler.hasRepeaterWork(),false));
  TEST_ASSERT_TRUE(scheduler.next(3,job,wait));TEST_ASSERT_TRUE(scheduler.hasRepeaterWork());
  scheduler.ended(job.ticket,true,4);TEST_ASSERT_FALSE(scheduler.hasRepeaterWork());
  TEST_ASSERT_EQUAL(CounterEditAdmission::admitted,safety.reserve(a,true,false,false));
  safety.finish({CounterEditState::saved},{ModelCommitState::saved});TEST_ASSERT_FALSE(safety.blocks(a));
}
static void verified_nvs_file_failure_requires_only_file_recovery() {
  CounterEditSafety safety;
  TEST_ASSERT_EQUAL(CounterEditAdmission::admitted,safety.reserve(a,true,false,false));
  safety.finish({CounterEditState::saved},{ModelCommitState::failed,ModelCommitStage::write});
  TEST_ASSERT_EQUAL(CounterQuarantine::filePending,safety.quarantine());
  TEST_ASSERT_TRUE(safety.blocks(a));TEST_ASSERT_FALSE(safety.blocks(b));
  TEST_ASSERT_EQUAL(a,safety.fileRecoveryAddress());
  safety.finishFileRecovery({ModelCommitState::saved}); // No held recovery claim.
  TEST_ASSERT_TRUE(safety.blocks(a));
  TEST_ASSERT_EQUAL(CounterEditAdmission::blocked,safety.reserve(b,true,false,false));
  TEST_ASSERT_EQUAL(CounterEditAdmission::busy,safety.reserveFileRecovery(true));
  TEST_ASSERT_EQUAL(CounterEditAdmission::admitted,safety.reserveFileRecovery(false));
  safety.finishFileRecovery({ModelCommitState::uncertain,ModelCommitStage::replace});
  TEST_ASSERT_EQUAL(CounterQuarantine::filePending,safety.quarantine());
  TEST_ASSERT_EQUAL(CounterEditAdmission::admitted,safety.reserveFileRecovery(false));
  safety.finishFileRecovery({ModelCommitState::saved});
  TEST_ASSERT_FALSE(safety.uncertain());TEST_ASSERT_FALSE(safety.blocks(a));
  TEST_ASSERT_EQUAL(CounterEditAdmission::invalid,safety.reserveFileRecovery(false));
}
static void unknown_nvs_cannot_be_cleared_by_file_or_unrelated_completions() {
  CounterEditSafety safety;
  TEST_ASSERT_EQUAL(CounterEditAdmission::admitted,safety.reserve(a,true,false,false));
  safety.finish({CounterEditState::uncertain},{ModelCommitState::saved});
  TEST_ASSERT_EQUAL(CounterQuarantine::nvsUnknown,safety.quarantine());
  TEST_ASSERT_EQUAL(0,safety.fileRecoveryAddress());
  TEST_ASSERT_EQUAL(CounterEditAdmission::blocked,safety.reserveFileRecovery(false));
  safety.finishFileRecovery({ModelCommitState::saved});
  safety.finish({CounterEditState::saved},{ModelCommitState::saved});
  TEST_ASSERT_TRUE(safety.blocks(a));
  TEST_ASSERT_EQUAL(CounterQuarantine::nvsUnknown,safety.quarantine());
}
static void observed_writer_never_lowers_retained_or_alias_highwater() {
  FakeNvs::reset();TEST_ASSERT_TRUE(persistExplicitCounter(a,500).saved());
  const auto sets=FakeNvs::sets,commits=FakeNvs::commits;
  TEST_ASSERT_EQUAL(CounterEditState::stale,persistObservedCounter(a,499,0).state);
  TEST_ASSERT_EQUAL(CounterEditState::stale,persistObservedCounter(a,550,600).state);
  TEST_ASSERT_EQUAL(sets,FakeNvs::sets);TEST_ASSERT_EQUAL(commits,FakeNvs::commits);
  TEST_ASSERT_TRUE(persistObservedCounter(a,600,600).saved());
  TEST_ASSERT_TRUE(persistExplicitCounter(a,65535).saved());
  const auto highSets=FakeNvs::sets;
  TEST_ASSERT_EQUAL(CounterEditState::stale,persistObservedCounter(a,0,0).state);
  TEST_ASSERT_EQUAL(highSets,FakeNvs::sets);
  TEST_ASSERT_TRUE(persistExplicitCounter(a,0).saved()); // Deliberate correction remains explicit.
}
static void observed_prewrite_read_error_is_not_absent_or_an_uncertain_write() {
  FakeNvs::reset();TEST_ASSERT_TRUE(persistObservedCounter(a,0,0).saved()); // Exact NOT_FOUND is absent.
  const auto sets=FakeNvs::sets;FakeNvs::failRead=FakeNvs::reads+1;
  TEST_ASSERT_EQUAL(CounterEditState::failed,persistObservedCounter(a,100,0).state);
  TEST_ASSERT_EQUAL(sets,FakeNvs::sets);
  FakeNvs::failRead=0;FakeNvs::values["ShadeCodes/_1193046"].type=FakeNvs::Type::text;
  TEST_ASSERT_EQUAL(CounterEditState::failed,persistObservedCounter(a,100,0).state);
  TEST_ASSERT_EQUAL(sets,FakeNvs::sets);TEST_ASSERT_TRUE(FakeNvs::handles.empty());
}
int main() {
  UNITY_BEGIN();
  RUN_TEST(observed_writer_never_lowers_retained_or_alias_highwater);
  RUN_TEST(observed_prewrite_read_error_is_not_absent_or_an_uncertain_write);
  RUN_TEST(verified_nvs_file_failure_requires_only_file_recovery);
  RUN_TEST(unknown_nvs_cannot_be_cleared_by_file_or_unrelated_completions);
  RUN_TEST(verified_writer_checks_set_commit_and_readback);
  RUN_TEST(prewrite_failure_never_touches_storage);
  RUN_TEST(attempted_write_failures_are_uncertain_and_never_replayed);
  RUN_TEST(reservation_quarantines_only_its_identity_and_never_clears);
  RUN_TEST(scheduler_waiting_taken_and_repeater_work_refuse_edit_admission);
  return UNITY_END();
}
