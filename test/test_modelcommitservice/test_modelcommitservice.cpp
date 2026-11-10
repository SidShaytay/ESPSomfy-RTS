#include "ApplicationUploadResult.h"
#include "LoopQueue.h"
#include "ModelCommitResult.h"
#include "ModelCommitService.h"
#include "ExplicitCounter.h"
#include "CounterEdit.h"
#include "ManualUpdateLifecycle.h"
#include <esp_timer.h>
#include <Arduino.h>
#include <deque>
#include <string.h>
#include <string>
#include <unity.h>
#include <vector>
#define SOMFY_H
#define PROTO_SOCKET_H
#define utils_h
static bool admitted = true;
static loop_item_t queued;
static std::deque<loop_item_t> pendingItems;
static ModelCommitReport report;
static unsigned commits = 0;
static ModelCommitResult outcome;
static std::vector<std::string> events;
static unsigned recoveries=0;
static unsigned preparations=0;
static GitUpdateOwnerToken preparedOwner;
void runApplicationPreparation(const GitUpdateOwnerToken &owner){++preparations;preparedOwner=owner;}
struct SomfyShadeController {
  struct {CounterQuarantine cause=CounterQuarantine::none;CounterQuarantine counterQuarantine(){return cause;}} transceiver;
  ModelCommitResult commit() {
    ++commits;
    events.push_back("commit");
    return outcome;
  }
} somfy;
CounterDomainResult recoverExplicitCounterFile() {
  ++recoveries;const auto file=somfy.commit();
  if(file.saved())somfy.transceiver.cause=CounterQuarantine::none;
  return {file.saved()?CounterDomainState::saved:CounterDomainState::modelFailed,false,file};
}
static bool pendingRestart=false;
static uint32_t requestedDelay=0;
static GitUpdateOwnerToken restartReservation;
struct Socket {
  void replyModelCommit(const proto_reply_to_t &, ModelCommitReport value) {
    report = value;
    TEST_ASSERT_EQUAL(value.rebooting,pendingRestart);
    events.push_back("reply");
  }
  void modelPersistence(ModelCommitReport, uint32_t) {
    events.push_back("event");
  }
} protoSocket;
LoopQueue loopQueue;
bool LoopQueue::submit(const loop_item_t &item, bool first) {
  queued = item;
  if (!admitted)
    return false;
  if (first)
    pendingItems.push_front(item);
  else
    pendingItems.push_back(item);
  return true;
}
uint32_t LoopQueue::pending() const { return pendingItems.size(); }
#include "../../src/GitUpdate.cpp"
RestartResult requestOwnedRestart(const GitUpdateOwnerToken &expected,uint32_t delay) {
  if(pendingRestart)return {false,RestartReason::pending};
  if(loopQueue.pending()) {gitUpdate.releaseRestartSource(expected,uint64_t(stubEspTimerMicros)/1000);return {false,RestartReason::queuedOperations};}
  const bool held=gitUpdate.acquireRestart(restartReservation,uint64_t(stubEspTimerMicros)/1000,expected);
  pendingRestart=held;requestedDelay=delay;return {held,held ? RestartReason::none : RestartReason::stale};
}
RestartResult requestRestart(uint32_t delay) {return requestOwnedRestart({},delay);}

static GitUpdateOwnerToken source;
static GitUpdateOwnerToken appSource() {
  const auto time=uint64_t(stubEspTimerMicros)/1000;
  TEST_ASSERT_TRUE(gitUpdate.acquireExternal(GitUpdateExternalOwner::manual,source));
  TEST_ASSERT_TRUE(gitUpdate.beginTransfer(source,ManualUpdateKind::application,time));
  TEST_ASSERT_TRUE(gitUpdate.claimTransfer(source,time));
  TEST_ASSERT_TRUE(gitUpdate.transferResource(source,true));
  TEST_ASSERT_TRUE(gitUpdate.transferResource(source,false));
  TEST_ASSERT_TRUE(gitUpdate.holdTransferRestart(source));
  TEST_ASSERT_TRUE(gitUpdate.finishTransfer(source,time,true));
  return source;
}
static unsigned long clockNow() { return 100; }
#define millis clockNow
#include "../../src/ModelCommitService.cpp"
#undef millis
void setUp() {
  if(restartReservation.generation)gitUpdate.releaseExternal(restartReservation);
  restartReservation={};
  const auto cleanup=gitUpdate.claimTransferCleanup(UINT64_MAX);
  if(cleanup.token.generation) {
    gitUpdate.transferResource(cleanup.token,false);
    gitUpdate.closeTransfer(cleanup.token);gitUpdate.releaseExternal(cleanup.token);
  }
  stubEspTimerMicros=100000;
  preparations=0;preparedOwner={};
  admitted = true;
  queued = {};
  pendingItems.clear();
  report = {};
  commits = recoveries = 0;somfy.transceiver.cause=CounterQuarantine::none;
  outcome = {};
  events.clear();
  pendingRestart=false;requestedDelay=0;
}
void tearDown() {}
static void admission_has_no_model_effect_and_commit_item_cannot_use_radio() {
  TEST_ASSERT_TRUE(submitModelCommit(
      {8, 7}, ModelCommitMode::applicationRecoveryAndRestart,appSource()));
  TEST_ASSERT_EQUAL(loop_item_type_t::model_commit, queued.type);
  TEST_ASSERT_EQUAL(8, queued.replyTo.client);
  TEST_ASSERT_TRUE(queued.restartAfterCommit);
  TEST_ASSERT_FALSE(loopItemUsesRadio(queued));
  TEST_ASSERT_EQUAL(0, commits);
  admitted = false;
  TEST_ASSERT_FALSE(submitModelCommit());
  TEST_ASSERT_EQUAL(0, commits);
}
static void
failed_or_uncertain_commit_never_restarts_and_retry_does_not_keep_restart_authority() {
  for(auto state:{ModelCommitState::failed,ModelCommitState::uncertain}) {
    loop_item_t item;item.op=uint8_t(ModelCommitMode::applicationRecoveryAndRestart);item.restartAfterCommit=true;item.externalOwner=appSource();
    outcome={state,ModelCommitStage::sync};const auto before=commits;
    runModelCommit(item);TEST_ASSERT_FALSE(pendingRestart);TEST_ASSERT_EQUAL(before+1,commits);
    runModelCommit(item);TEST_ASSERT_FALSE(pendingRestart);TEST_ASSERT_EQUAL(before+1,commits);
  }
  outcome={ModelCommitState::saved,ModelCommitStage::none};loop_item_t ordinary;
  runModelCommit(ordinary);TEST_ASSERT_FALSE(pendingRestart);TEST_ASSERT_EQUAL(3,commits);
}
static void
verified_upload_commit_replies_and_notifies_before_requested_restart() {
  loop_item_t item;
  item.op=uint8_t(ModelCommitMode::applicationRecoveryAndRestart);
  item.restartAfterCommit = true;
  item.externalOwner=appSource();
  outcome = {ModelCommitState::saved, ModelCommitStage::none};
  runModelCommit(item);
  TEST_ASSERT_TRUE(pendingRestart);
  TEST_ASSERT_EQUAL(500, requestedDelay);
  TEST_ASSERT_EQUAL_STRING("commit", events[0].c_str());
  TEST_ASSERT_EQUAL_STRING("reply", events[1].c_str());
  TEST_ASSERT_EQUAL_STRING("event", events[2].c_str());
}
static void
application_upload_http_reports_only_observed_upload_and_queue_admission() {
  for (unsigned uploaded = 0; uploaded < 2; uploaded++)
    for (unsigned queued = 0; queued < 2; queued++) {
      const auto result = applicationUploadResponse(uploaded, queued);
      TEST_ASSERT_EQUAL(!uploaded ? 500 : !queued ? 503 : 202, result.status);
      TEST_ASSERT_NULL(strstr(result.json, "Successfully updated"));
      TEST_ASSERT_NULL(strstr(result.json, "SUCCESS"));
      if (!queued)
        TEST_ASSERT_NOT_NULL(strstr(result.json, "not queued"));
    }
}
static void
upload_recovery_bypasses_held_head_but_never_replays_or_drops_pending_operations() {
  loop_item_t unpair;
  unpair.type = loop_item_type_t::shade_request;
  unpair.request.type = shade_request_type_t::unpair;
  unpair.request.id = 22;
  loop_item_t edit;
  edit.type = loop_item_type_t::model_edit;
  TEST_ASSERT_TRUE(loopQueue.submit(unpair));
  TEST_ASSERT_TRUE(loopQueue.submit(edit));
  TEST_ASSERT_TRUE(holdLoopItem(pendingItems.front(), true, false));
  TEST_ASSERT_TRUE(
      submitModelCommit({}, ModelCommitMode::applicationRecoveryAndRestart,appSource()));
  const auto marker = pendingItems.front();
  pendingItems.pop_front();
  TEST_ASSERT_EQUAL(loop_item_type_t::model_commit, marker.type);
  outcome = {ModelCommitState::saved, ModelCommitStage::none};
  runModelCommit(marker);
  TEST_ASSERT_EQUAL(1, commits);
  TEST_ASSERT_FALSE(pendingRestart);
  TEST_ASSERT_TRUE(report.file.saved());
  TEST_ASSERT_TRUE(report.restartDeferred);
  TEST_ASSERT_FALSE(report.rebooting);
  TEST_ASSERT_EQUAL(2, pendingItems.size());
  TEST_ASSERT_EQUAL(shade_request_type_t::unpair, pendingItems[0].request.type);
  TEST_ASSERT_EQUAL(22, pendingItems[0].request.id);
  TEST_ASSERT_EQUAL(loop_item_type_t::model_edit, pendingItems[1].type);
  TEST_ASSERT_TRUE(submitModelCommit());
  TEST_ASSERT_EQUAL(loop_item_type_t::shade_request, pendingItems.front().type);
  TEST_ASSERT_EQUAL(loop_item_type_t::model_commit, pendingItems.back().type);
  TEST_ASSERT_EQUAL(1, commits);
}
static void
pending_edit_alone_also_prevents_restart_and_no_authority_survives_retry() {
  loop_item_t edit;
  edit.type = loop_item_type_t::model_edit;
  loopQueue.submit(edit);
  TEST_ASSERT_TRUE(
      submitModelCommit({}, ModelCommitMode::applicationRecoveryAndRestart,appSource()));
  const auto marker = pendingItems.front();
  pendingItems.pop_front();
  outcome = {ModelCommitState::saved, ModelCommitStage::none};
  runModelCommit(marker);
  TEST_ASSERT_TRUE(report.restartDeferred);
  TEST_ASSERT_FALSE(pendingRestart);
  pendingItems.clear();
  loop_item_t ordinary;
  runModelCommit(ordinary);
  TEST_ASSERT_FALSE(pendingRestart);
  TEST_ASSERT_FALSE(report.restartDeferred);
  TEST_ASSERT_FALSE(report.rebooting);
}
static void local_commit_returns_the_notified_file_result_without_restart() {
  for (const auto state : {ModelCommitState::saved, ModelCommitState::failed,
                           ModelCommitState::uncertain}) {
    events.clear();
    outcome = {state, ModelCommitStage::verifyRead};
    const auto actual = applyModelCommit();
    TEST_ASSERT_EQUAL(state, actual.file.state);
    TEST_ASSERT_EQUAL(outcome.stage, actual.file.stage);
    TEST_ASSERT_EQUAL(actual.file.state, report.file.state);
    TEST_ASSERT_FALSE(actual.rebooting);
    TEST_ASSERT_FALSE(actual.restartDeferred);
    TEST_ASSERT_FALSE(pendingRestart);
    TEST_ASSERT_EQUAL(3, events.size());
    TEST_ASSERT_EQUAL_STRING("commit", events[0].c_str());
    TEST_ASSERT_EQUAL_STRING("reply", events[1].c_str());
    TEST_ASSERT_EQUAL_STRING("event", events[2].c_str());
  }
  TEST_ASSERT_EQUAL(3, commits);
}
static void stale_recovery_source_cannot_touch_a_newer_upload() {
  loop_item_t old;old.op=uint8_t(ModelCommitMode::applicationRecoveryAndRestart);old.externalOwner=appSource();old.restartAfterCommit=true;
  stubEspTimerMicros=30100000;
  const auto cleanup=gitUpdate.claimTransferCleanup(30100);
  TEST_ASSERT_TRUE(gitUpdate.closeTransfer(cleanup.token));TEST_ASSERT_TRUE(gitUpdate.releaseExternal(cleanup.token));
  const auto newer=appSource();
  runModelCommit(old);TEST_ASSERT_EQUAL(0,commits);TEST_ASSERT_FALSE(pendingRestart);
  TEST_ASSERT_EQUAL(RestartReason::stale,report.refusal);TEST_ASSERT_TRUE(gitUpdate.ownsExternal(newer));
}
static void explicit_counter_file_recovery_mode_never_leaks_into_unrelated_saves() {
  outcome={ModelCommitState::saved};somfy.transceiver.cause=CounterQuarantine::filePending;
  TEST_ASSERT_TRUE(applyModelCommit().file.saved());TEST_ASSERT_EQUAL(0,recoveries);
  TEST_ASSERT_EQUAL(CounterQuarantine::filePending,somfy.transceiver.cause);
  TEST_ASSERT_TRUE(submitModelCommit({8,7},ModelCommitMode::counterFileRecovery));
  TEST_ASSERT_EQUAL(uint8_t(ModelCommitMode::counterFileRecovery),queued.op);
  TEST_ASSERT_FALSE(queued.restartAfterCommit);pendingItems.pop_front();runModelCommit(queued);
  TEST_ASSERT_EQUAL(1,recoveries);TEST_ASSERT_EQUAL(CounterQuarantine::none,somfy.transceiver.cause);
  TEST_ASSERT_TRUE(report.file.saved());TEST_ASSERT_FALSE(report.rebooting);
}
static void counter_recovery_unknown_and_mode_source_mismatch_never_write() {
  somfy.transceiver.cause=CounterQuarantine::nvsUnknown;
  TEST_ASSERT_TRUE(submitModelCommit({},ModelCommitMode::counterFileRecovery));pendingItems.pop_front();runModelCommit(queued);
  TEST_ASSERT_EQUAL(0,commits);TEST_ASSERT_EQUAL(0,recoveries);TEST_ASSERT_EQUAL(RestartReason::counterUncertain,report.refusal);
  const auto owner=appSource();loop_item_t forged;forged.op=uint8_t(ModelCommitMode::counterFileRecovery);forged.externalOwner=owner;
  runModelCommit(forged);TEST_ASSERT_EQUAL(0,commits);TEST_ASSERT_TRUE(gitUpdate.ownsExternal(owner));
  TEST_ASSERT_FALSE(submitModelCommit({},ModelCommitMode::counterFileRecovery,owner));
}
static void application_preparation_keeps_fifo_and_never_commits_on_producer() {
  loop_item_t first;first.type=loop_item_type_t::shade_request;first.request.id=7;
  TEST_ASSERT_TRUE(loopQueue.submit(first));
  GitUpdateOwnerToken owner{123,GitUpdateExternalOwner::manual};
  TEST_ASSERT_TRUE(submitModelCommit({},ModelCommitMode::applicationPreparation,owner));
  TEST_ASSERT_EQUAL(0,commits);TEST_ASSERT_EQUAL(0,preparations);
  TEST_ASSERT_EQUAL(uint8_t(loop_item_type_t::shade_request),uint8_t(pendingItems.front().type));
  const auto preparation=pendingItems.back();pendingItems.clear();runModelCommit(preparation);
  TEST_ASSERT_EQUAL(1,preparations);TEST_ASSERT_EQUAL(owner.generation,preparedOwner.generation);TEST_ASSERT_EQUAL(0,commits);
  auto forged=preparation;forged.restartAfterCommit=true;runModelCommit(forged);TEST_ASSERT_EQUAL(1,preparations);
  TEST_ASSERT_FALSE(submitModelCommit({},ModelCommitMode::applicationPreparation));
  owner.kind=GitUpdateExternalOwner::restore;TEST_ASSERT_FALSE(submitModelCommit({},ModelCommitMode::applicationPreparation,owner));
}
int main() {
  UNITY_BEGIN();
  RUN_TEST(application_preparation_keeps_fifo_and_never_commits_on_producer);
  RUN_TEST(explicit_counter_file_recovery_mode_never_leaks_into_unrelated_saves);
  RUN_TEST(counter_recovery_unknown_and_mode_source_mismatch_never_write);
  RUN_TEST(stale_recovery_source_cannot_touch_a_newer_upload);
  RUN_TEST(local_commit_returns_the_notified_file_result_without_restart);
  RUN_TEST(admission_has_no_model_effect_and_commit_item_cannot_use_radio);
  RUN_TEST(
      failed_or_uncertain_commit_never_restarts_and_retry_does_not_keep_restart_authority);
  RUN_TEST(
      verified_upload_commit_replies_and_notifies_before_requested_restart);
  RUN_TEST(
      application_upload_http_reports_only_observed_upload_and_queue_admission);
  RUN_TEST(
      upload_recovery_bypasses_held_head_but_never_replays_or_drops_pending_operations);
  RUN_TEST(
      pending_edit_alone_also_prevents_restart_and_no_authority_survives_retry);
  return UNITY_END();
}
