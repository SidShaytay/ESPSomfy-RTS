#pragma once
#include "GitUpdate.h"
enum class ManualUpdateKind : uint8_t { firmware, application, restore };
struct ManualUpdateCleanup {
  GitUpdateOwnerToken token;
  ManualUpdateKind kind=ManualUpdateKind::firmware;
  bool resourceOpen=false;
};
// One bounded lifecycle beneath the existing GitUpdate owner. Every access is
// serialized by that service lock. Claims exclude cleanup, never hold a lock
// across SDK/file I/O, and never constitute a second updater owner.
class ManualUpdateLifecycle {
  enum class Phase : uint8_t { idle, receiving, queued, awaitingRecovery, awaitingRestart, retired };
  GitUpdateOwnerToken token;
  ManualUpdateKind kind=ManualUpdateKind::firmware;
  Phase phase=Phase::idle;
  uint64_t lastProgress=0,highWater=0;
  bool inFlight=false,resourceOpen=false,everOpened=false,expired=false;
  bool matches(const GitUpdateOwnerToken &expected) const {
    return phase!=Phase::idle && expected.generation && expected.generation==token.generation && expected.kind==token.kind;
  }
  void observe(uint64_t now) {
    if(phase==Phase::idle) return;
    if(now<highWater || now-lastProgress>=timeoutMs) { phase=Phase::retired;expired=true; }
    if(now>highWater) highWater=now;
  }
public:
  static constexpr uint64_t timeoutMs=30000;
  bool begin(const GitUpdateOwnerToken &expected,ManualUpdateKind type,uint64_t now) {
    const auto owner=type==ManualUpdateKind::restore ? GitUpdateExternalOwner::restore : GitUpdateExternalOwner::manual;
    if(phase!=Phase::idle || !expected.generation || expected.kind!=owner || uint8_t(type)>uint8_t(ManualUpdateKind::restore)) return false;
    token=expected;kind=type;phase=Phase::receiving;lastProgress=highWater=now;expired=false;return true;
  }
  bool claim(const GitUpdateOwnerToken &expected,uint64_t now,bool queued=false) {
    if(!matches(expected)) return false;
    observe(now);
    if(inFlight || phase!=(queued ? Phase::queued : Phase::receiving) || (queued && kind!=ManualUpdateKind::restore)) return false;
    inFlight=true;return true;
  }
  bool continuing(const GitUpdateOwnerToken &expected) const { return matches(expected) && phase!=Phase::retired; }
  bool continuing(const GitUpdateOwnerToken &expected,uint64_t now) {
    if(!matches(expected)) return false;
    observe(now);return phase!=Phase::retired;
  }
  bool resource(const GitUpdateOwnerToken &expected,bool opened) {
    if(!matches(expected) || !inFlight) return false;
    resourceOpen=opened;if(opened) everOpened=true;return true;
  }
  // Transfer a completed backup from request callbacks to its FIFO marker.
  bool queue(const GitUpdateOwnerToken &expected) {
    if(!matches(expected) || !inFlight || !resourceOpen || phase!=Phase::receiving || kind!=ManualUpdateKind::restore) return false;
    phase=Phase::queued;return true;
  }
  bool finish(const GitUpdateOwnerToken &expected,uint64_t now,bool progress) {
    if(!matches(expected) || !inFlight) return false;
    observe(now);inFlight=false;
    if(progress && phase!=Phase::retired) lastProgress=now;
    return true;
  }
  // SDK end succeeded: retain the owner across restart publication/admission.
  bool holdRestart(const GitUpdateOwnerToken &expected) {
    if(!matches(expected) || !inFlight || !everOpened || resourceOpen || (phase!=Phase::receiving && !(kind==ManualUpdateKind::restore && phase==Phase::queued))) return false;
    phase=Phase::awaitingRestart;return true;
  }
  bool holdRecovery(const GitUpdateOwnerToken &expected,uint64_t now) {
    if(!matches(expected) || !inFlight || resourceOpen || !everOpened || kind!=ManualUpdateKind::application) return false;
    observe(now);
    if(expired || (phase!=Phase::receiving && phase!=Phase::retired)) return false;
    phase=Phase::awaitingRecovery;return true;
  }
  bool restoreClaimed(const GitUpdateOwnerToken &expected,uint64_t now) {
    if(!matches(expected)) return false;
    observe(now);return phase==Phase::queued && kind==ManualUpdateKind::restore && inFlight && resourceOpen;
  }
  bool claimRecovery(const GitUpdateOwnerToken &expected,uint64_t now) {
    if(!matches(expected)) return false;
    observe(now);
    if(inFlight || kind!=ManualUpdateKind::application ||
       (phase!=Phase::awaitingRecovery && phase!=Phase::awaitingRestart)) return false;
    inFlight=true;return true;
  }
  bool restartHeld(const GitUpdateOwnerToken &expected,uint64_t now) {
    if(!matches(expected)) return false;
    observe(now);return phase==Phase::awaitingRestart && !resourceOpen;
  }
  bool restartReady(const GitUpdateOwnerToken &expected,uint64_t now) {
    if(!matches(expected)) return false;
    observe(now);return phase==Phase::awaitingRestart && !inFlight && !resourceOpen;
  }
  bool handoffRestart(const GitUpdateOwnerToken &expected,uint64_t now) {
    if(!restartReady(expected,now)) return false;
    token={};phase=Phase::idle;inFlight=everOpened=expired=false;lastProgress=highWater=0;return true;
  }
  // Disconnect retires only a request-owned receive, never a transferred file
  // or an already completed OTA awaiting restart. Those still expire below.
  bool retire(const GitUpdateOwnerToken &expected) {
    if(!matches(expected) || phase!=Phase::receiving) return false;
    phase=Phase::retired;return true;
  }
  // Claim cleanup once no SDK/file operation is in flight. The caller must
  // finish owned abort/removal, mark resource closed, then close/release owner.
  ManualUpdateCleanup cleanup(uint64_t now) {
    observe(now);
    if(phase!=Phase::retired || inFlight) return {};
    inFlight=true;return {token,kind,resourceOpen};
  }
  bool close(const GitUpdateOwnerToken &expected) {
    if(!matches(expected) || !inFlight || resourceOpen) return false;
    token={};phase=Phase::idle;inFlight=everOpened=expired=false;lastProgress=highWater=0;return true;
  }
  bool active(const GitUpdateOwnerToken &expected) const { return matches(expected); }
};
static_assert(sizeof(ManualUpdateLifecycle)<=48,"Keep manual transfer lifecycle bounded");
