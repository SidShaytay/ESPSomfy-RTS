#if defined(ARDUINO) && defined(RADIO_SETTINGS_HOST_TEST)
#error "Radio settings host hooks are forbidden in firmware"
#endif
#include "RadioSettingsAdmission.h"
#ifndef RADIO_SETTINGS_HOST_TEST
#include "Somfy.h"
#include "LoopQueue.h"
#include "SafeRestart.h"
#include "GitUpdate.h"
extern SomfyShadeController somfy;
extern LoopQueue loopQueue;
#endif
#include "ModelIdleForAdministration.h"
namespace {
// One loop-owned reducing cleanup record. No work or settings draft is retained.
struct RadioSettingsRelease {
  GitUpdateOwnerToken owner;
  RadioInstallToken radio;
  bool queueSealed=false;
} release;
bool idle() {
  return somfy.modelLoad.usable() && !restartPending() && modelIdleForAdministration(somfy);
}
settings_save_result_t refused(){return {settings_save_state_t::failed,"radio admission"};}
}
void serviceRadioSettingsCleanup() {
  if(!release.owner.generation)return;
  // Never touch another operation's token/resources after ownership loss.
  if(!gitUpdate.ownsExternal(release.owner))return;
  if(release.radio.generation) {
    const auto result=somfy.transceiver.resumeAfterInstall(release.radio);
    if(result==RadioInstallResume::busy)return;
    release.radio={};
  }
  if(release.queueSealed){loopQueue.reopenAfterRefusedRestart();release.queueSealed=false;}
  if(gitUpdate.releaseExternal(release.owner))release={};
}
settings_save_result_t saveRadioConfiguration(Transceiver &radio,const transceiver_config_t &next) {
  serviceRadioSettingsCleanup();
  if(release.owner.generation || &radio!=&somfy.transceiver || !idle())return refused();
  if(!gitUpdate.acquireExternal(GitUpdateExternalOwner::configuration,release.owner))return refused();
  if(!idle() || !loopQueue.sealForRestart()){serviceRadioSettingsCleanup();return refused();}
  release.queueSealed=true;
  if(!radio.pauseForRestore(release.radio)){serviceRadioSettingsCleanup();return refused();}
  if(!gitUpdate.ownsExternal(release.owner) || loopQueue.pending() || !idle()) {
    serviceRadioSettingsCleanup();return refused();
  }
  const auto result=radio.setConfigWhilePaused(next,release.radio,release.owner);
  serviceRadioSettingsCleanup();
  return result;
}
