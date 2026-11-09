#include "ModelCommitResult.h"
#include "ShadeTypes.h"
#include <Arduino.h>
#include <unity.h>
// Compile the actual runtime method against a minimal model/store boundary.
// There is deliberately no editor, publisher, NVS or motor API in this model.
#define SOMFY_H
#define configfile_h
#define TASK_WDT_H
static uint32_t now = 0;
static unsigned resets = 0, commits = 0;
static unsigned long runtimeTestMillis() { return now; }
static void resetTaskWdt() { ++resets; }
struct SomfyShadeController {
  bool isDirty = false;
  uint32_t lastCommit = 0;
  uint16_t counter = 54321;
  ModelCommitResult lastModelCommit;
  ModelCommitResult commit();
};
SomfyShadeController somfy;
struct SomfyRoom {
  ModelCommitResult save();
};
struct SomfyGroup {
  ModelCommitResult save();
};
struct SomfyShade {
  shade_types shadeType = shade_types::shutter;
  tilt_types tiltType = tilt_types::tiltmotor;
  float myTiltPos = 45, currentTiltPos = 67;
  unsigned publications = 0, clears = 0;
  uint16_t lastRollingCode = 54321;
  void clearTilt() {
    ++clears;
    myTiltPos = -1;
    currentTiltPos = 0;
  }
  void publish() { ++publications; }
  ModelCommitResult commit();
  ModelCommitResult save();
};
static ModelCommitResult outcome;
struct ShadeConfigFile {
  static ModelCommitResult commitModel(SomfyShadeController *model) {
    ++commits;
    TEST_ASSERT_EQUAL(54321, model->counter);
    return outcome;
  }
};
#define millis runtimeTestMillis
static unsigned restartOutcomes=0;
void noteModelCommit(ModelCommitResult) {++restartOutcomes;}
#include "../../src/ModelCommitRuntime.cpp"
#undef millis
void setUp() {
  now = 100;
  resets = commits = restartOutcomes = 0;
  outcome = {};
  somfy = SomfyShadeController{};
}
void tearDown() {TEST_ASSERT_EQUAL(commits,restartOutcomes);}
static void
failed_and_uncertain_commits_remain_dirty_and_throttle_attempt_time() {
  SomfyShadeController model;
  outcome.stage = ModelCommitStage::write;
  auto result = model.commit();
  TEST_ASSERT_FALSE(result.saved());
  TEST_ASSERT_TRUE(model.isDirty);
  TEST_ASSERT_EQUAL(100, model.lastCommit);
  TEST_ASSERT_EQUAL(1, commits);
  TEST_ASSERT_EQUAL(1, resets);
  outcome.state = ModelCommitState::uncertain;
  outcome.stage = ModelCommitStage::verifyRead;
  now = 1500;
  result = model.commit();
  TEST_ASSERT_EQUAL(ModelCommitState::uncertain, result.state);
  TEST_ASSERT_TRUE(model.isDirty);
  TEST_ASSERT_EQUAL(1500, model.lastCommit);
  TEST_ASSERT_EQUAL(2, commits);
  TEST_ASSERT_EQUAL(54321, model.counter);
}
static void
only_verified_success_clears_dirty_and_retry_never_replays_model_effects() {
  SomfyShadeController model;
  model.isDirty = true;
  outcome.stage = ModelCommitStage::sync;
  model.commit();
  TEST_ASSERT_TRUE(model.isDirty);
  outcome = {ModelCommitState::saved, ModelCommitStage::none};
  now = 2000;
  TEST_ASSERT_TRUE(model.commit().saved());
  TEST_ASSERT_FALSE(model.isDirty);
  TEST_ASSERT_EQUAL(2000, model.lastCommit);
  TEST_ASSERT_EQUAL(ModelCommitState::saved, model.lastModelCommit.state);
  TEST_ASSERT_EQUAL(2, commits);
  TEST_ASSERT_EQUAL(54321, model.counter);
  TEST_ASSERT_TRUE(model.commit().saved());
  TEST_ASSERT_EQUAL(3, commits);
  TEST_ASSERT_EQUAL(54321, model.counter);
}
static void
applied_save_normalizes_and_publishes_once_commit_retry_does_neither() {
  SomfyShade shade;
  outcome.stage = ModelCommitStage::sync;
  TEST_ASSERT_FALSE(shade.save().saved());
  TEST_ASSERT_EQUAL(1, shade.clears);
  TEST_ASSERT_EQUAL(1, shade.publications);
  TEST_ASSERT_EQUAL(tilt_types::none, shade.tiltType);
  TEST_ASSERT_FLOAT_WITHIN(0.001, -1, shade.myTiltPos);
  TEST_ASSERT_FLOAT_WITHIN(0.001, 0, shade.currentTiltPos);
  TEST_ASSERT_TRUE(somfy.isDirty);
  TEST_ASSERT_FALSE(somfy.commit().saved());
  outcome = {ModelCommitState::saved, ModelCommitStage::none};
  TEST_ASSERT_TRUE(somfy.commit().saved());
  TEST_ASSERT_FALSE(somfy.isDirty);
  TEST_ASSERT_EQUAL(3, commits);
  TEST_ASSERT_EQUAL(1, shade.clears);
  TEST_ASSERT_EQUAL(1, shade.publications);
  TEST_ASSERT_EQUAL(54321, shade.lastRollingCode);
}
static void
tilting_blind_save_keeps_live_tilt_and_all_entity_saves_return_failure() {
  SomfyShade shade;
  shade.shadeType = shade_types::blind;
  outcome.stage = ModelCommitStage::write;
  TEST_ASSERT_FALSE(shade.save().saved());
  TEST_ASSERT_EQUAL(0, shade.clears);
  TEST_ASSERT_FLOAT_WITHIN(0.001, 45, shade.myTiltPos);
  SomfyRoom room;
  SomfyGroup group;
  TEST_ASSERT_FALSE(room.save().saved());
  TEST_ASSERT_FALSE(group.save().saved());
  TEST_ASSERT_TRUE(somfy.isDirty);
  TEST_ASSERT_EQUAL(3, commits);
  TEST_ASSERT_EQUAL(54321, shade.lastRollingCode);
}
int main() {
  UNITY_BEGIN();
  RUN_TEST(failed_and_uncertain_commits_remain_dirty_and_throttle_attempt_time);
  RUN_TEST(
      only_verified_success_clears_dirty_and_retry_never_replays_model_effects);
  RUN_TEST(
      applied_save_normalizes_and_publishes_once_commit_retry_does_neither);
  RUN_TEST(
      tilting_blind_save_keeps_live_tilt_and_all_entity_saves_return_failure);
  return UNITY_END();
}
