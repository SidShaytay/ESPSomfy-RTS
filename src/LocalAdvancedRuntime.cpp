#ifdef ESPSOMFY_LOCAL_UI
#include "LocalAdvancedRuntime.h"
#include "GroupReadiness.h"
#include "LegacyRestartSource.h"
#include "LocalEntityRuntime.h"
#include "LocalOperationAdmission.h"
#include "LocalUiAuthorizationAdapter.h"
#include "LoopQueue.h"
#include "ModelCommitService.h"
#include "Somfy.h"
#include "SomfyEditor.h"
#include "Utils.h"
#include <stdio.h>
#include <string.h>
extern SomfyShadeController somfy;
extern LoopQueue loopQueue;
extern rebootDelay_t rebootDelay;
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
  bool validTarget(Target target) {
    if (target.scope == Scope::repeaters)
      return !target.id;
    if (target.scope == Scope::devices)
      return target.id && target.id != 255 && somfy.getShadeById(target.id);
    if (target.scope == Scope::groups)
      return target.id && target.id != 255 && somfy.getGroupById(target.id);
    return false;
  }
  Admission admit(Target target, const Context &expected, Authorization auth,
                  uint64_t revision,
                  const ConfigurationLease *lease = nullptr) {
    const auto now = authorization();
    if (!auth.allowed || !now.allowed)
      return Admission::locked;
    if (!auth.generation || !auth.configurationGeneration || !revision ||
        auth.generation != now.generation ||
        auth.configurationGeneration != now.configurationGeneration ||
        revision != this->revision())
      return Admission::stale;
    if ((lease ? !lease->owns() : gitUpdate.busy()) || loopQueue.pending() ||
        snapshotLegacyRestart(rebootDelay).reboot)
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
