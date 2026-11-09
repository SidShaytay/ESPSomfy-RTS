#include "LegacyRestartGuard.h"
#include "LegacyRestartSource.h"
#include "Somfy.h"
#include "Utils.h"
#include <esp_timer.h>
#include "esp_log.h"
extern SomfyShadeController somfy;
extern rebootDelay_t rebootDelay;
RestartResult legacyGenericRestartAdmission() {
  if(gitUpdate.busy()) return {false,RestartReason::persistenceBusy};
  if(somfy.transceiver.counterEditUncertain()) return {false,RestartReason::counterUncertain};
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
  performLegacyShutdown();return true;
}
