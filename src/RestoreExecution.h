#pragma once
#include "GitUpdateOwner.h"
#include "RestoreResult.h"
#include "RadioUpdateActivity.h"
#include <stdint.h>
// One loop-owned resource context beneath the existing claimed restore source.
// This is neither another transfer owner nor an installer.
enum class RestoreExecutionPhase : uint8_t { unavailable, idle, queued, running, cleanupPending, applied, partial, refused, cleanupFault };
enum class RestoreExecutionFault : uint8_t { none, unavailable, ownership, busy, cleanupDeferred, radioUnavailable, fileCleanup, staleSource };
struct RestoreExecutionSnapshot {
  uint64_t bootSession=0,generation=0;
  RestoreApplyResult apply;
  RestoreExecutionPhase phase=RestoreExecutionPhase::unavailable;
  RestoreExecutionFault fault=RestoreExecutionFault::unavailable;
  bool restartScheduled=false,restartBlocked=false;
  RestartReason restartReason=RestartReason::none;
};
static_assert(sizeof(RestoreExecutionSnapshot)<=64,"Bounded restore status without backup values");
// Once, from setup before HTTP handlers: bounded RNG attempts, no TLS/NTP.
bool beginRestoreExecution();
bool restoreExecutionReady();
// Called while the existing queued restore source still holds its receive claim,
// before its FIFO marker is published. Cached status only, no file/model I/O.
bool noteRestoreQueued(const GitUpdateOwnerToken &);
// An unpublished/expired marker is terminal for this exact cached generation.
// Never alters a running restore or a replacement source.
void refuseUnpublishedRestore(const GitUpdateOwnerToken &,RestoreRefusal);
// loop() only. The marker has already left the queue; claims the same source.
void runOwnedRestore(const GitUpdateOwnerToken &,uint8_t parts);
// loop() only, before generic manual cleanup. Never retries apply or restart.
void serviceRestoreCleanup();
// Loop-only context match; ConfigFile also verifies the live radio token.
bool restoreExecutionHolds(const GitUpdateOwnerToken &,const RadioInstallToken &);
RestoreExecutionSnapshot restoreExecutionStatus();
const char *restoreExecutionMessage(const RestoreExecutionSnapshot &);
