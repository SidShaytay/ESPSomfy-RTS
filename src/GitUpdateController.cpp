#include "GitUpdateController.h"
#if defined(ESPSOMFY_LILYGO_T_EMBED_CC1101) && defined(ESPSOMFY_LOCAL_UI)
#include "Somfy.h"
#include "ConfigSettings.h"
#include "LoopQueue.h"
#include "SafeRestart.h"
#include "ModelIdleForAdministration.h"
#include "ModelLoadSnapshot.h"
#include "SettingsDurability.h"
#include "LocalUiAuthorizationAdapter.h"
#include "LocalControlQueue.h"
extern SomfyShadeController somfy;
extern ConfigSettings settings;
extern LoopQueue loopQueue;
namespace GitUpdateControllerDetail {
struct Source {
  uint64_t operation=0,modelRevision=0,settingsEpoch=0;
  uint32_t session=0,security=0;
  GitUpdateOwnerToken owner;
  RadioInstallToken radio;
  bool queueHeld=false,cleanupFault=false,restartRequested=false;
} source;
bool authorized(uint32_t session,uint32_t security) {
  const auto current=LocalUi::pollAuthorization();
  return session && security && current.allowed && current.generation==session && current.configurationGeneration==security;
}
bool modelReady() {
  return modelFilesystemMounted() && settings.settingsUsable() && somfy.modelLoad.usable() &&
      somfy.transceiver.config.loadStatus.usable() && settingsDurability().status().restartAllowed() &&
      !somfy.transceiver.counterEditUncertain();
}
bool current(uint64_t operation) {
  return operation && operation==source.operation && authorized(source.session,source.security) && modelReady() &&
      source.modelRevision && somfy.configurationRevision==source.modelRevision &&
      settingsDurability().status().epoch==source.settingsEpoch;
}
GitUpdateReadAdmission admit() {
  RadioUpdateActivity activity;
  return {modelReady() && !restartPending() && !LocalUi::localControlPending() && !loopQueue.pending() &&
      modelIdleForAdministration(somfy) && somfy.transceiver.updateActivity(activity) && activity.canPause()};
}
GitUpdateInstallPreparation prepare(const GitUpdateOwnerToken &owner) {
  source.owner=owner;
  if(!gitUpdate.ownsExternal(owner) || !current(source.operation) || !admit().allowed || !loopQueue.sealForRestart())return {};
  source.queueHeld=true;
  if(!somfy.transceiver.pauseForInstall(source.radio))return {false,source.radio};
  if(!current(source.operation) || !loopQueue.restartSealHeld() || !somfy.transceiver.installPauseHeld(source.radio))return {false,source.radio};
  const auto saved=somfy.commit(); // Existing checked staging/readback/replace writer; no command replay.
  return {saved.saved() && current(source.operation) && loopQueue.restartSealHeld() &&
      somfy.transceiver.installPauseHeld(source.radio),source.radio};
}
bool cleanup(const GitUpdateOwnerToken &owner,const RadioInstallToken &radio) {
  if(!gitUpdate.ownsExternal(owner) || source.cleanupFault)return false;
  if(!source.owner.generation) { // Preparation never began; no physical resources were acquired.
    if(source.radio.generation || source.queueHeld)return false;
    source={};return true;
  }
  if(owner.generation!=source.owner.generation || owner.kind!=source.owner.kind || radio.generation!=source.radio.generation)return false;
  if(source.restartRequested && restartPending())cancelRestart();
  if(source.queueHeld && !loopQueue.restartSealHeld()){source.cleanupFault=true;return false;}
  if(source.radio.generation) {
    const auto result=somfy.transceiver.resumeAfterInstall(source.radio);
    if(result==RadioInstallResume::busy)return false;
    if(result==RadioInstallResume::stale){source.cleanupFault=true;return false;}
    source.radio={};
  }
  if(source.queueHeld)loopQueue.reopenAfterRefusedRestart();
  source={};return true;
}
bool activate(uint64_t operation){return current(operation) && gitUpdate.activateInstall(operation);}
void failed(uint64_t operation,RestartReason,bool mayClean) {
  if(operation!=source.operation)return;
  gitUpdate.finishInstallRestart(operation,mayClean);
  if(!mayClean)source.cleanupFault=true;
}
void written(const GitUpdateOwnerToken &owner,const RadioInstallToken &radio) {
  const InstallRestartSource retained{radio,source.operation,current,activate,failed};
  const auto result=requestInstallRestart(owner,retained);
  source.restartRequested=result.admitted;
  if(!result.admitted)gitUpdate.finishInstallRestart(source.operation,true);
}
}
void beginGitUpdateController(){gitUpdate.begin(GitUpdateControllerDetail::admit,{GitUpdateControllerDetail::current,
    GitUpdateControllerDetail::prepare,GitUpdateControllerDetail::cleanup,GitUpdateControllerDetail::written});}
void loopGitUpdateController(){gitUpdate.loop();}
bool gitUpdateControllerPaused(){return GitUpdateControllerDetail::source.queueHeld || GitUpdateControllerDetail::source.radio.generation;}
GitUpdateRequest requestLocalGitUpdateCheck(uint32_t session,uint32_t security) {
  if(!GitUpdateControllerDetail::authorized(session,security))return GitUpdateRequest::unavailable;
  return gitUpdate.requestCheck();
}
GitUpdateRequest requestLocalGitUpdateInstall(const GitUpdateCandidate &candidate,uint32_t session,uint32_t security,uint64_t &operation) {
  using namespace GitUpdateControllerDetail;
  if(!authorized(session,security) || !modelReady())return GitUpdateRequest::unavailable;
  if(source.queueHeld || source.radio.generation || source.cleanupFault)return GitUpdateRequest::busy;
  const auto result=gitUpdate.requestInstall(candidate,operation);
  if(result==GitUpdateRequest::accepted) {
    source={};source.operation=operation;source.session=session;source.security=security;
    source.modelRevision=somfy.configurationRevision;source.settingsEpoch=settingsDurability().status().epoch;
  }
  return result;
}
bool cancelLocalGitUpdate(uint32_t session,uint32_t security) {
  return GitUpdateControllerDetail::authorized(session,security) && gitUpdate.requestCancel();
}
#else
void beginGitUpdateController(){}
void loopGitUpdateController(){}
bool gitUpdateControllerPaused(){return false;}
GitUpdateRequest requestLocalGitUpdateCheck(uint32_t,uint32_t){return GitUpdateRequest::unavailable;}
GitUpdateRequest requestLocalGitUpdateInstall(const GitUpdateCandidate &,uint32_t,uint32_t,uint64_t &){return GitUpdateRequest::unavailable;}
bool cancelLocalGitUpdate(uint32_t,uint32_t){return false;}
#endif
