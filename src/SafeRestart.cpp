#include "SafeRestart.h"
#include "RestartCoordinator.h"
#include "GitUpdate.h"
#include "Somfy.h"
#include "GroupReadiness.h"
#include "LoopQueue.h"
#include "SettingsDurability.h"
#include <esp_timer.h>
#if defined(ESP_PLATFORM)
#include <freertos/FreeRTOS.h>
#else
#include <mutex>
#endif
extern SomfyShadeController somfy;
extern LoopQueue loopQueue;
// Hardware boundary: invoked only with the same reserved owner and both seals.
void performLegacyShutdown();
namespace SafeRestartDetail {
#if defined(ESP_PLATFORM)
portMUX_TYPE mutex=portMUX_INITIALIZER_UNLOCKED;
struct Guard { Guard(){portENTER_CRITICAL(&mutex);} ~Guard(){portEXIT_CRITICAL(&mutex);} };
#else
std::mutex mutex;
thread_local unsigned criticalDepth=0;
struct Guard {
  std::lock_guard<std::mutex> held;
  Guard():held(mutex){++criticalDepth;}
  ~Guard(){--criticalDepth;}
};
#endif
constexpr uint64_t maximumMailboxAge=60000;
StorageLeaseGate storage;
RestartCoordinator coordinator(storage);
struct Source {
  uint64_t intent=0,submittedAt=0;
  GitUpdateOwnerToken expected,reservation;
  bool owned=false,inFlight=false;
  uint64_t admittedAt=0;
  uint32_t delay=0;
} source;
RestartObservation cached;
uint64_t publishedAt=0,operationEpoch=1;
bool initialized=false,operationExhausted=false;
ModelCommitState recoveryState=ModelCommitState::failed;
uint64_t now() {
  const auto micros=esp_timer_get_time();
  return micros<0 ? UINT64_MAX : uint64_t(micros)/1000;
}
RestartObservation observation() {
  RestartObservation out;
  const auto stored=settingsDurability().status();
  out.settingsEpoch=stored.epoch;
  out.modelConfigurationRevision=somfy.configurationRevision;
  if(stored.exhausted)out.refusal=RestartReason::exhausted;
  else if(!stored.restartAllowed())out.refusal=RestartReason::settingsUncertain;
  else if(somfy.transceiver.counterEditUncertain())out.refusal=RestartReason::counterUncertain;
  else if(!somfy.transceiver.restartQuiescent())out.refusal=RestartReason::queuedOperations;
  for(auto &shade:somfy.shades)
    if(out.refusal==RestartReason::none && shade.getShadeId()!=255 && !shade.idleForAdministration())
      out.refusal=RestartReason::activeMovement;
  for(auto &group:somfy.groups) {
    if(out.refusal!=RestartReason::none || group.getGroupId()==255)continue;
    const auto current=readGroupReadiness(somfy,group);
    if(current.activeMember)out.refusal=RestartReason::activeMovement;
    else if(current.unknownMovement())out.refusal=RestartReason::unknownGroup;
  }
  return out;
}
// Only the owning loop supplies model/settings observations. Accepted work may
// advance the operation epoch concurrently, so merge it under this same lock.
void publish(uint64_t at,RestartObservation current) {
  current.operationEpoch=operationExhausted ? 0 : operationEpoch;
  if(at==UINT64_MAX)current.refusal=RestartReason::clock;
  cached=current;publishedAt=at;initialized=true;
  coordinator.observe(at,current);
}
bool validSource(const GitUpdateOwnerToken &token) {
  return token.generation && (token.kind==GitUpdateExternalOwner::manual || token.kind==GitUpdateExternalOwner::restore);
}
void releaseSource(const Source &old,uint64_t at) {
  if(old.reservation.generation)gitUpdate.releaseExternal(old.reservation);
  else if(old.owned)gitUpdate.releaseRestartSource(old.expected,at);
}
void cleanup(uint64_t at) {
  Source old;
  {
    Guard guard;
    if(!source.intent || source.inFlight || coordinator.snapshot().pending())return;
    old=source;source={};
  }
  releaseSource(old,at);
}
RestartReason modelFailure() {
  return recoveryState==ModelCommitState::uncertain ? RestartReason::modelUncertain : RestartReason::modelFailed;
}
RestartSubmission submit(uint32_t delay,const GitUpdateOwnerToken &expected,bool owned) {
  Guard guard;
  if(owned && !validSource(expected))return RestartSubmission::refused;
  if(source.intent)return RestartSubmission::busy;
  if(!initialized)return RestartSubmission::refused;
  const auto result=coordinator.submit(delay);
  if(result==RestartSubmission::queued) {
    source={coordinator.snapshot().generation,publishedAt,expected,{},owned,false};source.delay=delay;
  }
  return result;
}
// No runtime lock spans owner acquisition, queries, persistence or shutdown.
RestartResult admit(RestartWork work,Source current) {
  uint64_t at=now();
  RestartReason failure=RestartReason::none;
  GitUpdateOwnerToken owner;
  if(!gitUpdate.acquireRestart(owner,at,current.owned ? current.expected : GitUpdateOwnerToken{}))
    failure=current.owned ? RestartReason::stale : RestartReason::persistenceBusy;
  auto fresh=observation();
  if(fresh.refusal==RestartReason::none && loopQueue.pending())fresh.refusal=RestartReason::queuedOperations;
  bool mayWrite=false,recovery=false;
  {
    Guard guard;
    if(source.intent==current.intent)source.reservation=owner;
    publish(at,fresh);
    const auto view=coordinator.snapshot();
    recovery=coordinator.requiresModelRecovery();
    if(current.owned && recovery && failure==RestartReason::none)failure=modelFailure();
    mayWrite=failure==RestartReason::none && view.phase==RestartPhase::queued && view.generation==current.intent;
  }
  if(mayWrite && (somfy.isDirty || recovery)) {
    const auto result=somfy.commit(); // Actual commit records its outcome before returning.
    if(!result.saved())failure=result.state==ModelCommitState::uncertain ? RestartReason::modelUncertain : RestartReason::modelFailed;
  }
  at=now();fresh=observation();
  RestartResult result;
  {
    Guard guard;publish(at,fresh);
    result=coordinator.completeAdmission(work,at,cached,failure);
    if(result.admitted)source.admittedAt=at;
    if(coordinator.release(work))source.inFlight=false;
    else {coordinator.cancel(RestartReason::stale);result={false,RestartReason::stale};}
  }
  cleanup(at);return result;
}
RestartResult request(uint32_t delay,const GitUpdateOwnerToken &expected,bool owned) {
  {Guard guard;if(source.intent)return {false,RestartReason::pending};}
  auto at=now();cleanup(at);auto fresh=observation();
  if(fresh.refusal==RestartReason::none && loopQueue.pending())fresh.refusal=RestartReason::queuedOperations;
  RestartWork work;Source current;RestartResult refusal;
  {
    Guard guard;publish(at,fresh);
    if(owned && !validSource(expected))return {false,RestartReason::invalidRequest};
    if(source.intent)return {false,RestartReason::pending};
    work=coordinator.begin(delay,at,cached);
    if(work.ready()) {
      source={work.intent,at,expected,{},owned,true};source.delay=delay;current=source;
    } else refusal={false,work.refusal};
  }
  if(!work.ready()) {
    if(owned)gitUpdate.releaseRestartSource(expected,at);
    return refusal;
  }
  return admit(work,current);
}
}
RestartResult requestRestart(uint32_t delay){return SafeRestartDetail::request(delay,{},false);}
RestartResult requestOwnedRestart(const GitUpdateOwnerToken &source,uint32_t delay){return SafeRestartDetail::request(delay,source,true);}
RestartSubmission submitRestart(uint32_t delay){return SafeRestartDetail::submit(delay,{},false);}
RestartSubmission submitOwnedRestart(const GitUpdateOwnerToken &source,uint32_t delay){return SafeRestartDetail::submit(delay,source,true);}
bool restartTransferBlocked() {
  SafeRestartDetail::Guard guard;
  return !SafeRestartDetail::initialized || !SafeRestartDetail::coordinator.updatePrecheck(SafeRestartDetail::cached) ||
      SafeRestartDetail::coordinator.requiresModelRecovery();
}
bool restartPending(){SafeRestartDetail::Guard guard;return SafeRestartDetail::coordinator.snapshot().pending();}
RestartSnapshot restartStatus(){SafeRestartDetail::Guard guard;return SafeRestartDetail::coordinator.snapshot();}
void cancelRestart() {
  SafeRestartDetail::Guard guard;
  // Once final take() linearizes, shutdown owns both seals and cannot be cancelled.
  if(SafeRestartDetail::coordinator.snapshot().phase!=RestartPhase::restarting)
    SafeRestartDetail::coordinator.cancel();
}
void noteModelCommit(ModelCommitResult result) {
  SafeRestartDetail::Guard guard;
  SafeRestartDetail::recoveryState=result.state;
  SafeRestartDetail::coordinator.modelCommitted(result);
}
void noteRestartOperationAccepted() {
  using namespace SafeRestartDetail;
  Guard guard;
  if(operationEpoch==UINT64_MAX)operationExhausted=true;
  else ++operationEpoch;
  cached.operationEpoch=operationExhausted ? 0 : operationEpoch;
  coordinator.observe(publishedAt,cached);
}
bool restartLoop() {
  using namespace SafeRestartDetail;
  auto at=now();auto fresh=observation();const bool queuePending=loopQueue.pending()!=0;
  RestartWork work;Source current;bool admission=false;
  {
    Guard guard;
    if(coordinator.snapshot().phase==RestartPhase::queued && queuePending && fresh.refusal==RestartReason::none)
      fresh.refusal=RestartReason::queuedOperations;
    publish(at,fresh);
    const auto phase=coordinator.snapshot().phase;
    if(phase==RestartPhase::queued) {
      if(!source.intent || at<source.submittedAt || at-source.submittedAt>maximumMailboxAge)
        coordinator.cancel(RestartReason::stale);
      else {work=coordinator.prepareQueued(at,cached);admission=true;}
    } else if(phase==RestartPhase::admitted) {
      if(queuePending) {
        // Read/duplicate requests drain normally. Never retain a restart owner
        // indefinitely under continuous polling after its requested deadline.
        if(at>=source.admittedAt && at-source.admittedAt>=source.delay &&
            at-source.admittedAt-source.delay>=5000)coordinator.cancel(RestartReason::queuedOperations);
      } else work=coordinator.prepareFinal(at,cached);
    }
    if(work.ready()){source.inFlight=true;current=source;}
  }
  if(!work.ready()){cleanup(at);return false;}
  if(admission){admit(work,current);return false;}
  RestartReason failure=RestartReason::none;
  bool queueSealed=false,radioSealed=false;
  if(!gitUpdate.ownsExternal(current.reservation))failure=RestartReason::stale;
  fresh=observation();if(failure==RestartReason::none)failure=fresh.refusal;
  if(failure==RestartReason::none) {
    queueSealed=loopQueue.sealForRestart();if(!queueSealed)failure=RestartReason::queuedOperations;
  }
  if(failure==RestartReason::none) {
    radioSealed=somfy.transceiver.sealForRestart();if(!radioSealed)failure=RestartReason::queuedOperations;
  }
  bool mayWrite=false;
  {
    Guard guard;
    const auto view=coordinator.snapshot();
    mayWrite=view.phase==RestartPhase::admitted && view.generation==current.intent;
  }
  if(failure==RestartReason::none && mayWrite) {
    const auto result=somfy.commit();
    if(!result.saved())failure=result.state==ModelCommitState::uncertain ? RestartReason::modelUncertain : RestartReason::modelFailed;
  }
  at=now();fresh=observation();
  if(!gitUpdate.ownsExternal(current.reservation))failure=RestartReason::stale;
  bool shutdown=false;
  {
    Guard guard;publish(at,fresh);
    shutdown=coordinator.completeFinal(work,at,cached,failure);
    if(!shutdown) {
      if(coordinator.release(work))source.inFlight=false;
      else coordinator.cancel(RestartReason::stale);
    }
  }
  if(shutdown){performLegacyShutdown();return true;}
  if(radioSealed)somfy.transceiver.reopenAfterRefusedRestart();
  if(queueSealed)loopQueue.reopenAfterRefusedRestart();
  cleanup(at);return false;
}
