#include "ApplicationUploadResult.h"
#include "LoopQueue.h"
#include "ModelCommitResult.h"
#include "ModelCommitService.h"
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
struct SomfyShadeController {
  ModelCommitResult commit() {
    ++commits;
    events.push_back("commit");
    return outcome;
  }
} somfy;
struct rebootDelay_t {
  bool reboot = false;
  int rebootTime = 0;
  bool closed = false;
} rebootDelay;
struct Socket {
  void replyModelCommit(const proto_reply_to_t &, ModelCommitReport value) {
    report = value;
    TEST_ASSERT_FALSE(rebootDelay.reboot);
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
static unsigned long clockNow() { return 100; }
#define millis clockNow
#include "../../src/ModelCommitService.cpp"
#undef millis
void setUp() {
  admitted = true;
  queued = {};
  pendingItems.clear();
  report = {};
  commits = 0;
  outcome = {};
  events.clear();
  rebootDelay = {};
}
void tearDown() {}
static void admission_has_no_model_effect_and_commit_item_cannot_use_radio() {
  TEST_ASSERT_TRUE(submitModelCommit(
      {8, 7}, ModelCommitMode::applicationRecoveryAndRestart));
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
  loop_item_t item;
  item.restartAfterCommit = true;
  outcome.stage = ModelCommitStage::sync;
  runModelCommit(item);
  TEST_ASSERT_FALSE(rebootDelay.reboot);
  TEST_ASSERT_EQUAL(1, commits);
  outcome.state = ModelCommitState::uncertain;
  runModelCommit(item);
  TEST_ASSERT_FALSE(rebootDelay.reboot);
  TEST_ASSERT_EQUAL(2, commits);
  outcome = {ModelCommitState::saved, ModelCommitStage::none};
  item.restartAfterCommit = false;
  runModelCommit(item);
  TEST_ASSERT_FALSE(rebootDelay.reboot);
  TEST_ASSERT_EQUAL(3, commits);
}
static void
verified_upload_commit_replies_and_notifies_before_requested_restart() {
  loop_item_t item;
  item.restartAfterCommit = true;
  outcome = {ModelCommitState::saved, ModelCommitStage::none};
  runModelCommit(item);
  TEST_ASSERT_TRUE(rebootDelay.reboot);
  TEST_ASSERT_EQUAL(600, rebootDelay.rebootTime);
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
      submitModelCommit({}, ModelCommitMode::applicationRecoveryAndRestart));
  const auto marker = pendingItems.front();
  pendingItems.pop_front();
  TEST_ASSERT_EQUAL(loop_item_type_t::model_commit, marker.type);
  outcome = {ModelCommitState::saved, ModelCommitStage::none};
  runModelCommit(marker);
  TEST_ASSERT_EQUAL(1, commits);
  TEST_ASSERT_FALSE(rebootDelay.reboot);
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
      submitModelCommit({}, ModelCommitMode::applicationRecoveryAndRestart));
  const auto marker = pendingItems.front();
  pendingItems.pop_front();
  outcome = {ModelCommitState::saved, ModelCommitStage::none};
  runModelCommit(marker);
  TEST_ASSERT_TRUE(report.restartDeferred);
  TEST_ASSERT_FALSE(rebootDelay.reboot);
  pendingItems.clear();
  loop_item_t ordinary;
  runModelCommit(ordinary);
  TEST_ASSERT_FALSE(rebootDelay.reboot);
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
    TEST_ASSERT_FALSE(rebootDelay.reboot);
    TEST_ASSERT_EQUAL(3, events.size());
    TEST_ASSERT_EQUAL_STRING("commit", events[0].c_str());
    TEST_ASSERT_EQUAL_STRING("reply", events[1].c_str());
    TEST_ASSERT_EQUAL_STRING("event", events[2].c_str());
  }
  TEST_ASSERT_EQUAL(3, commits);
}
int main() {
  UNITY_BEGIN();
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
