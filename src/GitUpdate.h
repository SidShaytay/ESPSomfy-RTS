#pragma once
#if defined(ESPSOMFY_AUDIT_CONTROL) && !defined(ESPSOMFY_STACK_AUDIT)
#error "Audit control requires stack audit instrumentation"
#endif
#include <ArduinoJson.h>
#include "GitUpdateOwner.h"
#include "RadioUpdateActivity.h"
#include <cstddef>
#include <cstdint>

// One shared updater service. Installation callbacks are an unfinished draft:
// startup/UI wiring and postboot acceptance remain unfinished.
enum class GitUpdatePhase : uint8_t { unavailable, idle, queued, checking, available, current, preparing, prepared, changed, cancelled, error, downloading, verifying, writing, written, cleanupPending, retryWaiting };
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
enum class GitUpdateDownload : uint8_t { none,api,manifest,image };
enum class GitUpdateTransportStage : uint8_t { none,init,setHeaders,open,headers,status,read,complete };
// Numeric-only diagnostic of the current operation; never contains network/model identifiers.
struct GitUpdateTransport {
  GitUpdateDownload download=GitUpdateDownload::none;
  GitUpdateTransportStage stage=GitUpdateTransportStage::none;
  uint8_t certificateState=0,certificateFailure=0; // attached/leaf/bundle bits; date-failure enum
  int32_t sdkError=0,httpStatus=0,socketError=0,tlsSdkError=0,tlsError=0,tlsFlags=0;
};
static_assert(sizeof(GitUpdateTransport)<=32,"Bound updater transport diagnostics");
constexpr uint32_t gitUpdateInstallDurationMs=30u*60u*1000u;
struct GitUpdateSnapshot {
  GitUpdatePhase phase=GitUpdatePhase::unavailable;
  GitUpdateReason reason=GitUpdateReason::notStarted;
  bool supported=false, busy=false, compatible=false, cancellable=false, canPrepare=false,manual=false;
  // Metadata-only prepared state never claims installation is available.
  bool canInstall=false;
  bool installContinuation=false;
  uint16_t retryCount=0;
  uint32_t retryDelayMs=0,installRemainingMs=0;
  uint8_t progress=0;
  uint64_t operation=0;
  char installed[48]={};
  GitUpdateCandidate candidate;
  GitUpdateTransport transport;
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
struct ManualApplicationSource;
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
#ifdef ESPSOMFY_AUDIT_CONTROL
  // Download/verify/free only. Includes compatible equal/older releases for
  // measurement without relaxing any installation eligibility or admission.
  GitUpdateRequest requestAuditStage(const GitUpdateCandidate &expected);
#endif
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
  bool requestCancel(uint64_t expectedOperation,GitUpdateReason reason=GitUpdateReason::cancelled);
  bool acquireExternal(GitUpdateExternalOwner kind,GitUpdateOwnerToken &token);
  bool releaseExternal(const GitUpdateOwnerToken &token);
  bool ownsExternal(const GitUpdateOwnerToken &token) const;
  // Short lifecycle claims beneath the same external owner. Callers perform
  // SDK/file I/O after these methods release the service lock.
  bool beginTransfer(const GitUpdateOwnerToken &,ManualUpdateKind,uint64_t now);
  bool prepareApplication(const GitUpdateOwnerToken &,const ManualApplicationSource &,uint64_t now);
  bool claimApplicationPreparation(const GitUpdateOwnerToken &,const ManualApplicationSource &,uint64_t now);
  bool applicationPrepared(const GitUpdateOwnerToken &);
  bool claimPreparedApplication(const GitUpdateOwnerToken &,const ManualApplicationSource &,uint64_t now);
  bool cancelApplicationPreparation(const GitUpdateOwnerToken &,const ManualApplicationSource &);
  bool claimTransfer(const GitUpdateOwnerToken &,uint64_t now,bool queuedRestore=false);
  bool transferContinuing(const GitUpdateOwnerToken &) const;
  bool transferContinuing(const GitUpdateOwnerToken &,uint64_t now);
  bool transferResource(const GitUpdateOwnerToken &,bool opened);
  bool transferFilesystem(const GitUpdateOwnerToken &,bool unavailable);
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
  // Reducing loop cleanup must never acquire a replacement transfer.
  ManualUpdateCleanup claimTransferCleanup(const GitUpdateOwnerToken &,uint64_t now);
  bool closeTransfer(const GitUpdateOwnerToken &);
  bool busy() const;
  GitUpdateSnapshot snapshot() const;
  void status(JsonObject destination) const;
};
extern GitUpdateClass gitUpdate;
