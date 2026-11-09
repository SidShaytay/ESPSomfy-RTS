#pragma once
#include <ArduinoJson.h>
#include "GitUpdateOwner.h"
#include "RadioUpdateActivity.h"
#include <cstddef>
#include <cstdint>

// One shared updater service. Installation callbacks are an unfinished draft:
// startup/UI wiring and postboot acceptance remain unfinished.
enum class GitUpdatePhase : uint8_t { unavailable, idle, queued, checking, available, current, preparing, prepared, changed, cancelled, error, downloading, verifying, writing, written, cleanupPending };
enum class GitUpdateReason : uint8_t {
  none, unsupported, notStarted, development, memory, admission, clock, hardware,
  layout, bootloader, assets, transport, headers, redirect, notPublished, body,
  metadata, repository, manifest, digest, compatibility, noNewerRelease, cancelled,
  timeout, candidateChanged, invalidCandidate, settingsUncertain, certificateTime, freshness, invalidImage, writeFailed, activationFailed
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
  bool supported=false, busy=false, compatible=false, cancellable=false, canPrepare=false,manual=false;
  // Metadata-only prepared state never claims installation is available.
  bool canInstall=false;
  uint8_t progress=0;
  uint64_t operation=0;
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
// Callbacks run on loop(), never on the transport/flash worker or under its
// lock. The existing manual owner/lifecycle carries the installation too.
struct GitUpdateInstallPreparation {bool ready=false;RadioInstallToken radio;};
struct GitUpdateInstallHooks {
  bool (*current)(uint64_t operation)=nullptr;
  GitUpdateInstallPreparation (*prepare)(const GitUpdateOwnerToken &)=nullptr;
  bool (*cleanup)(const GitUpdateOwnerToken &,const RadioInstallToken &)=nullptr;
  void (*written)(const GitUpdateOwnerToken &,const RadioInstallToken &)=nullptr;
  bool (*manual)(const GitUpdateOwnerToken &,uint64_t operation,uint64_t securityRevision)=nullptr;
};
class GitUpdateClass {
public:
  // loop() alone invokes admission. The worker never owns model/settings data.
  // Global TLS allocation must already be initialized before ANY mbedTLS allocation, including token HMAC.
  // begin() and loop() are loop-owned; request/snapshot/owner methods are thread-safe.
  void begin(GitUpdateReadAdmission (*admit)()=nullptr,GitUpdateInstallHooks hooks={});
  void loop();
  GitUpdateRequest requestCheck();
  GitUpdateRequest requestCheck(uint64_t &operation);
  GitUpdateRequest requestPrepare(const GitUpdateCandidate &expected);
  GitUpdateRequest requestInstall(const GitUpdateCandidate &,uint64_t &operation);
  // Receive completion holds the queued firmware claim. Reserve a public
  // operation before FIFO publication; loop adoption retains this same ticket.
  bool reserveManualInstall(const GitUpdateOwnerToken &,uint64_t &operation);
  // Loop only. Caller holds the queued firmware claim/resource. On acceptance
  // the service owns this heap_caps PSRAM allocation and nulls image; refusal
  // leaves image and transfer ownership with the caller. No GitHub provenance.
  GitUpdateRequest requestManualInstall(const GitUpdateOwnerToken &,uint8_t *&image,size_t size,uint64_t securityRevision,uint64_t &operation);
  // Loop only, after SafeRestart's final take and exact source handoff.
  bool activateInstall(uint64_t operation);
  void finishInstallRestart(uint64_t operation,bool mayClean);
  bool requestCancel();
  bool requestCancel(uint64_t expectedOperation);
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
  bool transferRestartReady(const GitUpdateOwnerToken &,uint64_t now);
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
