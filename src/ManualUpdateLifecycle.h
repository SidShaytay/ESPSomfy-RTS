#pragma once
#include "GitUpdate.h"
enum class ManualUpdateKind : uint8_t { firmware, application, restore };
struct ManualUpdateCleanup {
  GitUpdateOwnerToken token;
  ManualUpdateKind kind=ManualUpdateKind::firmware;
  bool resourceOpen=false,filesystemUnavailable=false;
};
// Prepared file admission binds one nonce, security history and exact byte count.
struct ManualApplicationSource {
  uint64_t nonce=0,securityRevision=0;
  uint32_t bytes=0;
  bool valid()const{return nonce&&securityRevision&&bytes;}
  bool same(const ManualApplicationSource &other)const{
    return valid()&&other.valid()&&nonce==other.nonce&&securityRevision==other.securityRevision&&bytes==other.bytes;
  }
};
// One bounded lifecycle beneath the existing GitUpdate owner. Every access is
// serialized by that service lock. Claims exclude cleanup, never hold a lock
// across SDK/file I/O, and never constitute a second updater owner.
class ManualUpdateLifecycle {
  enum class Phase : uint8_t { idle, receiving, preparing, prepared, queued, awaitingRecovery, awaitingRestart, retired };
  GitUpdateOwnerToken token;
  ManualUpdateKind kind=ManualUpdateKind::firmware;
  Phase phase=Phase::idle;
  uint64_t lastProgress=0,highWater=0;
  ManualApplicationSource applicationSource;
  bool inFlight=false,resourceOpen=false,everOpened=false,expired=false,filesystemUnavailable=false;
  bool matches(const GitUpdateOwnerToken &expected) const {
    return phase!=Phase::idle && expected.generation && expected.generation==token.generation && expected.kind==token.kind;
  }
  void observe(uint64_t now) {
    if(phase==Phase::idle) return;
    if(now<highWater || now-lastProgress>=timeoutMs) {
      expired=true;
      // Retain only reducing mount recovery; expired sources never regain receive claims.
      if(phase!=Phase::awaitingRecovery || !filesystemUnavailable)phase=Phase::retired;
    }
    if(now>highWater) highWater=now;
  }
public:
  static constexpr uint64_t timeoutMs=30000;
  bool begin(const GitUpdateOwnerToken &expected,ManualUpdateKind type,uint64_t now) {
    const auto owner=type==ManualUpdateKind::restore ? GitUpdateExternalOwner::restore : GitUpdateExternalOwner::manual;
    if(phase!=Phase::idle || !expected.generation || expected.kind!=owner || uint8_t(type)>uint8_t(ManualUpdateKind::restore)) return false;
    token=expected;kind=type;phase=Phase::receiving;lastProgress=highWater=now;expired=false;return true;
  }
  bool prepareApplication(const GitUpdateOwnerToken &expected,const ManualApplicationSource &source,uint64_t now) {
    if(!matches(expected)||kind!=ManualUpdateKind::application||!source.valid()||inFlight||resourceOpen||everOpened||filesystemUnavailable)return false;
    observe(now);if(phase!=Phase::receiving||applicationSource.valid())return false;
    applicationSource=source;phase=Phase::preparing;return true;
  }
  bool claimPreparation(const GitUpdateOwnerToken &expected,const ManualApplicationSource &source,uint64_t now) {
    if(!matches(expected)||!applicationSource.same(source))return false;
    observe(now);if(phase!=Phase::preparing||inFlight)return false;
    inFlight=true;return true;
  }
  bool applicationPrepared(const GitUpdateOwnerToken &expected) {
    if(!matches(expected)||phase!=Phase::preparing||!inFlight||!filesystemUnavailable||resourceOpen)return false;
    phase=Phase::prepared;return true;
  }
  bool claimPreparedApplication(const GitUpdateOwnerToken &expected,const ManualApplicationSource &source,uint64_t now) {
    if(!matches(expected)||!applicationSource.same(source))return false;
    observe(now);if(phase!=Phase::prepared||inFlight||!filesystemUnavailable||resourceOpen)return false;
    phase=Phase::receiving;inFlight=true;return true;
  }
  bool cancelPreparation(const GitUpdateOwnerToken &expected,const ManualApplicationSource &source) {
    if(!matches(expected)||!applicationSource.same(source)||(phase!=Phase::preparing&&phase!=Phase::prepared))return false;
    phase=Phase::retired;return true;
  }
  bool claim(const GitUpdateOwnerToken &expected,uint64_t now,bool queued=false) {
    if(!matches(expected)) return false;
    observe(now);
    if(inFlight || phase!=(queued ? Phase::queued : Phase::receiving) || (queued && kind!=ManualUpdateKind::restore)) return false;
    inFlight=true;return true;
  }
  bool continuing(const GitUpdateOwnerToken &expected) const { return matches(expected) && phase!=Phase::retired && !expired; }
  bool continuing(const GitUpdateOwnerToken &expected,uint64_t now) {
    if(!matches(expected)) return false;
    observe(now);return phase!=Phase::retired && !expired;
  }
  bool resource(const GitUpdateOwnerToken &expected,bool opened) {
    if(!matches(expected) || !inFlight || (opened && kind==ManualUpdateKind::application && phase!=Phase::receiving)) return false;
    resourceOpen=opened;if(opened) everOpened=true;return true;
  }
  // Separate from an owned Update object: failed SDK begin still owes a
  // remount, while cleanup must not abort an SDK it never acquired.
  bool filesystem(const GitUpdateOwnerToken &expected,bool unavailable) {
    if(!matches(expected) || !inFlight || kind!=ManualUpdateKind::application || resourceOpen) return false;
    if(unavailable && ((phase!=Phase::receiving&&phase!=Phase::preparing) || everOpened)) return false;
    filesystemUnavailable=unavailable;return true;
  }
  // Transfer a completed backup from request callbacks to its FIFO marker.
  bool queue(const GitUpdateOwnerToken &expected) {
    if(!matches(expected) || !inFlight || !resourceOpen || phase!=Phase::receiving || kind!=ManualUpdateKind::restore) return false;
    phase=Phase::queued;return true;
  }
  bool finish(const GitUpdateOwnerToken &expected,uint64_t now,bool progress) {
    if(!matches(expected) || !inFlight) return false;
    observe(now);inFlight=false;
    if(progress && phase!=Phase::retired && !expired) lastProgress=now;
    return true;
  }
  // SDK end succeeded: retain the owner across restart publication/admission.
  bool holdRestart(const GitUpdateOwnerToken &expected) {
    if(!matches(expected) || !inFlight || !everOpened || resourceOpen || (phase!=Phase::receiving && !(kind==ManualUpdateKind::restore && phase==Phase::queued))) return false;
    phase=Phase::awaitingRestart;return true;
  }
  bool holdRecovery(const GitUpdateOwnerToken &expected,uint64_t now) {
    if(!matches(expected) || !inFlight || resourceOpen || (!everOpened && !filesystemUnavailable) || kind!=ManualUpdateKind::application) return false;
    observe(now);
    if((expired && !filesystemUnavailable) || (phase!=Phase::receiving && phase!=Phase::retired)) return false;
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
    observe(now);return phase==Phase::awaitingRestart && !inFlight && !resourceOpen && !filesystemUnavailable;
  }
  bool handoffRestart(const GitUpdateOwnerToken &expected,uint64_t now) {
    if(!restartReady(expected,now)) return false;
    token={};applicationSource={};phase=Phase::idle;inFlight=everOpened=expired=filesystemUnavailable=false;lastProgress=highWater=0;return true;
  }
  bool cancelRestart(const GitUpdateOwnerToken &expected,uint64_t now) {
    return handoffRestart(expected,now); // Same exact ready-source predicates, without acquiring a restart owner.
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
    inFlight=true;return {token,kind,resourceOpen,filesystemUnavailable};
  }
  bool close(const GitUpdateOwnerToken &expected) {
    if(!matches(expected) || !inFlight || resourceOpen || filesystemUnavailable) return false;
    token={};applicationSource={};phase=Phase::idle;inFlight=everOpened=expired=filesystemUnavailable=false;lastProgress=highWater=0;return true;
  }
  bool active(const GitUpdateOwnerToken &expected) const { return matches(expected); }
};
static_assert(sizeof(ManualUpdateLifecycle)<=80,"Keep manual transfer lifecycle bounded");
