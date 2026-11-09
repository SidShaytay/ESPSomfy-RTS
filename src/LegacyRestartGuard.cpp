#include "LegacyRestartGuard.h"
#include "LegacyRestartSource.h"
#include "Somfy.h"
#include "Utils.h"
#include "LoopQueue.h"
#include "SettingsDurability.h"
#include <esp_timer.h>
#include "esp_log.h"
extern SomfyShadeController somfy;
extern rebootDelay_t rebootDelay;
extern LoopQueue loopQueue;
namespace {
RestartResult readiness() {
  const auto stored=settingsDurability().status();
  if(stored.exhausted) return {false,RestartReason::exhausted};
  if(!stored.restartAllowed()) return {false,RestartReason::settingsUncertain};
  if(somfy.transceiver.counterEditUncertain()) return {false,RestartReason::counterUncertain};
  if(loopQueue.pending() || !somfy.transceiver.restartQuiescent()) return {false,RestartReason::queuedOperations};
  for(auto &shade:somfy.shades)
    if(shade.getShadeId()!=255 && (!shade.isIdle() || shade.gpioRelease))
      return {false,RestartReason::activeMovement};
  for(auto &group:somfy.groups)
    if(group.getGroupId()!=255 && group.direction) return {false,RestartReason::activeMovement};
  return {true,RestartReason::none};
}
}

RestartResult legacyGenericRestartAdmission() {
  if(gitUpdate.busy()) return {false,RestartReason::persistenceBusy};
  const auto ready=readiness();if(!ready.admitted)return ready;
  if(snapshotLegacyRestart(rebootDelay).reboot) return {false,RestartReason::pending};
  return {true,RestartReason::none};
}
namespace {
RestartResult reserveFrom(const rebootDelay_t &snapshot,GitUpdateOwnerToken &reservation) {
  const GitUpdateOwnerToken source{snapshot.externalOwnerGeneration,GitUpdateExternalOwner(snapshot.externalOwnerKind)};
  if(!gitUpdate.acquireRestart(reservation,uint64_t(esp_timer_get_time())/1000,source))
    return {false,source.generation ? RestartReason::stale : RestartReason::persistenceBusy};
  // Lock order is the shared updater owner, then the TX queue query. The
  // reservation is retained through actual shutdown, released only on refusal.
  if(somfy.transceiver.counterEditUncertain()) {
    gitUpdate.releaseExternal(reservation);return {false,RestartReason::counterUncertain};
  }
  return {true,RestartReason::none};
}

}
RestartResult reserveLegacyRestart(GitUpdateOwnerToken &reservation) {
  return reserveFrom(snapshotLegacyRestart(rebootDelay),reservation);
}

// Hardware boundary supplied by the controller; called only with the
// non-expiring shared restart owner already reserved.
void performLegacyShutdown();
bool legacyRestartLoop() {
  const auto snapshot=snapshotLegacyRestart(rebootDelay);
  if(!snapshot.reboot || millis()<=static_cast<uint32_t>(snapshot.rebootTime)) return false;
  GitUpdateOwnerToken reservation;
  const auto result=reserveFrom(snapshot,reservation);
  if(!result.admitted) {
    ESP_LOGW("Restart","Restart refused: %s",restartReason(result.reason));
    clearLegacyRestart(rebootDelay,snapshot);return false;
  }
  bool queueSealed=false,radioSealed=false;
  auto final=readiness();
  if(final.admitted) {
    queueSealed=loopQueue.sealForRestart();
    if(!queueSealed) final={false,RestartReason::queuedOperations};
  }
  if(final.admitted) {
    radioSealed=somfy.transceiver.sealForRestart();
    if(!radioSealed) final={false,RestartReason::queuedOperations};
  }
  if(final.admitted) {
    const auto settingsEpoch=settingsDurability().status().epoch;
    const auto modelEpoch=somfy.configurationRevision;
    // The same non-expiring owner and both admission seals span file I/O.
    // No edit, rolling-code correction or command is replayed here.
    const auto file=somfy.commit();
    final=readiness();
    if(!file.saved()) final={false,file.state==ModelCommitState::uncertain ? RestartReason::modelUncertain : RestartReason::modelFailed};
    else if(final.admitted && (settingsEpoch!=settingsDurability().status().epoch || modelEpoch!=somfy.configurationRevision))
      final={false,RestartReason::stale};
  }
  if(!final.admitted) {
    if(radioSealed)somfy.transceiver.reopenAfterRefusedRestart();
    if(queueSealed)loopQueue.reopenAfterRefusedRestart();
    gitUpdate.releaseExternal(reservation);
    clearLegacyRestart(rebootDelay,snapshot);
    ESP_LOGW("Restart","Final restart refused: %s",restartReason(final.reason));
    return false;
  }
  performLegacyShutdown();return true;
}
