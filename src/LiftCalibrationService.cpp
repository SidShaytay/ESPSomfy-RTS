#include "LiftCalibrationService.h"
#include "MotionDiagnosticsService.h"
#include "LiftCalibrationGate.h"
#include "Somfy.h"
#include "SomfyEditor.h"
#include "ConfigSettings.h"
#include "ModelIdleForAdministration.h"
#include "LoopQueue.h"
#include "GitUpdate.h"
#include "DeviceOps.h"
#include "SafeRestart.h"
#include "BitLength.h"
#include "outputs/ProtoSocket.h"
#include <esp_timer.h>
#include <esp_random.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#ifdef ESPSOMFY_LOCAL_UI
#include "LocalUiAuthorizationAdapter.h"
#include "LocalOperationAdmission.h"
#endif
extern SomfyShadeController somfy;
extern ConfigSettings settings;
extern LoopQueue loopQueue;
namespace LiftCal {
namespace {
uint64_t now() { return uint64_t(esp_timer_get_time()) / 1000; }
std::atomic<TaskHandle_t> ownerTask{nullptr};
std::atomic<bool> dispatching{false};
bool permitted() {
  return dispatching.load(std::memory_order_acquire) && ownerTask.load() == xTaskGetCurrentTaskHandle();
}
class Backend final : public LiftGuideBackend {
  GitUpdateOwnerToken owner;
public:
  bool current(const LiftGuideSource &source) override {
    if(!source.valid() || !settings.settingsUsable() || !somfy.modelLoad.usable() ||
        !somfy.transceiver.config.loadStatus.usable() || source.security != securityConfigurationRevision() ||
        source.radio != deviceSettingsRevision(settings_section_t::radio)) return false;
    if(source.client) return protoSocket.acceptsClientMutation(source.client);
#ifdef ESPSOMFY_LOCAL_UI
    const auto auth = LocalUi::pollAuthorization();
    return !LocalUi::localControlsFrozen() && auth.allowed && source.session == auth.generation &&
      source.configuration == auth.configurationGeneration;
#else
    return false;
#endif
  }
  bool capture(uint8_t id, LiftGuideContext &out) override {
    auto *shade = somfy.getShadeById(id);
    if(!shade || shade->tiltType != tilt_types::none ||
        !(shade->shadeType == shade_types::roller || shade->shadeType == shade_types::blind ||
          shade->shadeType == shade_types::shutter || shade->shadeType == shade_types::awning ||
          shade->shadeType == shade_types::ldrapery || shade->shadeType == shade_types::rdrapery ||
          shade->shadeType == shade_types::cdrapery) ||
        (shade->proto != radio_proto::RTS && shade->proto != radio_proto::RTW && shade->proto != radio_proto::RTV) ||
        !supportedBitLength(shade->bitLength ? shade->bitLength : somfy.transceiver.config.type)) return false;
    out = {id,shade->getRemoteAddress(),somfy.configurationRevision,shade->configurationRevision,somfy.restoreRevision};
    return out.same(out);
  }
  bool acquire(uint64_t &lease) override {
    if(restartPending() || !modelIdleForAdministration(somfy) || !loopQueue.sealForRestart()) return false;
    struct Unseal { ~Unseal() { loopQueue.reopenAfterRefusedRestart(); } } unseal;
    if(!gitUpdate.acquireExternal(GitUpdateExternalOwner::configuration,owner)) return false;
    ownerTask.store(xTaskGetCurrentTaskHandle());
    LiftCalibrationGate::active.store(true,std::memory_order_release);
    RadioUpdateActivity activity;
    if(!somfy.transceiver.updateActivity(activity) || !activity.canPause()) {
      if(gitUpdate.releaseExternal(owner)) {
        owner = {};
        LiftCalibrationGate::active.store(false,std::memory_order_release);
      }
      return false;
    }
    lease = owner.generation;
    return true;
  }
  bool owns(uint64_t lease) override { return lease && lease == owner.generation && gitUpdate.ownsExternal(owner); }
  bool release(uint64_t lease) override {
    if(!owns(lease) || !gitUpdate.releaseExternal(owner)) return false;
    owner = {};
    dispatching.store(false,std::memory_order_release);
    LiftCalibrationGate::active.store(false,std::memory_order_release);
    return true;
  }
  send_result_t send(uint8_t id, somfy_commands command, bool) override {
    if(!owns(owner.generation)) return {};
    auto *shade = somfy.getShadeById(id);
    if(!shade || (command != somfy_commands::Up && command != somfy_commands::Down && command != somfy_commands::My)) return {};
    // Every job still passes the production identity/counter/transport guards.
    dispatching.store(true,std::memory_order_release);
    const auto result = command == somfy_commands::My ? shade->stopCalibrationRun() :
      shade->sendCommandReceipt(command,shade->repeats,0);
    dispatching.store(false,std::memory_order_release);
    return result;
  }
  bool cancel(uint32_t ticket) override { return somfy.transceiver.cancelUnstartedTx(ticket); }
  bool anchor(uint8_t id, bool closed) override {
    auto *shade = somfy.getShadeById(id);
    if(!shade || !owns(owner.generation)) return false;
    shade->confirmEndpoint(closed);
    return true;
  }
  bool anchorHalf(uint8_t id) override {
    auto *shade = somfy.getShadeById(id);
    if(!shade || !owns(owner.generation)) return false;
    shade->setPosition(50,-1);
    return true;
  }
  LiftGuideSave save(uint8_t id, const LiftCalibration &profile, uint32_t up, uint32_t down) override {
    if(!owns(owner.generation)) return {};
    auto *shade = somfy.getShadeById(id);
    if(!shade || !shade->idleForAdministration() || !validLiftCalibration(profile,up,down)) return {};
    model_edit_t edit; edit.type = model_edit_type_t::save_shade;
    edit.doc["shadeId"] = id;
    edit.doc["upTime"] = up; edit.doc["downTime"] = down;
    edit.doc["calibrationKind"] = uint8_t(profile.kind);
    edit.doc["upTakeupMs"] = profile.up.takeupMs;
    edit.doc["upMidpointMs"] = profile.up.midpointMs;
    edit.doc["upSettlingMs"] = profile.up.settlingMs;
    edit.doc["upReversalMs"] = profile.up.reversalMs;
    edit.doc["downTakeupMs"] = profile.down.takeupMs;
    edit.doc["downMidpointMs"] = profile.down.midpointMs;
    edit.doc["downSettlingMs"] = profile.down.settlingMs;
    edit.doc["downReversalMs"] = profile.down.reversalMs;
    if(edit.doc.overflowed()) return {};
    LiftCalibrationGate::modelSaving = true;
    const auto result = applyModelEdit(edit);
    LiftCalibrationGate::modelSaving = false;
    return {result.applied,result.ok};
  }
};
Backend &driver() { static Backend backend; return backend; }
LiftGuideSession &service() {
  static LiftGuideSession instance(driver(),(esp_random() & 0x7fffffffU) + 1);
  return instance;
}
void observedTx(const somfy_tx_event_t &event) { service().txEvent(event); }
void observedFrame(const somfy_frame_t &frame) {
  if(!LiftCalibrationGate::active.load()) return;
  auto *shade = somfy.getShadeById(service().targetId());
  bool affected = shade && shade->findRemote(frame.remoteAddress);
  for(auto &group : somfy.groups)
    if(group.getGroupId() != 255 && group.getRemoteAddress() == frame.remoteAddress &&
        group.hasShadeId(service().targetId())) affected = true;
  // Unrelated received traffic is not a motor observation or an interruption.
  if(!shade || affected) service().interrupt();
}
void hooks() {
  static bool installed = false;
  if(installed) return;
  LiftCalibrationGate::permitted = permitted;
  LiftCalibrationGate::txObserver = observedTx;
  LiftCalibrationGate::frameObserver = observedFrame;
  installed = true;
}
}
LiftGuideSource webSource(uint32_t client) {
  return {client,0,0,securityConfigurationRevision(),deviceSettingsRevision(settings_section_t::radio)};
}
LiftGuideSource localSource(uint32_t session, uint32_t configuration) {
  return {0,session,configuration,securityConfigurationRevision(),deviceSettingsRevision(settings_section_t::radio)};
}
bool available(uint8_t id) { LiftGuideContext context; return driver().capture(id,context); }
bool begin(const LiftGuideSource &source, uint8_t id, LiftKind kind, uint32_t &token) {
  hooks(); return service().begin(source,id,kind,now(),token);
}
bool act(const LiftGuideSource &source, uint32_t token, LiftGuideAction action, bool human) {
  const bool accepted = service().act(source,token,action,now(),human);
  MotionTrace::record(4,uint8_t(action),service().targetId(),0,accepted?1:0,uint32_t(now()));
  return accepted;
}
bool snapshot(const LiftGuideSource &source, uint32_t token, LiftGuideSnapshot &out) {
  return service().snapshot(source,token,now(),out);
}
void poll() { service().poll(now()); }
}
