#define ESPSOMFY_LOCAL_UI
#define SOMFY_H
#include "GitUpdate.h"
#include "LocalAdvancedRuntime.h"
#include "LocalEntityRuntime.h"
#include "LocalJsonAllocator.h"
#include "LocalUiAuthorizationAdapter.h"
#include "LoopQueue.h"
#include "ModelCommitService.h"
#include "SomfyEditor.h"
#include "Utils.h"
#include <stdlib.h>
#include <string.h>
#include <unity.h>
// Native 64-bit ArduinoJson uses larger pools than ESP32. These logic/failure
// tests allow that pool; the actual Runtime harness uses target-like pools and
// the unchanged production 4096-byte wiping allocator budget.
using namespace LocalUi;
using namespace LocalUi::EntityAdmin;
using namespace LocalUi::Advanced;
namespace LocalUi {
static SecuritySnapshot security{AuthMode::none, true, false, 1, 1};
SecuritySnapshot pollAuthorization() { return security; }
bool localControlsFrozen() { return false; }
void authorizationActivity() {}
} // namespace LocalUi
struct Remote {
  uint32_t address = 0;
  uint16_t lastRollingCode = 0;
  uint32_t getRemoteAddress() { return address; }
};
struct Shade {
  uint8_t id = 255;
  uint32_t address = 100;
  uint64_t configurationRevision = 1;
  bool paired = false, idle = true;
  Remote linkedRemotes[7];
  uint8_t getShadeId() { return id; }
  uint32_t getRemoteAddress() { return address; }
  bool idleForAdministration() { return idle; }
};
struct Group {
  uint8_t id = 255;
  uint32_t address = 200;
  uint64_t configurationRevision = 1;
  uint8_t linkedShades[32]{};
  uint8_t getGroupId() { return id; }
  uint32_t getRemoteAddress() { return address; }
};
struct SomfyShadeController {
  uint64_t configurationRevision = 1, restoreRevision = 1;
  Shade shades[32];
  Group groups[16];
  uint32_t repeaters[7]{};
  struct {
    uint64_t configurationRevision = 1;
    bool quiet = true;
    bool restartQuiescent() { return quiet; }
  } transceiver;
  Shade *getShadeById(uint8_t id) {
    for (auto &s : shades)
      if (s.id == id && id != 255)
        return &s;
    return nullptr;
  }
  Group *getGroupById(uint8_t id) {
    for (auto &g : groups)
      if (g.id == id && id != 255)
        return &g;
    return nullptr;
  }
} somfy;
LoopQueue loopQueue;
rebootDelay_t rebootDelay;
static unsigned pendingCount = 0, applies = 0, commits = 0;
static bool expireAllocation = false, restartAllocation = false,
            memberMoves = false;
static ModelCommitState file = ModelCommitState::saved;
uint32_t LoopQueue::pending() const { return pendingCount; }
static void *acquire(size_t n) {
  if (expireAllocation)
    ++LocalUi::security.generation;
  if (restartAllocation)
    rebootDelay.reboot = true;
  if (memberMoves)
    somfy.shades[0].idle = false;
  return malloc(n);
}
static void release(void *p) { free(p); }
edit_result_t applyModelEdit(model_edit_t &edit) {
  ++applies;
  TEST_ASSERT_TRUE(gitUpdate.busy());
  GitUpdateOwnerToken competing;
  TEST_ASSERT_FALSE(
      gitUpdate.acquireExternal(GitUpdateExternalOwner::manual, competing));
  TEST_ASSERT_FALSE(
      gitUpdate.acquireExternal(GitUpdateExternalOwner::counter, competing));
  TEST_ASSERT_FALSE(edit.doc.overflowed());
  if (edit.type == model_edit_type_t::set_paired)
    somfy.shades[0].paired = edit.doc["paired"].as<bool>();
  else if (edit.type == model_edit_type_t::unlink_remote)
    somfy.shades[0].linkedRemotes[6].address = 0;
  else if (edit.type == model_edit_type_t::unlink_repeater)
    somfy.repeaters[6] = 0;
  else {
    TEST_ASSERT_EQUAL(model_edit_type_t::unlink_from_group, edit.type);
    TEST_ASSERT_EQUAL(88, edit.doc["shadeId"].as<uint8_t>());
    somfy.groups[0].linkedShades[31] = 0;
  }
  ++somfy.configurationRevision;
  return edit_result_t::completed(7, {file, ModelCommitStage::write});
}
ModelCommitReport applyModelCommit(const proto_reply_to_t &, ModelCommitMode) {
  ++commits;
  TEST_ASSERT_TRUE(gitUpdate.busy());
  return {{file, ModelCommitStage::write}};
}
namespace LocalUi {
namespace EntityAdmin {
struct Reader : Backend {
  Authorization authorization() override { return {true, 1, 1}; }
  uint64_t revision() override { return 1; }
  bool prepare(Kind, uint8_t, uint8_t &) override { return false; }
  bool read(Kind, uint8_t, proto_field_t, Value &) override { return false; }
  ApplyResult apply(model_edit_type_t, JsonDocument &, Authorization,
                    uint64_t) override {
    return {};
  }
  CommitResult retryFileCommit(Authorization, uint64_t) override { return {}; }
  void activity() override {}
  bool readOrder(Kind kind, OrderIds &ids, OrderIds &slots, uint8_t &count,
                 bool &needed) override {
    ids[0] = kind == Kind::shade ? 7 : 9;
    slots[0] = 0;
    count = 1;
    needed = false;
    return true;
  }
  bool readOrderLabel(Kind, uint8_t slot, uint8_t id, Value &value) override {
    if (slot || (id != 7 && id != 9))
      return false;
    value = {};
    value.kind = 's';
    strcpy(value.text, "Office");
    return true;
  }
};
Backend &runtimeBackend() {
  static Reader reader;
  return reader;
}
} // namespace EntityAdmin
} // namespace LocalUi
#include "../../src/GitUpdate.cpp"
#include "../../src/LocalAdvancedRuntime.cpp"
void setUp() {
  somfy = {};
  somfy.shades[0].id = 7;
  somfy.shades[0].linkedRemotes[6].address = 123456;
  somfy.shades[0].linkedRemotes[6].lastRollingCode = 321;
  somfy.groups[0].id = 9;
  somfy.groups[0].linkedShades[0] = 7;
  somfy.groups[0].linkedShades[31] = 88;
  somfy.repeaters[6] = 654321;
  LocalUi::security = {AuthMode::none, true, false, 1, 1};
  pendingCount = applies = commits = 0;
  expireAllocation = restartAllocation = memberMoves = false;
  rebootDelay = {};
  file = ModelCommitState::saved;
  TEST_ASSERT_FALSE(gitUpdate.busy());
}
void tearDown() { TEST_ASSERT_FALSE(gitUpdate.busy()); }
static Advanced::Backend &source() { return Advanced::runtimeBackend(); }
static void read_stable_targets_and_every_physical_association() {
  OrderIds ids{}, slots{};
  uint8_t count = 0;
  TEST_ASSERT_TRUE(source().targets(Scope::devices, ids, slots, count));
  TEST_ASSERT_EQUAL(7, ids[0]);
  Choice label;
  TEST_ASSERT_TRUE(source().targetLabel({Scope::devices, 7}, 0, label));
  TEST_ASSERT_EQUAL_STRING("#7 Office", label.label);
  TEST_ASSERT_EQUAL(7, label.value);
  Item item;
  TEST_ASSERT_EQUAL(1, source().itemCount({Scope::devices, 7}));
  TEST_ASSERT_TRUE(source().itemAt({Scope::devices, 7}, 0, item));
  TEST_ASSERT_EQUAL(6, item.slot);
  TEST_ASSERT_EQUAL(123456, item.key);
  TEST_ASSERT_EQUAL(321, item.rollingCode);
  TEST_ASSERT_TRUE(source().itemAt({Scope::groups, 9}, 1, item));
  TEST_ASSERT_EQUAL(88, item.key);
  TEST_ASSERT_EQUAL(31, item.slot);
  TEST_ASSERT_TRUE(item.missing);
  TEST_ASSERT_TRUE(source().itemAt({Scope::repeaters, 0}, 0, item));
  TEST_ASSERT_EQUAL(6, item.slot);
  TEST_ASSERT_EQUAL(654321, item.key);
  TEST_ASSERT_EQUAL(0, applies);
}
static void checked_marker_and_missing_member_under_configuration_lease() {
  JsonAllocator memory(65536, acquire, release);
  Context context;
  Target target{Scope::devices, 7};
  source().capture(target, context);
  auto result =
      source().apply(target, {ActionKind::setPaired, 0, 0, true}, context,
                     source().authorization(), source().revision(), memory);
  TEST_ASSERT_TRUE(result.edit.durable());
  TEST_ASSERT_TRUE(somfy.shades[0].paired);
  TEST_ASSERT_EQUAL(1, applies);
  TEST_ASSERT_EQUAL(0, memory.used());
  target = {Scope::groups, 9};
  source().capture(target, context);
  result = source().apply(
      target, {ActionKind::removeMissingMember, 88, 31, false}, context,
      source().authorization(), source().revision(), memory);
  TEST_ASSERT_TRUE(result.edit.applied);
  TEST_ASSERT_EQUAL(2, applies);
  TEST_ASSERT_EQUAL(7, somfy.groups[0].linkedShades[0]);
}
static void refuses_stale_busy_nonmissing_and_nonidle_without_mutation() {
  JsonAllocator memory(65536, acquire, release);
  Target target{Scope::groups, 9};
  Context context;
  source().capture(target, context);
  const auto auth = source().authorization();
  const auto revision = source().revision();
  auto result =
      source().apply(target, {ActionKind::removeMissingMember, 7, 0, false},
                     context, auth, revision, memory);
  TEST_ASSERT_EQUAL(Admission::invalid, result.admission);
  somfy.shades[0].idle = false;
  result =
      source().apply(target, {ActionKind::removeMissingMember, 88, 31, false},
                     context, auth, revision, memory);
  TEST_ASSERT_EQUAL(Admission::busy, result.admission);
  somfy.shades[0].idle = true;
  ++somfy.groups[0].configurationRevision;
  result =
      source().apply(target, {ActionKind::removeMissingMember, 88, 31, false},
                     context, auth, revision, memory);
  TEST_ASSERT_EQUAL(Admission::stale, result.admission);
  TEST_ASSERT_EQUAL(0, applies);
  TEST_ASSERT_EQUAL(revision, somfy.configurationRevision);
}
static void final_authority_after_allocation_and_owner_exclusion() {
  for (unsigned which = 0; which < 3; which++) {
    setUp();
    JsonAllocator memory(65536, acquire, release);
    Context context;
    Target target{Scope::devices, 7};
    source().capture(target, context);
    expireAllocation = which == 0;
    restartAllocation = which == 1;
    memberMoves = which == 2;
    auto result =
        source().apply(target, {ActionKind::setPaired, 0, 0, true}, context,
                       source().authorization(), source().revision(), memory);
    TEST_ASSERT_NOT_EQUAL(Admission::accepted, result.admission);
    TEST_ASSERT_EQUAL(0, applies);
    TEST_ASSERT_EQUAL(0, memory.used());
    TEST_ASSERT_FALSE(gitUpdate.busy());
  }
  setUp();
  JsonAllocator memory(65536, acquire, release);
  Context context;
  Target target{Scope::devices, 7};
  source().capture(target, context);
  GitUpdateOwnerToken owner;
  TEST_ASSERT_TRUE(
      gitUpdate.acquireExternal(GitUpdateExternalOwner::manual, owner));
  auto result =
      source().apply(target, {ActionKind::setPaired, 0, 0, true}, context,
                     source().authorization(), source().revision(), memory);
  TEST_ASSERT_EQUAL(Admission::busy, result.admission);
  TEST_ASSERT_TRUE(gitUpdate.releaseExternal(owner));
}
static void repeater_work_pending_queue_and_file_retry() {
  JsonAllocator memory(65536, acquire, release);
  Context context;
  Target target{Scope::repeaters, 0};
  source().capture(target, context);
  somfy.transceiver.quiet = false;
  auto result = source().apply(
      target, {ActionKind::unlinkRepeater, 654321, 6, false}, context,
      source().authorization(), source().revision(), memory);
  TEST_ASSERT_EQUAL(Admission::busy, result.admission);
  TEST_ASSERT_EQUAL(0, applies);
  somfy.transceiver.quiet = true;
  pendingCount = 1;
  result = source().apply(
      target, {ActionKind::unlinkRepeater, 654321, 6, false}, context,
      source().authorization(), source().revision(), memory);
  TEST_ASSERT_EQUAL(Admission::busy, result.admission);
  pendingCount = 0;
  file = ModelCommitState::failed;
  result = source().apply(
      target, {ActionKind::unlinkRepeater, 654321, 6, false}, context,
      source().authorization(), source().revision(), memory);
  TEST_ASSERT_TRUE(result.edit.applied);
  TEST_ASSERT_FALSE(result.edit.ok);
  TEST_ASSERT_EQUAL(1, applies);
  source().capture(target, context);
  file = ModelCommitState::saved;
  auto retry = source().retryFile(target, context, source().authorization(),
                                  source().revision());
  TEST_ASSERT_TRUE(retry.file.saved());
  TEST_ASSERT_EQUAL(1, applies);
  TEST_ASSERT_EQUAL(1, commits);
}
static void no_memory_invalid_action_and_uncertain_do_not_replay() {
  Target target{Scope::devices, 7};
  Context context;
  source().capture(target, context);
  JsonAllocator denied(0, acquire, release);
  auto result = source().apply(
      target, {ActionKind::unlinkRemote, 123456, 6, false}, context,
      source().authorization(), source().revision(), denied);
  TEST_ASSERT_EQUAL(Admission::noMemory, result.admission);
  TEST_ASSERT_EQUAL(0, applies);
  TEST_ASSERT_FALSE(gitUpdate.busy());
  JsonAllocator memory(65536, acquire, release);
  for (auto action : {Action{ActionKind::unlinkRepeater, 123456, 6, false},
                      Action{ActionKind::unlinkRemote, 999, 6, false},
                      Action{static_cast<ActionKind>(255), 123456, 6, false}}) {
    result = source().apply(target, action, context, source().authorization(),
                            source().revision(), memory);
    TEST_ASSERT_EQUAL(Admission::invalid, result.admission);
    TEST_ASSERT_EQUAL(0, applies);
    TEST_ASSERT_FALSE(gitUpdate.busy());
  }
  file = ModelCommitState::uncertain;
  result = source().apply(target, {ActionKind::unlinkRemote, 123456, 6, false},
                          context, source().authorization(),
                          source().revision(), memory);
  TEST_ASSERT_TRUE(result.edit.applied);
  TEST_ASSERT_FALSE(result.edit.ok);
  TEST_ASSERT_EQUAL(ModelCommitState::uncertain, result.edit.persistence.state);
  TEST_ASSERT_EQUAL(0, somfy.shades[0].linkedRemotes[6].address);
  TEST_ASSERT_EQUAL(1, applies);
  source().capture(target, context);
  file = ModelCommitState::saved;
  somfy.shades[0].idle = false;
  auto retry = source().retryFile(target, context, source().authorization(),
                                  source().revision());
  TEST_ASSERT_TRUE(retry.file.saved());
  TEST_ASSERT_EQUAL(1, applies);
  TEST_ASSERT_EQUAL(1, commits);
  TEST_ASSERT_EQUAL(0, memory.used());
}
static void parser_preserves_exact_out_of_capacity_legacy_member_id() {
  JsonDocument wire, expanded;
  proto_error_buffer_t error;
  wire[protoField(proto_field_t::groupId).key] = 9;
  wire[protoField(proto_field_t::shadeId).key] = 88;
  TEST_ASSERT_NULL(expandEditPayload(model_edit_type_t::unlink_from_group,
                                     wire.as<JsonObjectConst>(), expanded,
                                     error));
  TEST_ASSERT_EQUAL(88, expanded["shadeId"].as<unsigned>());
}
int main() {
  UNITY_BEGIN();
  RUN_TEST(read_stable_targets_and_every_physical_association);
  RUN_TEST(checked_marker_and_missing_member_under_configuration_lease);
  RUN_TEST(refuses_stale_busy_nonmissing_and_nonidle_without_mutation);
  RUN_TEST(final_authority_after_allocation_and_owner_exclusion);
  RUN_TEST(repeater_work_pending_queue_and_file_retry);
  RUN_TEST(no_memory_invalid_action_and_uncertain_do_not_replay);
  RUN_TEST(parser_preserves_exact_out_of_capacity_legacy_member_id);
  return UNITY_END();
}
