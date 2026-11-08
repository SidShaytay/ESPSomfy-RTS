#include "RestartCoordinator.h"
RestartObservation RestartCoordinator::current(RestartObservation o) const {
  o.modelEpoch=modelEpoch;
  if(modelExhausted) o.refusal=RestartReason::exhausted;
  return o;
}
void RestartCoordinator::observe(uint64_t now,RestartObservation o) {
  published=current(o);policy.observe(now,published);
}
RestartSubmission RestartCoordinator::submit(uint32_t delay) {
  if(policy.snapshot().pending()) return RestartSubmission::busy;
  if(storage.busy()) {
    if(!inFlight) policy.cancel(RestartReason::persistenceBusy);
    return RestartSubmission::refused;
  }
  return policy.submit(delay,published);
}
RestartWork RestartCoordinator::reserve() {
  if(inFlight) return {0,{},RestartReason::pending};
  StorageLease token;
  if(!storage.acquire(StorageUse::restart,token)) {
    policy.cancel(RestartReason::persistenceBusy);return {0,{},RestartReason::persistenceBusy};
  }
  inFlight=policy.snapshot().generation;
  return {inFlight,token};
}
bool RestartCoordinator::owns(const RestartWork &w) const {
  return w.intent && w.intent==inFlight && w.intent==policy.snapshot().generation &&
    w.storage.use==StorageUse::restart && storage.owns(w.storage);
}
RestartWork RestartCoordinator::begin(uint32_t delay,uint64_t now,RestartObservation o) {
  observe(now,o);
  const auto submitted=submit(delay);
  if(submitted!=RestartSubmission::queued) return {0,{},submitted==RestartSubmission::busy ? RestartReason::pending :
      (inFlight ? RestartReason::persistenceBusy : policy.snapshot().reason)};
  return reserve();
}
RestartWork RestartCoordinator::prepareQueued(uint64_t now,RestartObservation o) {
  observe(now,o);
  if(policy.snapshot().phase!=RestartPhase::queued) return {0,{},policy.snapshot().reason};
  return reserve();
}
RestartResult RestartCoordinator::completeAdmission(const RestartWork &w,uint64_t now,RestartObservation o,RestartReason failure) {
  if(!owns(w)) return {false,RestartReason::stale};
  if(failure!=RestartReason::none) policy.cancel(failure);
  const bool admitted=policy.admitQueued(now,current(o),w.intent);
  return {admitted,admitted ? RestartReason::none : policy.snapshot().reason};
}
RestartWork RestartCoordinator::prepareFinal(uint64_t now,RestartObservation o) {
  observe(now,o);
  if(!policy.due(now,published)) return {0,{},policy.snapshot().reason};
  return reserve();
}
bool RestartCoordinator::completeFinal(const RestartWork &w,uint64_t now,RestartObservation o,RestartReason failure) {
  if(!owns(w)) return false;
  if(failure!=RestartReason::none) policy.cancel(failure);
  return policy.take(now,current(o));
}
bool RestartCoordinator::release(const RestartWork &w) {
  if(!owns(w)) return false;
  if(!storage.release(w.storage)) return false;
  inFlight=0;return true;
}
void RestartCoordinator::modelCommitted(ModelCommitResult result) {
  modelRecovery=!result.saved();
  if(result.saved()) return;
  if(modelEpoch==UINT64_MAX) modelExhausted=true;
  else ++modelEpoch;
  published.modelEpoch=modelEpoch;
  if(modelExhausted) published.refusal=RestartReason::exhausted;
  if(policy.snapshot().pending()) policy.cancel(result.state==ModelCommitState::uncertain ? RestartReason::modelUncertain : RestartReason::modelFailed);
}
bool RestartCoordinator::acquireUpdate(RestartObservation observation,StorageLease &token) {
  const auto o=current(observation);
  if(policy.snapshot().pending() || o.refusal!=RestartReason::none || o.operationsPending || !o.settingsEpoch || !o.modelEpoch) return false;
  return storage.acquire(StorageUse::exclusiveUpdate,token);
}
