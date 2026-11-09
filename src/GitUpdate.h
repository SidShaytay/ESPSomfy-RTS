#pragma once
#include <ArduinoJson.h>
#include "GitUpdateOwner.h"
#include <cstddef>
#include <cstdint>

// One shared updater service. This stage only checks/revalidates metadata;
// no installer, startup invocation, route or automatic network request exists.
enum class GitUpdatePhase : uint8_t { unavailable, idle, queued, checking, available, current, preparing, prepared, changed, cancelled, error };
enum class GitUpdateReason : uint8_t {
  none, unsupported, notStarted, development, memory, admission, clock, hardware,
  layout, bootloader, assets, transport, headers, redirect, notPublished, body,
  metadata, repository, manifest, digest, compatibility, noNewerRelease, cancelled,
  timeout, candidateChanged, invalidCandidate, settingsUncertain, certificateTime, freshness
};
enum class GitUpdateRequest : uint8_t { accepted, busy, unavailable, invalidCandidate };
struct GitUpdateCandidate {
  uint64_t generation=0;
  uint32_t size=0;
  char sourceSha[41]={}, version[48]={}, imageSha[65]={}, manifestSha[65]={};
};
struct GitUpdateSnapshot {
  GitUpdatePhase phase=GitUpdatePhase::unavailable;
  GitUpdateReason reason=GitUpdateReason::notStarted;
  bool supported=false, busy=false, compatible=false, cancellable=false, canPrepare=false;
  // Metadata-only prepared state never claims installation is available.
  bool canInstall=false;
  uint8_t progress=0;
  char installed[48]={};
  GitUpdateCandidate candidate;
  size_t minInternalFree=0,minLargestInternal=0,maxPsramUsed=0,workerStackMinimum=0;
  size_t tlsRequested=0,tlsPeak=0,tlsInternalRequested=0,tlsFailures=0;
};
struct GitUpdateReadAdmission {
  bool allowed=false;
};
const char *gitUpdateReason(GitUpdateReason reason);
const char *gitUpdatePhase(GitUpdatePhase phase);
enum class ManualUpdateKind : uint8_t;
struct ManualUpdateCleanup;
class GitUpdateClass {
public:
  // loop() alone invokes admission. The worker never owns model/settings data.
  // Global TLS allocation must already be initialized before ANY mbedTLS allocation, including token HMAC.
  // begin() and loop() are loop-owned; request/snapshot/owner methods are thread-safe.
  void begin(GitUpdateReadAdmission (*admit)()=nullptr);
  void loop();
  GitUpdateRequest requestCheck();
  GitUpdateRequest requestPrepare(const GitUpdateCandidate &expected);
  bool requestCancel();
  bool acquireExternal(GitUpdateExternalOwner kind,GitUpdateOwnerToken &token);
  bool releaseExternal(const GitUpdateOwnerToken &token);
  bool ownsExternal(const GitUpdateOwnerToken &token) const;
  // Short lifecycle claims beneath the same external owner. Callers perform
  // SDK/file I/O after these methods release the service lock.
  bool beginTransfer(const GitUpdateOwnerToken &,ManualUpdateKind,uint64_t now);
  bool claimTransfer(const GitUpdateOwnerToken &,uint64_t now,bool queuedRestore=false);
  bool transferContinuing(const GitUpdateOwnerToken &) const;
  bool transferContinuing(const GitUpdateOwnerToken &,uint64_t now);
  bool transferResource(const GitUpdateOwnerToken &,bool opened);
  bool queueTransfer(const GitUpdateOwnerToken &);
  bool holdTransferRestart(const GitUpdateOwnerToken &);
  bool holdTransferRecovery(const GitUpdateOwnerToken &,uint64_t now);
  bool claimTransferRecovery(const GitUpdateOwnerToken &,uint64_t now);
  bool transferRestoreClaimed(const GitUpdateOwnerToken &,uint64_t now);
  bool transferRestartHeld(const GitUpdateOwnerToken &,uint64_t now);
  // Atomic handoff to the same non-expiring owner, held through actual shutdown.
  // A nonzero expected source never falls back to a generic owner-none restart.
  bool acquireRestart(GitUpdateOwnerToken &result,uint64_t now,const GitUpdateOwnerToken &expected = {});
  // Cancel only an exact completed source before restart handoff; no I/O claim/resource may remain.
  bool releaseRestartSource(const GitUpdateOwnerToken &,uint64_t now);
  bool finishTransfer(const GitUpdateOwnerToken &,uint64_t now,bool progress);
  bool retireTransfer(const GitUpdateOwnerToken &);
  ManualUpdateCleanup claimTransferCleanup(uint64_t now);
  bool closeTransfer(const GitUpdateOwnerToken &);
  bool busy() const;
  GitUpdateSnapshot snapshot() const;
  void status(JsonObject destination) const;
};
extern GitUpdateClass gitUpdate;
