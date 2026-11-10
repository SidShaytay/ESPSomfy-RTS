#include "GitUpdateController.h"
#include <esp_timer.h>
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
#include "GitUpdateBootHealth.h"
#include "SettingsRevisions.h"
extern SomfyShadeController somfy;
extern ConfigSettings settings;
extern LoopQueue loopQueue;
namespace GitUpdateControllerDetail {
struct Source {
  uint64_t operation=0,modelRevision=0,settingsEpoch=0,browserSecurityRevision=0;
  uint32_t session=0,security=0;
  GitUpdateOwnerToken owner;
  RadioInstallToken radio;
  bool queueHeld=false,cleanupFault=false,restartRequested=false,confirmedInstall=false,installRevoked=false;
  uint64_t installAccepted=0,installObserved=0;
} source;
bool authorized(uint32_t session,uint32_t security) {
  const auto current=LocalUi::pollAuthorization();
  return session && security && current.allowed && current.generation==session && current.configurationGeneration==security;
}
bool modelReady() {
  return !gitUpdateBootStatus().pending && modelFilesystemMounted() && settings.settingsUsable() && somfy.modelLoad.usable() &&
      somfy.transceiver.config.loadStatus.usable() && settingsDurability().status().restartAllowed() &&
      !somfy.transceiver.counterEditUncertain();
}
bool current(uint64_t operation) {
  bool credential=source.browserSecurityRevision?source.browserSecurityRevision==securityConfigurationRevision():authorized(source.session,source.security);
  if(source.confirmedInstall){
    const auto micros=esp_timer_get_time();const uint64_t now=micros<0?UINT64_MAX:uint64_t(micros)/1000;
    const auto auth=LocalUi::pollAuthorization();
    credential=!source.installRevoked && now!=UINT64_MAX && now>=source.installObserved && now>=source.installAccepted &&
      now-source.installAccepted<gitUpdateInstallDurationMs && auth.mode!=LocalUi::AuthMode::unavailable &&
      auth.configurationGeneration==source.security;
    if(!credential)source.installRevoked=true;else source.installObserved=now;
  }
  return operation && operation==source.operation && credential && modelReady() &&
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
void capture(uint64_t operation,uint32_t session,uint32_t security) {
  source={};source.operation=operation;source.session=session;source.security=security;
  source.modelRevision=somfy.configurationRevision;source.settingsEpoch=settingsDurability().status().epoch;
}
bool manual(const GitUpdateOwnerToken &owner,uint64_t operation,uint64_t securityRevision) {
  if(!operation || !securityRevision || securityRevision!=securityConfigurationRevision() || !gitUpdate.ownsExternal(owner) || !modelReady())return false;
  capture(operation,0,0);source.browserSecurityRevision=securityRevision;
  return current(operation);
}
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
    GitUpdateControllerDetail::prepare,GitUpdateControllerDetail::cleanup,GitUpdateControllerDetail::written,GitUpdateControllerDetail::manual});}
void loopGitUpdateController(){
  using namespace GitUpdateControllerDetail;
  const auto status=gitUpdate.snapshot();
  if(source.operation && status.busy && status.operation==source.operation && !current(source.operation))gitUpdate.requestCancel(source.operation);
  gitUpdate.loop();
}
bool gitUpdateControllerPaused(){return GitUpdateControllerDetail::source.queueHeld || GitUpdateControllerDetail::source.radio.generation;}
GitUpdateRequest requestLocalGitUpdateCheck(uint32_t session,uint32_t security,uint64_t &operation) {
  if(!GitUpdateControllerDetail::authorized(session,security))return GitUpdateRequest::unavailable;
  const auto result=gitUpdate.requestCheck(operation);
  if(result==GitUpdateRequest::accepted)GitUpdateControllerDetail::capture(operation,session,security);
  return result;
}
GitUpdateRequest requestLocalGitUpdateInstall(const GitUpdateCandidate &candidate,uint32_t session,uint32_t security,uint64_t &operation) {
  using namespace GitUpdateControllerDetail;
  if(!authorized(session,security) || !modelReady())return GitUpdateRequest::unavailable;
  if(source.queueHeld || source.radio.generation || source.cleanupFault)return GitUpdateRequest::busy;
  const auto result=gitUpdate.requestInstall(candidate,operation);
  if(result==GitUpdateRequest::accepted) {
    capture(operation,session,security);source.confirmedInstall=true;
    const auto now=esp_timer_get_time();source.installAccepted=source.installObserved=now<0?UINT64_MAX:uint64_t(now)/1000;
  }
  return result;
}
bool cancelLocalGitUpdate(uint32_t session,uint32_t security,uint64_t operation) {
  const auto &source=GitUpdateControllerDetail::source;
  // Reducing an exact previously authorized operation remains safe on expiry.
  return operation && source.operation==operation && source.security==security && GitUpdateControllerDetail::authorized(session,security) && gitUpdate.requestCancel(operation);
}
#else
void beginGitUpdateController(){}
void loopGitUpdateController(){}
bool gitUpdateControllerPaused(){return false;}
GitUpdateRequest requestLocalGitUpdateCheck(uint32_t,uint32_t,uint64_t &){return GitUpdateRequest::unavailable;}
GitUpdateRequest requestLocalGitUpdateInstall(const GitUpdateCandidate &,uint32_t,uint32_t,uint64_t &){return GitUpdateRequest::unavailable;}
bool cancelLocalGitUpdate(uint32_t,uint32_t,uint64_t){return false;}
#endif
