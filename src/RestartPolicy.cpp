#include "RestartPolicy.h"
namespace {
constexpr uint32_t maximumDelay=60000;
bool validDelay(uint32_t delay,uint64_t now=0) { return delay && delay<=maximumDelay && now<=UINT64_MAX-delay; }
}
const char *restartReason(RestartReason reason) {
  static const char *const text[]={"","Settings storage is uncertain; save current RUNNING settings before Restart.",
    "Model file save failed; save the current model, then explicitly Restart.",
    "Model file storage is uncertain; save the current model, then explicitly Restart.",
    "Operations remain queued or active; finish them, then explicitly Restart.",
    "A restart request is already pending.","Restart request expired after state changed; explicitly request Restart again.",
    "Restart cancelled.","Restart history is exhausted.","Monotonic restart clock moved backward.",
    "Invalid restart request.","Restart admission is not ready.","Storage or update I/O is active; explicitly request Restart again when finished.",
    "Counter storage is uncertain; managed restart and affected identity commands are blocked. No automatic counter recovery is available.",
    "Movement or a GPIO pulse is active; wait for it to finish before Restart."};
  const auto index=static_cast<unsigned>(reason);
  return index<sizeof(text)/sizeof(*text) ? text[index] : "Restart refused.";
}
RestartReason RestartPolicy::refusal(const RestartObservation &o) const {
  if(!o.settingsEpoch || !o.modelEpoch) return RestartReason::exhausted;
  if(o.refusal!=RestartReason::none) return o.refusal;
  return o.operationsPending ? RestartReason::queuedOperations : RestartReason::none;
}
bool RestartPolicy::clock(uint64_t now) {
  if(now<highWater) { cancel(RestartReason::clock);return false; }
  highWater=now;return true;
}
bool RestartPolicy::observe(uint64_t now,const RestartObservation &o) {
  if(!clock(now)) return false;
  if(!value.pending()) return true;
  const auto reason=refusal(o);
  if(reason!=RestartReason::none) cancel(reason);
  else if(value.settingsEpoch!=o.settingsEpoch || value.modelEpoch!=o.modelEpoch) cancel(RestartReason::stale);
  return true;
}
RestartSubmission RestartPolicy::submit(uint32_t delayMs,const RestartObservation &o) {
  if(value.pending()) return RestartSubmission::busy;
  const auto reason=generation==UINT64_MAX ? RestartReason::exhausted : (validDelay(delayMs) ? refusal(o) : RestartReason::invalidRequest);
  if(reason!=RestartReason::none) { value.phase=RestartPhase::refused;value.reason=reason;return RestartSubmission::refused; }
  value={RestartPhase::queued,RestartReason::none,o.settingsEpoch,o.modelEpoch,++generation};delay=delayMs;
  return RestartSubmission::queued;
}
bool RestartPolicy::admitQueued(uint64_t now,const RestartObservation &o,uint64_t expected) {
  if(!expected || value.generation!=expected) return false;
  observe(now,o);
  if(value.phase!=RestartPhase::queued) return false;
  if(!validDelay(delay,now)) { cancel(RestartReason::invalidRequest);return false; }
  value.phase=RestartPhase::admitted;deadline=now+delay;return true;
}
void RestartPolicy::cancel(RestartReason reason) { value.phase=RestartPhase::cancelled;value.reason=reason; }
bool RestartPolicy::due(uint64_t now,const RestartObservation &o) {
  observe(now,o);return value.phase==RestartPhase::admitted && now>=deadline;
}
bool RestartPolicy::take(uint64_t now,const RestartObservation &o) {
  if(!due(now,o)) return false;
  value.phase=RestartPhase::restarting;return true;
}
