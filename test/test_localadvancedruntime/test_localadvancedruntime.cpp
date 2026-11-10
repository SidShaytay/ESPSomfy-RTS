#define ESPSOMFY_LOCAL_UI
#define SOMFY_H
#include "CounterEdit.h"
#include "CounterResults.h"
#include "GitUpdate.h"
#include "LocalAdvancedRuntime.h"
#include "LocalEntityRuntime.h"
#include "LocalJsonAllocator.h"
#include "LocalUiAuthorizationAdapter.h"
#include "LoopQueue.h"
#include "ModelCommitService.h"
#include "ModelLoadTrust.h"
#include "SomfyEditor.h"
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
  radio_proto proto=radio_proto::RTS;uint8_t bitLength=56;
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
  radio_proto proto=radio_proto::RTS;uint8_t bitLength=56;
  int8_t direction = 1;
  somfy_frame_t lastFrame;
  uint8_t id = 255;
  uint32_t address = 200;
  uint64_t configurationRevision = 1;
  uint8_t linkedShades[32]{};
  uint8_t getGroupId() { return id; }
  uint32_t getRemoteAddress() { return address; }
};
struct SomfyShadeController {
  ModelLoadTrust modelLoad{ModelLoadStatus::loaded};
  uint64_t configurationRevision = 1, restoreRevision = 1;
  Shade shades[32];
  Group groups[16];
  uint32_t repeaters[7]{};
  struct {
    uint64_t configurationRevision = 1;
    bool quiet = true;
    bool present=true,suspended=false;
    struct {uint8_t type=56;} config;
    bool available(){return present;}bool txSuspended(){return suspended;}
    uint32_t blocked = 0;
    CounterQuarantine quarantine = CounterQuarantine::none;
    CounterQuarantine counterQuarantine() { return quarantine; }
    bool counterBlocksIdentity(uint32_t address) {
      return address && blocked == address;
    }
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
static bool pendingRestart = false;
bool restartPending() { return pendingRestart; }
static unsigned pendingCount = 0, applies = 0, commits = 0;
static bool expireAllocation = false, restartAllocation = false,
            memberMoves = false, blockAllocation = false;
static ModelCommitState file = ModelCommitState::saved;
uint32_t LoopQueue::pending() const { return pendingCount; }
static loop_item_t learnedMarker;
static bool queueAccepts = true;
bool LoopQueue::submit(const loop_item_t &item, bool) {
  if (!queueAccepts)
    return false;
  learnedMarker = item;
  return true;
}
static unsigned primaryWrites=0;
static bool primaryMatches=true;
static void (*afterPrimaryRead)()=nullptr;
CounterDomainState reviewPrimaryCounter(uint8_t id,PrimaryCounterReview &out) {
  TEST_ASSERT_FALSE(gitUpdate.busy());out={};out.shadeId=id;out.address=100;out.primary=1000;out.stored={1000,true};out.codes[0]=1000;out.matching[0]=1;
  if(afterPrimaryRead){auto callback=afterPrimaryRead;afterPrimaryRead=nullptr;callback();}
  return CounterDomainState::saved;
}
bool primaryCounterReviewCurrent(uint8_t id,const PrimaryCounterReview &review) {return primaryMatches && id==review.shadeId && review.address==100;}
CounterDomainResult applyPrimaryCounterCorrection(uint8_t,uint16_t,const PrimaryCounterReview &,CounterCorrectionWitness &witness) {
  TEST_ASSERT_FALSE(gitUpdate.busy());GitUpdateOwnerToken owner;TEST_ASSERT_TRUE(gitUpdate.acquireExternal(GitUpdateExternalOwner::counter,owner));
  const bool current=witness.current();if(current)++primaryWrites;TEST_ASSERT_TRUE(gitUpdate.releaseExternal(owner));
  return current?CounterDomainResult{CounterDomainState::saved,true,{ModelCommitState::saved}}:CounterDomainResult{CounterDomainState::stale};
}
static unsigned linkWrites = 0;
static CounterDomainState linkState = CounterDomainState::saved;
static void *acquire(size_t n) {
  if (expireAllocation)
    ++LocalUi::security.generation;
  if (restartAllocation)
    pendingRestart = true;
  if (memberMoves)
    somfy.shades[0].idle = false;
  if (blockAllocation)
    somfy.transceiver.blocked = 100;
  return malloc(n);
}
static void release(void *p) { free(p); }
edit_result_t applyModelEdit(model_edit_t &edit) {
  ++applies;
  if (edit.type == model_edit_type_t::link_remote) {
    TEST_ASSERT_FALSE(
        gitUpdate.busy()); // Shared checked domain owns counter lease itself.
    GitUpdateOwnerToken owner;
    TEST_ASSERT_TRUE(
        gitUpdate.acquireExternal(GitUpdateExternalOwner::counter, owner));
    TEST_ASSERT_EQUAL(3, edit.doc.size());
    TEST_ASSERT_TRUE(edit.doc["proto"].isNull());
    TEST_ASSERT_TRUE(edit.doc["bitLength"].isNull());
    const bool applied = linkState == CounterDomainState::saved ||
                         linkState == CounterDomainState::modelFailed ||
                         linkState == CounterDomainState::modelUncertain;
    if (applied || linkState == CounterDomainState::nvsUncertain)
      ++linkWrites;
    if (applied) {
      somfy.shades[0].linkedRemotes[0].address = edit.doc["remoteAddress"];
      ++somfy.configurationRevision;
    }
    if (linkState == CounterDomainState::nvsUncertain)
      somfy.transceiver.quarantine = CounterQuarantine::nvsUnknown;
    else if (applied && file != ModelCommitState::saved)
      somfy.transceiver.quarantine = CounterQuarantine::filePending;
    TEST_ASSERT_TRUE(gitUpdate.releaseExternal(owner));
    return counterLinkResult(
        7, {linkState, applied, {file, ModelCommitStage::write}});
  }
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
ModelCommitReport applyModelCommit(const proto_reply_to_t &,
                                   ModelCommitMode mode) {
  ++commits;
  if (mode == ModelCommitMode::counterFileRecovery &&
      somfy.transceiver.quarantine != CounterQuarantine::none) {
    TEST_ASSERT_FALSE(gitUpdate.busy());
    if (somfy.transceiver.quarantine == CounterQuarantine::nvsUnknown) {
      ModelCommitReport report;
      report.refusal = RestartReason::counterUncertain;
      return report;
    }
    GitUpdateOwnerToken owner;
    TEST_ASSERT_TRUE(
        gitUpdate.acquireExternal(GitUpdateExternalOwner::counter, owner));
    if (file == ModelCommitState::saved)
      somfy.transceiver.quarantine = CounterQuarantine::none;
    TEST_ASSERT_TRUE(gitUpdate.releaseExternal(owner));
  } else
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
static unsigned radioCommands=0;static shade_request_t sentRemote;
struct RecordingRequests {shade_request_result_t run(const shade_request_t &request){TEST_ASSERT_TRUE(gitUpdate.busy());GitUpdateOwnerToken competing;const bool acquired=gitUpdate.acquireExternal(GitUpdateExternalOwner::manual,competing);if(acquired)gitUpdate.releaseExternal(competing);TEST_ASSERT_FALSE(acquired);++radioCommands;sentRemote=request;return shade_request_result_t::done;}} shadeRequests;
#define ShadeRequests RecordingRequests
#include "../../src/LocalAdvancedRuntime.cpp"
#undef ShadeRequests
#include "../../src/LocalLearnQueueRuntime.cpp"
void setUp() {
  somfy = {};radioCommands=0;sentRemote={};
  somfy.shades[0].id = 7;
  somfy.shades[0].linkedRemotes[6].address = 123456;
  somfy.shades[0].linkedRemotes[6].lastRollingCode = 321;
  somfy.groups[0].id = 9;
  somfy.groups[0].linkedShades[0] = 7;
  somfy.groups[0].linkedShades[31] = 88;
  somfy.repeaters[6] = 654321;
  LocalUi::security = {AuthMode::none, true, false, 1, 1};
  pendingCount = applies = commits = linkWrites = 0;
  linkState = CounterDomainState::saved;
  queueAccepts = true;
  expireAllocation = restartAllocation = memberMoves = blockAllocation = false;
  pendingRestart = false;primaryWrites=0;primaryMatches=true;afterPrimaryRead=nullptr;
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
  for (unsigned which = 0; which < 4; which++) {
    setUp();
    JsonAllocator memory(65536, acquire, release);
    Context context;
    Target target{Scope::devices, 7};
    source().capture(target, context);
    expireAllocation = which == 0;
    restartAllocation = which == 1;
    memberMoves = which == 2;
    blockAllocation = which == 3;
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
static void full_association_capacity_and_exact_repeater_context() {
  for (uint8_t i = 0; i < 7; i++) {
    somfy.shades[0].linkedRemotes[i].address = 100 + i;
    somfy.shades[0].linkedRemotes[i].lastRollingCode = 200 + i;
    somfy.repeaters[i] = 1000 + i;
  }
  Item item;
  TEST_ASSERT_EQUAL(7, source().itemCount({Scope::devices, 7}));
  TEST_ASSERT_TRUE(source().itemAt({Scope::devices, 7}, 6, item));
  TEST_ASSERT_EQUAL(106, item.key);
  TEST_ASSERT_EQUAL(206, item.rollingCode);
  TEST_ASSERT_EQUAL(7, source().itemCount({Scope::repeaters, 0}));
  TEST_ASSERT_TRUE(source().itemAt({Scope::repeaters, 0}, 6, item));
  TEST_ASSERT_EQUAL(1006, item.key);
  Context context;
  Target target{Scope::repeaters, 0};
  source().capture(target, context);
  somfy.repeaters[0] = 99;
  JsonAllocator memory(65536, acquire, release);
  auto result = source().apply(
      target, {ActionKind::unlinkRepeater, 1006, 6, false}, context,
      source().authorization(), source().revision(), memory);
  TEST_ASSERT_EQUAL(Admission::stale, result.admission);
  TEST_ASSERT_EQUAL(0, applies);
  TEST_ASSERT_EQUAL(1006, somfy.repeaters[6]);
}
static void exact_identity_quarantine_cannot_be_hidden_by_metadata() {
  JsonAllocator memory(65536, acquire, release);
  Context context;
  Target shade{Scope::devices, 7};
  source().capture(shade, context);
  somfy.transceiver.blocked = 100;
  auto result =
      source().apply(shade, {ActionKind::setPaired, 0, 0, true}, context,
                     source().authorization(), source().revision(), memory);
  TEST_ASSERT_EQUAL(Admission::busy, result.admission);
  TEST_ASSERT_NOT_NULL(strstr(result.reason, "Counter recovery"));
  TEST_ASSERT_EQUAL(0, applies);
  somfy.transceiver.blocked = 123456;
  result = source().apply(shade, {ActionKind::unlinkRemote, 123456, 6, false},
                          context, source().authorization(),
                          source().revision(), memory);
  TEST_ASSERT_EQUAL(Admission::busy, result.admission);
  TEST_ASSERT_EQUAL(123456, somfy.shades[0].linkedRemotes[6].address);
  // Quarantine of an unrelated address must not disable missing-member repair
  // or removal of an unrelated repeater. Neither operation clears quarantine.
  Target group{Scope::groups, 9};
  source().capture(group, context);
  result = source().apply(
      group, {ActionKind::removeMissingMember, 88, 31, false}, context,
      source().authorization(), source().revision(), memory);
  TEST_ASSERT_TRUE(result.edit.applied);
  TEST_ASSERT_EQUAL(123456, somfy.transceiver.blocked);
  Target repeaters{Scope::repeaters, 0};
  source().capture(repeaters, context);
  result = source().apply(
      repeaters, {ActionKind::unlinkRepeater, 654321, 6, false}, context,
      source().authorization(), source().revision(), memory);
  TEST_ASSERT_TRUE(result.edit.applied);
  TEST_ASSERT_EQUAL(123456, somfy.transceiver.blocked);
}
struct Witness : LearnPending {
  LearnQueueReason state = LearnQueueReason::ready;
  unsigned calls = 0;
  LearnQueueReason current(uint64_t) override {
    ++calls;
    return state;
  }
  void execute(uint64_t) override {}
  void cancelled(LearnQueueReason) override {}
};
static void checked_link_uses_only_address_counter_and_domain_owned_lease() {
  JsonAllocator memory(65536, acquire, release);
  Context context;
  const Target target{Scope::devices, 7};
  source().capture(target, context);
  Witness witness;
  LearnSelection candidate{1, 1, 12345, 65535, 2, 80};
  const auto result =
      source().linkLearn(target, context, source().authorization(),
                         source().revision(), candidate, witness, memory);
  TEST_ASSERT_TRUE(result.edit.applied);
  TEST_ASSERT_TRUE(result.edit.hasCounterResult);
  TEST_ASSERT_EQUAL(CounterDomainState::saved, result.edit.counterState);
  TEST_ASSERT_EQUAL(1, applies);
  TEST_ASSERT_EQUAL(1, linkWrites);
  TEST_ASSERT_EQUAL(12345, somfy.shades[0].linkedRemotes[0].address);
  TEST_ASSERT_GREATER_THAN(0, witness.calls);
}
static void final_link_witness_auth_queue_owner_and_memory_are_fresh() {
  const Target target{Scope::devices, 7};
  Context context;
  source().capture(target, context);
  Witness witness;
  LearnSelection candidate{1, 1, 12345, 0, 0, 56};
  for (unsigned reason = 0; reason < 5; reason++) {
    JsonAllocator memory(65536, acquire, release);
    if (reason == 0)
      witness.state = LearnQueueReason::candidate;
    if (reason == 1)
      pendingCount = 1;
    if (reason == 2)
      pendingRestart = true;
    if (reason == 3)
      expireAllocation = true;
    auto result =
        source().linkLearn(target, context, source().authorization(),
                           source().revision(), candidate, witness, memory);
    TEST_ASSERT_FALSE(result.edit.applied);
    if (reason == 0)
      TEST_ASSERT_EQUAL(LearnQueueReason::candidate, result.freshness);
    TEST_ASSERT_EQUAL(0, applies);
    TEST_ASSERT_EQUAL(0, linkWrites);
    witness.state = LearnQueueReason::ready;
    pendingCount = 0;
    pendingRestart = false;primaryWrites=0;primaryMatches=true;afterPrimaryRead=nullptr;
    expireAllocation = false;
    if (reason == 3) {
      source().capture(target, context);
    }
    if (reason == 4)
      break;
    if (reason == 3) { // Next iteration denies allocation entirely.
      JsonAllocator tiny(1, acquire, release);
      result =
          source().linkLearn(target, context, source().authorization(),
                             source().revision(), candidate, witness, tiny);
      TEST_ASSERT_EQUAL(Admission::noMemory, result.admission);
      break;
    }
  }
}
static void
counter_file_retry_uses_shared_special_mode_without_nested_owner_or_link() {
  JsonAllocator memory(65536, acquire, release);
  const Target target{Scope::devices, 7};
  Context context;
  source().capture(target, context);
  Witness witness;
  somfy.transceiver.quarantine = CounterQuarantine::filePending;
  auto result = source().retryLearnFile(
      target, context, source().authorization(), source().revision(), witness);
  TEST_ASSERT_TRUE(result.report.file.saved());
  TEST_ASSERT_EQUAL(1, commits);
  TEST_ASSERT_EQUAL(0, applies);
  TEST_ASSERT_EQUAL(0, linkWrites);
  result = source().retryLearnFile(
      target, context, source().authorization(), source().revision(),
      witness); // Already recovered: protected current-file save.
  TEST_ASSERT_TRUE(result.report.file.saved());
  TEST_ASSERT_EQUAL(2, commits);
  somfy.transceiver.quarantine = CounterQuarantine::nvsUnknown;
  result = source().retryLearnFile(target, context, source().authorization(),
                                   source().revision(), witness);
  TEST_ASSERT_EQUAL(RestartReason::counterUncertain, result.report.refusal);
  TEST_ASSERT_EQUAL(0, linkWrites);
}
static void primary_counter_read_and_final_apply_share_fresh_context_without_nested_owner() {
  const Target target{Scope::devices,7};Context context;source().capture(target,context);
  const auto auth=source().authorization();const auto history=source().revision();
  PrimaryCounterReview reviewed;
  auto read=source().readPrimary(target,context,auth,history,reviewed);
  TEST_ASSERT_EQUAL(Admission::accepted,read.admission);TEST_ASSERT_EQUAL(CounterDomainState::saved,read.state);
  TEST_ASSERT_EQUAL(1000,reviewed.primary);TEST_ASSERT_TRUE(source().primaryCurrent(target,reviewed));
  Witness witness;auto result=source().correctPrimary(target,context,auth,history,reviewed,10,witness);
  TEST_ASSERT_EQUAL(Admission::accepted,result.admission);TEST_ASSERT_TRUE(result.counter.applied);TEST_ASSERT_TRUE(result.counter.file.saved());
  TEST_ASSERT_EQUAL(1,primaryWrites);TEST_ASSERT_EQUAL(0,linkWrites);TEST_ASSERT_EQUAL(0,applies);
}
static void primary_final_read_side_effect_and_witness_changes_refuse_without_correction() {
  for(unsigned which=0;which<6;++which) {
    setUp();const Target target{Scope::devices,7};Context context;source().capture(target,context);
    const auto auth=source().authorization();const auto history=source().revision();PrimaryCounterReview reviewed;
    if(which==0)afterPrimaryRead=[]{++LocalUi::security.generation;};
    if(which==1)afterPrimaryRead=[]{pendingCount=1;};
    if(which==2)afterPrimaryRead=[]{pendingRestart=true;};
    auto read=source().readPrimary(target,context,auth,history,reviewed);
    if(which<3) {TEST_ASSERT_EQUAL(which==0?Admission::stale:Admission::busy,read.admission);TEST_ASSERT_FALSE(reviewed.address);continue;}
    Witness witness;
    if(which==3)witness.state=LearnQueueReason::expired;
    if(which==4)primaryMatches=false;
    GitUpdateOwnerToken owner;if(which==5)TEST_ASSERT_TRUE(gitUpdate.acquireExternal(GitUpdateExternalOwner::manual,owner));
    auto result=source().correctPrimary(target,context,auth,history,reviewed,10,witness);
    if(which==3)TEST_ASSERT_EQUAL(LearnQueueReason::expired,result.freshness);
    if(which==4)TEST_ASSERT_EQUAL(Admission::stale,result.admission);
    if(which==5) {TEST_ASSERT_EQUAL(Admission::busy,result.admission);TEST_ASSERT_TRUE(gitUpdate.releaseExternal(owner));}
    TEST_ASSERT_EQUAL(0,primaryWrites);
  }
}
static void remote_runtime_uses_exact_shared_request_and_fresh_admission() {
  struct Witness:LearnPending {
    bool valid=true,expectLease=true;
    unsigned checks=0;
    LearnQueueReason current(uint64_t) override{
      ++checks;GitUpdateOwnerToken competing;
      const bool acquired=gitUpdate.acquireExternal(GitUpdateExternalOwner::manual,competing);
      if(acquired)gitUpdate.releaseExternal(competing);
      if(expectLease)TEST_ASSERT_FALSE(acquired);
      return valid?LearnQueueReason::ready:LearnQueueReason::expired;
    }
    void execute(uint64_t) override{}void cancelled(LearnQueueReason) override{}
    bool usesRadio()const override{return true;}
  } witness;
  Target target{Scope::devices,7};Context context;TEST_ASSERT_TRUE(source().capture(target,context));
  RemoteProgram program;program.command=somfy_commands::Prog;program.repeats=7;
  TEST_ASSERT_TRUE(source().supportsProgramming(target));
  const EntityAdmin::Authorization auth{true,1,1};
  GitUpdateOwnerToken occupied;TEST_ASSERT_TRUE(gitUpdate.acquireExternal(GitUpdateExternalOwner::manual,occupied));
  auto refused=source().sendProgramming(target,context,auth,1,program,witness);
  TEST_ASSERT_EQUAL(Admission::busy,refused.admission);TEST_ASSERT_EQUAL(0,witness.checks);TEST_ASSERT_EQUAL(0,radioCommands);
  TEST_ASSERT_TRUE(gitUpdate.releaseExternal(occupied));
  auto result=source().sendProgramming(target,context,auth,1,program,witness);
  TEST_ASSERT_EQUAL(Admission::accepted,result.admission);TEST_ASSERT_EQUAL(1,radioCommands);TEST_ASSERT_GREATER_THAN(0,witness.checks);TEST_ASSERT_FALSE(gitUpdate.busy());
  TEST_ASSERT_EQUAL(7,sentRemote.id);TEST_ASSERT_EQUAL(somfy_commands::Prog,sentRemote.command);TEST_ASSERT_EQUAL(7,sentRemote.repeat);
  somfy.shades[0].idle=false;source().sendProgramming(target,context,auth,1,program,witness);TEST_ASSERT_EQUAL(1,radioCommands);
  somfy.shades[0].idle=true;witness.valid=false;source().sendProgramming(target,context,auth,1,program,witness);TEST_ASSERT_EQUAL(1,radioCommands);
  witness.valid=true;pendingRestart=true;source().sendProgramming(target,context,auth,1,program,witness);TEST_ASSERT_EQUAL(1,radioCommands);pendingRestart=false;
  somfy.shades[0].configurationRevision++;source().sendProgramming(target,context,auth,1,program,witness);TEST_ASSERT_EQUAL(1,radioCommands);
  TEST_ASSERT_TRUE(source().capture(target,context));somfy.shades[0].proto=radio_proto::GP_Relay;
  TEST_ASSERT_FALSE(source().supportsProgramming(target));source().sendProgramming(target,context,auth,1,program,witness);TEST_ASSERT_EQUAL(1,radioCommands);
  witness.expectLease=false;auto queued=submitLocalLearn(witness,10);TEST_ASSERT_EQUAL(LearnQueueReason::ready,queued.reason);
  TEST_ASSERT_TRUE(learnedMarker.localRadio);TEST_ASSERT_TRUE(loopItemUsesRadio(learnedMarker));
  TEST_ASSERT_TRUE(cancelLocalLearn(queued.token,witness));
}
int main() {
  UNITY_BEGIN();
  RUN_TEST(remote_runtime_uses_exact_shared_request_and_fresh_admission);
  RUN_TEST(primary_counter_read_and_final_apply_share_fresh_context_without_nested_owner);
  RUN_TEST(primary_final_read_side_effect_and_witness_changes_refuse_without_correction);
  RUN_TEST(checked_link_uses_only_address_counter_and_domain_owned_lease);
  RUN_TEST(final_link_witness_auth_queue_owner_and_memory_are_fresh);
  RUN_TEST(
      counter_file_retry_uses_shared_special_mode_without_nested_owner_or_link);
  RUN_TEST(read_stable_targets_and_every_physical_association);
  RUN_TEST(checked_marker_and_missing_member_under_configuration_lease);
  RUN_TEST(refuses_stale_busy_nonmissing_and_nonidle_without_mutation);
  RUN_TEST(final_authority_after_allocation_and_owner_exclusion);
  RUN_TEST(repeater_work_pending_queue_and_file_retry);
  RUN_TEST(no_memory_invalid_action_and_uncertain_do_not_replay);
  RUN_TEST(parser_preserves_exact_out_of_capacity_legacy_member_id);
  RUN_TEST(full_association_capacity_and_exact_repeater_context);
  RUN_TEST(exact_identity_quarantine_cannot_be_hidden_by_metadata);
  return UNITY_END();
}
