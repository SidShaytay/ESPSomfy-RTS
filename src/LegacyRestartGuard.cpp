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
  if(rebootDelay.reboot) return {false,RestartReason::pending};
  return {true,RestartReason::none};
}
RestartResult reserveLegacyRestart(GitUpdateOwnerToken &reservation) {
  const GitUpdateOwnerToken source{rebootDelay.externalOwnerGeneration,GitUpdateExternalOwner(rebootDelay.externalOwnerKind)};
  if(!gitUpdate.acquireRestart(reservation,uint64_t(esp_timer_get_time())/1000,source))
    return {false,source.generation ? RestartReason::stale : RestartReason::persistenceBusy};
  // Lock order is the shared updater owner, then the TX queue query. The
  // reservation is retained through actual shutdown, released only on refusal.
  if(somfy.transceiver.counterEditUncertain()) {
    gitUpdate.releaseExternal(reservation);return {false,RestartReason::counterUncertain};
  }
  return {true,RestartReason::none};
}

// Hardware boundary supplied by the controller; called only with the
// non-expiring shared restart owner already reserved.
void performLegacyShutdown();
bool legacyRestartLoop() {
  if(!rebootDelay.reboot || millis()<=static_cast<uint32_t>(rebootDelay.rebootTime)) return false;
  GitUpdateOwnerToken reservation;
  const auto result=reserveLegacyRestart(reservation);
  if(!result.admitted) {
    ESP_LOGW("Restart","Restart refused: %s",restartReason(result.reason));
    clearLegacyRestart(rebootDelay);return false;
  }
  performLegacyShutdown();return true;
}
