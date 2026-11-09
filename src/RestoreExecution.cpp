#include "RestoreExecution.h"
#include "RestoreCompletion.h"
#include "ConfigFile.h"
#include "GitUpdate.h"
#include "LoopQueue.h"
#include "ModelIdleForAdministration.h"
#include "SafeRestart.h"
#include "SettingsDurability.h"
#include "SettingsRevisions.h"
#include "ConfigSettings.h"
#include "Somfy.h"
#include <LittleFS.h>
#include <esp_random.h>
#include <esp_timer.h>
#if defined(ESP_PLATFORM)
#include <freertos/FreeRTOS.h>
#else
#include <mutex>
#endif
extern SomfyShadeController somfy;
extern LoopQueue loopQueue;
extern ConfigSettings settings;
namespace RestoreExecutionDetail {
#if defined(ESP_PLATFORM)
portMUX_TYPE mutex=portMUX_INITIALIZER_UNLOCKED;
struct Guard{Guard(){portENTER_CRITICAL(&mutex);}~Guard(){portEXIT_CRITICAL(&mutex);}};
#else
std::mutex mutex;
struct Guard{std::lock_guard<std::mutex> lock;Guard():lock(mutex){}};
#endif
RestoreExecutionSnapshot view;
uint64_t admittedSecurityRevision=0;
bool started=false;
struct Pending {
  GitUpdateOwnerToken source;
  RadioInstallToken radio;
  bool queueHeld=false,deferred=false,faulted=false,grantConsumed=false;
} pending;
static_assert(sizeof(Pending)<=64,"Bounded source-bound cleanup context");
uint64_t now(){const auto value=esp_timer_get_time();return value<0?UINT64_MAX:uint64_t(value)/1000;}
bool same(const GitUpdateOwnerToken &a,const GitUpdateOwnerToken &b){return a.generation==b.generation&&a.kind==b.kind;}
void publish(RestoreExecutionPhase phase,RestoreExecutionFault fault=RestoreExecutionFault::none,
             bool restart=false,RestartReason reason=RestartReason::none){
  Guard guard;
  if(view.generation!=pending.source.generation)return;
  view.phase=phase;view.fault=fault;view.restartScheduled=restart;
  view.restartBlocked=!restart;view.restartReason=reason;
}
RestoreApplyResult outcome(){Guard guard;return view.apply;}
void setOutcome(const RestoreApplyResult &result){Guard guard;if(view.generation==pending.source.generation)view.apply=result;}
void refuse(RestoreRefusal reason){auto result=outcome();result.refusal=reason;setOutcome(result);}
RestoreExecutionPhase appliedPhase(){const auto apply=outcome();return apply.complete?RestoreExecutionPhase::applied:
  apply.partial()?RestoreExecutionPhase::partial:RestoreExecutionPhase::refused;}
void finishSource(RestoreExecutionFault fault){
  const auto source=pending.source;
  // Resource order is deliberate: radio then queue, then the private file and
  // claim, and only afterwards can this exact source hand off to restart.
  if(pending.queueHeld){loopQueue.reopenAfterRefusedRestart();pending.queueHeld=false;}
  if(!LittleFS.remove("/shades.tmp")){
    pending.faulted=true;publish(RestoreExecutionPhase::cleanupFault,RestoreExecutionFault::fileCleanup);return;
  }
  if(!gitUpdate.transferResource(source,false)){
    pending.faulted=true;publish(RestoreExecutionPhase::cleanupFault,RestoreExecutionFault::staleSource);return;
  }
  const auto at=now();
  if(outcome().complete && !pending.deferred && fault==RestoreExecutionFault::none &&
      gitUpdate.transferContinuing(source,at) && gitUpdate.holdTransferRestart(source)){
    if(!gitUpdate.finishTransfer(source,at,true)){
      pending.faulted=true;publish(RestoreExecutionPhase::cleanupFault,RestoreExecutionFault::staleSource);return;
    }
    const auto restart=requestOwnedRestart(source,1000);
    publish(appliedPhase(),restart.admitted?RestoreExecutionFault::none:RestoreExecutionFault::ownership,
            restart.admitted,restart.reason);
    if(!restart.admitted)gitUpdate.releaseRestartSource(source,now());
    pending={};return;
  }
  publish(appliedPhase(),fault==RestoreExecutionFault::none && pending.deferred?
    RestoreExecutionFault::cleanupDeferred:fault);
  if(!gitUpdate.closeTransfer(source) || !gitUpdate.releaseExternal(source)){
    pending.faulted=true;publish(RestoreExecutionPhase::cleanupFault,RestoreExecutionFault::staleSource);return;
  }
  pending={};
}
void unwind(){
  if(!pending.source.generation || pending.faulted)return;
  if(!gitUpdate.ownsExternal(pending.source)){
    pending.faulted=true;publish(RestoreExecutionPhase::cleanupFault,RestoreExecutionFault::staleSource);return;
  }
  if(pending.radio.generation){
    const auto resumed=somfy.transceiver.resumeAfterInstall(pending.radio);
    if(resumed==RadioInstallResume::busy){pending.deferred=true;publish(RestoreExecutionPhase::cleanupPending,RestoreExecutionFault::busy);return;}
    if(resumed==RadioInstallResume::stale){pending.faulted=true;publish(RestoreExecutionPhase::cleanupFault,RestoreExecutionFault::staleSource);return;}
    pending.radio={};
    finishSource(resumed==RadioInstallResume::unavailable?RestoreExecutionFault::radioUnavailable:RestoreExecutionFault::none);
  }else finishSource(RestoreExecutionFault::none);
}
}
bool beginRestoreExecution(){
  {RestoreExecutionDetail::Guard guard;if(RestoreExecutionDetail::started)return RestoreExecutionDetail::view.bootSession!=0;RestoreExecutionDetail::started=true;}
  uint64_t nonce=0;
  for(unsigned i=0;i<4&&!nonce;++i)nonce=((uint64_t(esp_random())<<32)|esp_random())&(UINT64_MAX>>1);
  RestoreExecutionDetail::Guard guard;
  RestoreExecutionDetail::view.bootSession=nonce;
  if(nonce){RestoreExecutionDetail::view.phase=RestoreExecutionPhase::idle;RestoreExecutionDetail::view.fault=RestoreExecutionFault::none;}
  return nonce!=0;
}
bool restoreExecutionReady(){RestoreExecutionDetail::Guard guard;return RestoreExecutionDetail::view.bootSession!=0;}
bool noteRestoreQueued(const GitUpdateOwnerToken &source,uint64_t securityRevision){
  if(!securityRevision || securityRevision!=securityConfigurationRevision() || source.kind!=GitUpdateExternalOwner::restore || !gitUpdate.transferRestoreClaimed(source,RestoreExecutionDetail::now()))return false;
  RestoreExecutionDetail::Guard guard;auto &view=RestoreExecutionDetail::view;
  if(!view.bootSession || source.generation<=view.generation)return false;
  const auto nonce=view.bootSession;view={};view.bootSession=nonce;view.generation=source.generation;
  view.phase=RestoreExecutionPhase::queued;view.fault=RestoreExecutionFault::none;RestoreExecutionDetail::admittedSecurityRevision=securityRevision;return true;
}
void refuseUnpublishedRestore(const GitUpdateOwnerToken &source,RestoreRefusal reason){
  if(source.kind!=GitUpdateExternalOwner::restore)return;
  RestoreExecutionDetail::Guard guard;auto &view=RestoreExecutionDetail::view;
  if(view.generation!=source.generation||view.phase!=RestoreExecutionPhase::queued)return;
  view.phase=RestoreExecutionPhase::refused;view.fault=RestoreExecutionFault::ownership;
  view.apply.refusal=reason;view.restartBlocked=true;
}
namespace RestoreExecutionDetail {
void run(const GitUpdateOwnerToken &source,uint8_t parts){
  if(pending.source.generation || source.kind!=GitUpdateExternalOwner::restore || !restoreExecutionReady())return;
  uint64_t securityRevision=0;
  {Guard guard;if(view.generation!=source.generation || view.phase!=RestoreExecutionPhase::queued)return;securityRevision=admittedSecurityRevision;}
  if(!gitUpdate.claimTransfer(source,now(),true)){
    Guard guard;if(view.generation==source.generation){view.phase=RestoreExecutionPhase::refused;view.fault=RestoreExecutionFault::ownership;}return;
  }
  pending.source=source;publish(RestoreExecutionPhase::running);
  if(!securityRevision || securityRevision!=securityConfigurationRevision() || !settings.Security.loadTrusted()){
    refuse(RestoreRefusal::ownership);unwind();return;
  }
  if(!parts || (parts&~uint8_t(RESTORE_SETTINGS|RESTORE_SHADES|RESTORE_NETWORK|RESTORE_TRANSCEIVER|RESTORE_REPEATERS|RESTORE_MQTT))){refuse(RestoreRefusal::invalid);unwind();return;}
  if(restartPending() || !modelIdleForAdministration(somfy) || !loopQueue.sealForRestart()){
    refuse(RestoreRefusal::busy);unwind();return;
  }
  pending.queueHeld=true;
  if(!somfy.transceiver.pauseForRestore(pending.radio)){
    refuse(RestoreRefusal::busy);unwind();return;
  }
  if(!gitUpdate.transferRestoreClaimed(source,now()) || !modelIdleForAdministration(somfy) ||
      securityRevision!=securityConfigurationRevision() || !settings.Security.loadTrusted()){
    refuse(RestoreRefusal::ownership);unwind();return;
  }
  restore_options_t opts;
  opts.settings=parts&RESTORE_SETTINGS;opts.shades=parts&RESTORE_SHADES;opts.network=parts&RESTORE_NETWORK;
  opts.transceiver=parts&RESTORE_TRANSCEIVER;opts.repeaters=parts&RESTORE_REPEATERS;opts.mqtt=parts&RESTORE_MQTT;
  // Synchronous loop adoption now consumes the verified grant. Its own
  // restore-history invalidation must not revoke cleanup or replay this work.
  pending.grantConsumed=true;
  auto apply=ShadeConfigFile::restore(&somfy,"/shades.tmp",opts,source,pending.radio);
  // Certify every requested part, never just a callee's complete boolean.
  const bool modelRequested=parts&(RESTORE_SHADES|RESTORE_REPEATERS);
  const bool modelVerified=!modelRequested || (apply.modelApplied&&apply.modelFile.saved());
  uint8_t required=0;
  if(parts&RESTORE_SETTINGS)required|=settingsNamespaceBit(SettingsNamespace::cfg)|settingsNamespaceBit(SettingsNamespace::ntp);
  if(parts&RESTORE_NETWORK)required|=settingsNamespaceBit(SettingsNamespace::cfg)|settingsNamespaceBit(SettingsNamespace::ip);
  if(parts&RESTORE_MQTT)required|=settingsNamespaceBit(SettingsNamespace::mqtt);
  if(parts&RESTORE_TRANSCEIVER)required|=settingsNamespaceBit(SettingsNamespace::radio);
#if HAS_ETHERNET
  if(parts&RESTORE_NETWORK)required|=settingsNamespaceBit(SettingsNamespace::ethernet);
#endif
  const uint8_t permitted=required;
  const bool settingsVerified=apply.settings.saved() &&
    (apply.settings.requestedNamespaces&required)==required && !(apply.settings.requestedNamespaces&uint8_t(~permitted));
  if(apply.complete && (apply.refusal!=RestoreRefusal::none || !modelVerified || !settingsVerified)){
    apply.complete=false;apply.refusal=modelVerified?RestoreRefusal::settings:RestoreRefusal::modelStorage;
  }
  setOutcome(apply);if(apply.modelApplied)completeRestoreModel(apply);unwind();
}
}
void runOwnedRestore(const GitUpdateOwnerToken &source,uint8_t parts){RestoreExecutionDetail::run(source,parts);}
bool restoreExecutionHolds(const GitUpdateOwnerToken &source,const RadioInstallToken &radio){
  const auto &p=RestoreExecutionDetail::pending;
  return p.grantConsumed && source.generation && RestoreExecutionDetail::same(source,p.source) && p.queueHeld &&
    radio.generation && radio.generation==p.radio.generation && !p.deferred && !p.faulted;
}
void serviceRestoreCleanup(){RestoreExecutionDetail::unwind();}
RestoreExecutionSnapshot restoreExecutionStatus(){RestoreExecutionDetail::Guard guard;return RestoreExecutionDetail::view;}
const char *restoreExecutionMessage(const RestoreExecutionSnapshot &s){
  if(s.fault==RestoreExecutionFault::radioUnavailable)return "Radio receive could not be resumed; no restart is scheduled.";
  if(s.fault==RestoreExecutionFault::fileCleanup)return "Restore file cleanup failed; no restart is scheduled.";
  if(s.phase==RestoreExecutionPhase::cleanupPending)return "Restore cleanup is pending; no restart is scheduled.";
  if(s.phase==RestoreExecutionPhase::cleanupFault)return "Restore cleanup is unavailable; resources remain protected. No restart is scheduled.";
  if(s.phase==RestoreExecutionPhase::queued)return "Restore queued; nothing has been applied yet.";
  if(s.phase==RestoreExecutionPhase::running)return "Restore is processing; wait for its result.";
  if(s.phase==RestoreExecutionPhase::partial)return "Restore was partly applied; review recovery status. No restart is scheduled.";
  if(s.phase==RestoreExecutionPhase::applied)return s.restartScheduled?"Restore applied; restart scheduled.":"Restore applied; no restart is scheduled.";
  if(s.phase==RestoreExecutionPhase::refused)return "Restore refused; no model was applied.";
  return "Restore is unavailable.";
}
