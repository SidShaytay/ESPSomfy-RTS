#ifdef ESPSOMFY_LOCAL_UI
#include "LocalAdvancedRuntime.h"
#include "CounterEdit.h"
#include "CounterRuntime.h"
#include "GroupReadiness.h"
#include "LocalEntityRuntime.h"
#include "LocalOperationAdmission.h"
#include "LocalUiAuthorizationAdapter.h"
#include "LoopQueue.h"
#include "ModelCommitService.h"
#include "SafeRestart.h"
#include "Somfy.h"
#include "SomfyEditor.h"
#include <esp_timer.h>
#include <stdio.h>
#include <string.h>
extern SomfyShadeController somfy;
extern LoopQueue loopQueue;
extern ShadeRequests shadeRequests;
namespace LocalUi {
namespace Advanced {
using namespace EntityAdmin;
using EntityAdmin::Authorization;
namespace {
class ConfigurationLease {
  GitUpdateOwnerToken token;
  bool held = false;

public:
  ConfigurationLease() {
    held =
        gitUpdate.acquireExternal(GitUpdateExternalOwner::configuration, token);
  }
  ~ConfigurationLease() {
    if (held)
      gitUpdate.releaseExternal(token);
  }
  bool owns() const { return held && gitUpdate.ownsExternal(token); }
};
class RuntimeBackend final : public Backend {
  bool remoteRequest(Target target,const RemoteProgram &program,shade_request_t &request) {
    radio_proto protocol;uint8_t bits;uint32_t address;
    if(target.scope==Scope::devices) {
      auto *shade=somfy.getShadeById(target.id);if(!shade)return false;
      protocol=shade->proto;bits=shade->bitLength;address=shade->getRemoteAddress();
    } else if(target.scope==Scope::groups) {
      auto *group=somfy.getGroupById(target.id);if(!group)return false;
      protocol=group->proto;bits=group->bitLength;address=group->getRemoteAddress();
    } else return false;
    return address&&address<=0xffffff&&prepareRemoteProgram(protocol,bits?bits:somfy.transceiver.config.type,
        target.scope==Scope::groups,target.id,program,request);
  }
  bool validTarget(Target target) {
    if (target.scope == Scope::repeaters)
      return !target.id;
    if (target.scope == Scope::devices)
      return target.id && target.id != 255 && somfy.getShadeById(target.id);
    if (target.scope == Scope::groups)
      return target.id && target.id != 255 && somfy.getGroupById(target.id);
    return false;
  }
  Admission admittedIdentity(Authorization auth, uint64_t revision) {
    const auto now = authorization();
    if (!auth.allowed || !now.allowed)
      return Admission::locked;
    if (!auth.generation || !auth.configurationGeneration || !revision ||
        auth.generation != now.generation ||
        auth.configurationGeneration != now.configurationGeneration ||
        revision != this->revision())
      return Admission::stale;
    return Admission::accepted;
  }
  Admission admit(Target target, const Context &expected, Authorization auth,
                  uint64_t revision,
                  const ConfigurationLease *lease = nullptr) {
    const auto identity = admittedIdentity(auth, revision);
    if (identity != Admission::accepted)
      return identity;
    if ((lease ? !lease->owns() : gitUpdate.busy()) || loopQueue.pending() ||
        restartPending())
      return Admission::busy;
    Context current;
    if (!capture(target, current) || !expected.same(current))
      return Admission::stale;
    return Admission::accepted;
  }
  bool idle(Target target) {
    if (target.scope == Scope::repeaters)
      return somfy.transceiver.restartQuiescent();
    if (target.scope == Scope::devices) {
      auto *shade = somfy.getShadeById(target.id);
      return shade && shade->idleForAdministration();
    }
    auto *group = somfy.getGroupById(target.id);
    if (!group)
      return false;
    // Missing members remain repairable. Only present physical members must
    // be idle; cached group.direction is the last command, not live motion.
    return !readGroupReadiness(somfy, *group).activeMember;
  }
  bool matches(Target target, const Action &action) {
    if (action.kind == ActionKind::setPaired)
      return target.scope == Scope::devices && !action.key && !action.slot;
    Item item;
    const uint8_t count = itemCount(target);
    for (uint8_t i = 0; i < count; i++)
      if (itemAt(target, i, item) && item.slot == action.slot &&
          item.key == action.key) {
        if (action.kind == ActionKind::unlinkRemote)
          return target.scope == Scope::devices;
        if (action.kind == ActionKind::unlinkRepeater)
          return target.scope == Scope::repeaters;
        if (action.kind == ActionKind::removeMissingMember)
          return target.scope == Scope::groups && item.missing &&
                 action.key > 0 && action.key < 255;
      }
    return false;
  }

  bool identityBlocked(Target target, const Action &action) {
    uint32_t address = 0;
    if (action.kind == ActionKind::setPaired) {
      auto *shade = somfy.getShadeById(target.id);
      if (shade)
        address = shade->getRemoteAddress();
    } else if (action.kind == ActionKind::unlinkRemote ||
               action.kind == ActionKind::unlinkRepeater)
      address = action.key;
    return address && somfy.transceiver.counterBlocksIdentity(address);
  }
  static ApplyResult counterRefused() {
    ApplyResult result;
    result.admission = Admission::busy;
    strcpy(result.reason, "Counter recovery is required for this identity.");
    return result;
  }

public:
  bool supportsProgramming(Target target) override {
    shade_request_t request;
    return authorization().allowed&&somfy.modelLoad.usable()&&remoteRequest(target,{},request);
  }
  Admission checkProgrammingContext(Target target,const Context &expected,Authorization auth,uint64_t history) override {
    const auto identity=admittedIdentity(auth,history);if(identity!=Admission::accepted)return identity;
    Context current;
    return supportsProgramming(target)&&capture(target,current)&&expected.same(current)?Admission::accepted:Admission::stale;
  }
  ProgrammingResult sendProgramming(Target target,const Context &expected,Authorization auth,uint64_t history,
      const RemoteProgram &program,LearnPending &witness) override {
    ConfigurationLease lease;
    auto admission=admit(target,expected,auth,history,&lease);
    if(admission!=Admission::accepted)return {shade_request_result_t::radio_refused,admission};
    if(localControlsFrozen()||!idle(target)||!somfy.transceiver.available()||somfy.transceiver.txSuspended()||
       !somfy.transceiver.restartQuiescent()||somfy.transceiver.counterBlocksIdentity(expected.raw.identity))
      return {shade_request_result_t::radio_refused,Admission::busy};
    shade_request_t request;
    if(!remoteRequest(target,program,request))return {shade_request_result_t::radio_refused,Admission::invalid};
    if(witness.current(uint64_t(esp_timer_get_time())/1000)!=LearnQueueReason::ready)
      return {shade_request_result_t::radio_refused,Admission::stale};
    return {shadeRequests.run(request)};
  }
  PrimaryReadResult readPrimary(Target target,const Context &context,
      Authorization auth,uint64_t history,PrimaryCounterReview &out) override {
    out={};
    auto admission=admit(target,context,auth,history);
    if(admission!=Admission::accepted)return {CounterDomainState::invalid,admission};
    if(target.scope!=Scope::devices)return {CounterDomainState::invalid,Admission::invalid};
    const auto state=reviewPrimaryCounter(target.id,out);
    admission=admit(target,context,auth,history);
    if(admission!=Admission::accepted)out={};
    return {state,admission};
  }
  bool primaryCurrent(Target target,const PrimaryCounterReview &review) override {
    return target.scope==Scope::devices && primaryCounterReviewCurrent(target.id,review);
  }
  PrimaryApplyResult correctPrimary(Target target,const Context &context,
      Authorization auth,uint64_t history,const PrimaryCounterReview &review,
      uint16_t code,LearnPending &pending) override {
    auto admission=admit(target,context,auth,history);
    if(admission!=Admission::accepted)return {{},admission};
    if(!primaryCurrent(target,review))return {{},Admission::stale};
    // No configuration lease: the shared correction acquires its counter
    // owner/reservation. This witness remains read-only under that owner.
    class Witness final : public CounterCorrectionWitness {
      LearnPending &pending;
    public:
      LearnQueueReason reason=LearnQueueReason::ready;
      explicit Witness(LearnPending &p):pending(p){}
      bool current() override {
        reason=pending.current(uint64_t(esp_timer_get_time())/1000);
        return reason==LearnQueueReason::ready;
      }
    } witness(pending);
    if(!witness.current())return {{},Admission::accepted,witness.reason};
    const auto result=applyPrimaryCounterCorrection(target.id,code,review,witness);
    return {result,Admission::accepted,witness.reason};
  }
  ReceiveHub &receivedFrames() override { return LocalUi::receiveHub(); }
  LearnQueued queueLearn(LearnPending &pending, uint64_t now) override {
    return submitLocalLearn(pending, now);
  }
  bool cancelLearn(uint64_t token, LearnPending &pending) override {
    return cancelLocalLearn(token, pending);
  }
  Admission checkLearnContext(Target target, const Context &expected,
                              Authorization auth, uint64_t history) override {
    const auto identity = admittedIdentity(auth, history);
    if (identity != Admission::accepted)
      return identity;
    if (target.scope != Scope::devices || !somfy.modelLoad.usable())
      return Admission::stale;
    Context current;
    return capture(target, current) && expected.same(current)
               ? Admission::accepted
               : Admission::stale;
  }
  LearnApplyResult linkLearn(Target target, const Context &context,
                             Authorization expected, uint64_t history,
                             const LearnSelection &candidate,
                             LearnPending &witness,
                             ArduinoJson::Allocator &allocator) override {
    auto admission = admit(target, context, expected, history);
    if (admission != Admission::accepted)
      return {{}, admission};
    admission = checkLearnContext(target, context, expected, history);
    if (admission != Admission::accepted)
      return {{}, admission};
    JsonDocument payload(&allocator), expanded(&allocator);
    payload[protoField(proto_field_t::shadeId).key] = target.id;
    payload[protoField(proto_field_t::remoteAddress).key] = candidate.address;
    payload[protoField(proto_field_t::lastRollingCode).key] =
        candidate.rollingCode;
    proto_error_buffer_t why = {};
    const char *error =
        expandEditPayload(model_edit_type_t::link_remote,
                          payload.as<JsonObjectConst>(), expanded, why);
    if (payload.overflowed() || expanded.overflowed())
      return {{}, Admission::noMemory};
    if (error)
      return {{}, Admission::invalid};
    // No Stage1 configuration lease: checked linking owns its counter lease,
    // fresh NVS/all-alias guard and one model-file commit/fanout.
    admission = admit(target, context, expected, history);
    if (admission != Admission::accepted)
      return {{}, admission};
    admission = checkLearnContext(target, context, expected, history);
    if (admission != Admission::accepted)
      return {{}, admission};
    const auto fresh = witness.current(uint64_t(esp_timer_get_time()) / 1000);
    if (fresh != LearnQueueReason::ready)
      return {{}, Admission::accepted, fresh};
    model_edit_t edit;
    edit.type = model_edit_type_t::link_remote;
    swap(edit.doc, expanded);
    return {applyModelEdit(edit)};
  }
  LearnFileResult retryLearnFile(Target target, const Context &context,
                                 Authorization expected, uint64_t history,
                                 LearnPending &witness) override {
    auto admission = admit(target, context, expected, history);
    if (admission != Admission::accepted)
      return {{}, admission};
    admission = checkLearnContext(target, context, expected, history);
    if (admission != Admission::accepted)
      return {{}, admission};
    const auto fresh = witness.current(uint64_t(esp_timer_get_time()) / 1000);
    if (fresh != LearnQueueReason::ready)
      return {{}, Admission::accepted, fresh};
    if (somfy.transceiver.counterQuarantine() == CounterQuarantine::none) {
      // Another deliberate Commit may already have recovered the counter file.
      // Protect this ordinary current-model serialization with its own lease.
      ConfigurationLease lease;
      admission = admit(target, context, expected, history, &lease);
      if (admission != Admission::accepted)
        return {{}, admission};
      const auto final = witness.current(uint64_t(esp_timer_get_time()) / 1000);
      if (final != LearnQueueReason::ready)
        return {{}, Admission::accepted, final};
      return {applyModelCommit({}, ModelCommitMode::counterFileRecovery)};
    }
    // Known-NVS recovery acquires its own counter owner. Unknown NVS returns
    // the shared typed refusal; neither path may replay linking or NVS writes.
    return {applyModelCommit({}, ModelCommitMode::counterFileRecovery)};
  }
  Authorization authorization() override {
    const auto state = pollAuthorization();
    return {state.allowed && !localControlsFrozen(), state.generation,
            state.configurationGeneration};
  }
  uint64_t revision() override { return somfy.configurationRevision; }
  void activity() override { authorizationActivity(); }
  bool targets(Scope scope, OrderIds &ids, OrderIds &slots,
               uint8_t &count) override {
    if (!authorization().allowed ||
        (scope != Scope::devices && scope != Scope::groups))
      return false;
    bool normalization = false;
    return EntityAdmin::runtimeBackend().readOrder(
        scope == Scope::devices ? Kind::shade : Kind::group, ids, slots, count,
        normalization);
  }
  bool targetLabel(Target target, uint8_t slot, Choice &out) override {
    out = {};
    if (!authorization().allowed || !validTarget(target) ||
        target.scope == Scope::repeaters)
      return false;
    Value value;
    if (!EntityAdmin::runtimeBackend().readOrderLabel(
            target.scope == Scope::devices ? Kind::shade : Kind::group, slot,
            target.id, value) ||
        !memchr(value.text, 0, sizeof(value.text)))
      return false;
    out.value = target.id;
    snprintf(out.label, sizeof(out.label), "#%u %s", unsigned(target.id),
             value.text);
    return true;
  }
  bool capture(Target target, Context &out) override {
    out = {};
    if (!authorization().allowed || !validTarget(target))
      return false;
    out.scope = target.scope;
    out.history = revision();
    if (!out.history)
      return false;
    if (target.scope == Scope::repeaters) {
      static_assert(sizeof(somfy.repeaters) == sizeof(out.repeaters),
                    "Capture all repeater slots");
      for (unsigned i = 0; i < 7; i++)
        out.repeaters[i] = somfy.repeaters[i];
      return true;
    }
    return readRawTargetContext(somfy,
                                {target.scope == Scope::devices
                                     ? EntityKind::device
                                     : EntityKind::group,
                                 target.id},
                                out.raw) &&
           out.raw.targetExists;
  }
  bool paired(Target target, bool &out) override {
    if (!authorization().allowed || target.scope != Scope::devices)
      return false;
    auto *shade = somfy.getShadeById(target.id);
    if (!shade)
      return false;
    out = shade->paired;
    return true;
  }
  uint8_t itemCount(Target target) override {
    if (!authorization().allowed || !validTarget(target))
      return 0;
    uint8_t count = 0;
    if (target.scope == Scope::devices) {
      for (auto &remote : somfy.getShadeById(target.id)->linkedRemotes)
        if (remote.getRemoteAddress())
          ++count;
    } else if (target.scope == Scope::groups) {
      for (auto id : somfy.getGroupById(target.id)->linkedShades)
        if (id && id != 255)
          ++count;
    } else
      for (auto address : somfy.repeaters)
        if (address)
          ++count;
    return count;
  }
  bool itemAt(Target target, uint8_t ordinal, Item &out) override {
    out = {};
    if (!authorization().allowed || !validTarget(target))
      return false;
    if (target.scope == Scope::devices) {
      auto &remotes = somfy.getShadeById(target.id)->linkedRemotes;
      for (uint8_t slot = 0; slot < SOMFY_MAX_LINKED_REMOTES; slot++) {
        const auto address = remotes[slot].getRemoteAddress();
        if (!address || ordinal--)
          continue;
        out.key = address;
        out.rollingCode = remotes[slot].lastRollingCode;
        out.slot = slot;
        snprintf(out.label, sizeof(out.label), "%lu",
                 static_cast<unsigned long>(address));
        return true;
      }
    } else if (target.scope == Scope::groups) {
      auto *group = somfy.getGroupById(target.id);
      for (uint8_t slot = 0; slot < SOMFY_MAX_GROUPED_SHADES; slot++) {
        const auto id = group->linkedShades[slot];
        if (!id || id == 255 || ordinal--)
          continue;
        auto *shade = somfy.getShadeById(id);
        out.key = id;
        out.slot = slot;
        out.missing = !shade;
        snprintf(out.label, sizeof(out.label), "#%u %s", unsigned(id),
                 shade ? "Device" : "Missing device");
        return true;
      }
    } else
      for (uint8_t slot = 0; slot < SOMFY_MAX_REPEATERS; slot++) {
        const auto address = somfy.repeaters[slot];
        if (!address || ordinal--)
          continue;
        out.key = address;
        out.slot = slot;
        snprintf(out.label, sizeof(out.label), "%lu",
                 static_cast<unsigned long>(address));
        return true;
      }
    return false;
  }
  ApplyResult apply(Target target, const Action &action, const Context &context,
                    Authorization expected, uint64_t revision,
                    ArduinoJson::Allocator &allocator) override {
    auto admission = admit(target, context, expected, revision);
    if (admission != Admission::accepted)
      return {{}, admission};
    if (!idle(target))
      return {{}, Admission::busy};
    if (!matches(target, action))
      return {{}, Admission::invalid};
    if (identityBlocked(target, action))
      return counterRefused();
    ConfigurationLease lease;
    admission = admit(target, context, expected, revision, &lease);
    if (admission != Admission::accepted)
      return {{}, admission};
    JsonDocument doc(&allocator);
    model_edit_type_t type;
    switch (action.kind) {
    case ActionKind::setPaired:
      type = model_edit_type_t::set_paired;
      doc["shadeId"] = target.id;
      doc["paired"] = action.paired;
      break;
    case ActionKind::unlinkRemote:
      type = model_edit_type_t::unlink_remote;
      doc["shadeId"] = target.id;
      doc["remoteAddress"] = action.key;
      break;
    case ActionKind::unlinkRepeater:
      type = model_edit_type_t::unlink_repeater;
      doc["remoteAddress"] = action.key;
      break;
    case ActionKind::removeMissingMember:
      type = model_edit_type_t::unlink_from_group;
      doc["groupId"] = target.id;
      doc["shadeId"] = action.key;
      break;
    default:
      return {{}, Admission::invalid};
    }
    if (doc.overflowed())
      return {{}, Admission::noMemory};
    admission = admit(target, context, expected, revision, &lease);
    if (admission != Admission::accepted)
      return {{}, admission};
    if (!idle(target))
      return {{}, Admission::busy};
    if (!matches(target, action))
      return {{}, Admission::stale};
    if (identityBlocked(target, action))
      return counterRefused();
    model_edit_t edit;
    edit.type = type;
    swap(edit.doc, doc);
    return {applyModelEdit(edit)};
  }
  CommitResult retryFile(Target target, const Context &context,
                         Authorization expected, uint64_t revision) override {
    auto admission = admit(target, context, expected, revision);
    if (admission != Admission::accepted)
      return {{}, admission};
    ConfigurationLease lease;
    admission = admit(target, context, expected, revision, &lease);
    if (admission != Admission::accepted)
      return {{}, admission};
    // Current model serialization only. No mutation, counter write or RF.
    return {applyModelCommit().file};
  }
};
static_assert(sizeof(RuntimeBackend) <= 16,
              "No retained Advanced model or context");
} // namespace
Backend &runtimeBackend() {
  static RuntimeBackend source;
  return source;
}
} // namespace Advanced
} // namespace LocalUi
#endif
