#pragma once
#include "RestartTypes.h"
#include "ModelCommitResult.h"
#include "GitUpdateOwner.h"
#include "RadioUpdateActivity.h"
// Loop-owned immediate admission: checks settings/queue and commits the current
// dirty model without replaying edits, counters, RF or GPIO commands.
RestartResult requestRestart(uint32_t delayMs=500);
// Loop only. Revalidate a caller's source outside locks, including after the
// final save. sealed=true means this intent holds the final producer/TX seals.
RestartResult requestCheckedRestart(bool (*sourceCurrent)(bool sealed),uint32_t delayMs=3000);
// Any task: cached-only bounded mailbox, never a claim of admission. Its
// maximum 60 s age includes the cached observation age; loop revalidates it.
RestartSubmission submitRestart(uint32_t delayMs=500);
// Exact completed transfer source; never falls back to a generic restart.
RestartResult requestOwnedRestart(const GitUpdateOwnerToken &,uint32_t delayMs=500);
RestartSubmission submitOwnedRestart(const GitUpdateOwnerToken &,uint32_t delayMs=500);
// Loop-only installer handoff. The caller retains its exact queue/radio pause
// and completed transfer until the final noncancellable decision. activate()
// runs only then, outside locks. A false result retains both seals and owner;
// failed(..., false) must report uncertainty without cleanup or another attempt.
struct InstallRestartSource {
  RadioInstallToken radio;
  uint64_t operation=0;
  bool (*current)(uint64_t)=nullptr;
  bool (*activate)(uint64_t)=nullptr;
  void (*failed)(uint64_t,RestartReason,bool mayClean)=nullptr;
};
RestartResult requestInstallRestart(const GitUpdateOwnerToken &,const InstallRestartSource &,uint32_t delayMs=500);
// Accepted mutating/RF queue work, including work drained before the next poll.
void noteRestartOperationAccepted();
// Conservative cached manual-transfer precheck, not complete image admission.
bool restartTransferBlocked();
// Restore receive only; no live-model mutation or restart permission. A full
// validated model restore may repair unavailable boot state in the loop.
bool restartRestoreStagingBlocked();
// Cheap synchronized cached reads; pending includes mailbox and admitted timer.
bool restartPending();
RestartSnapshot restartStatus();
// Loop only. Advances admission/final checks before any shutdown. True only
// when shutdown/restart was invoked; no rejected intent is retried implicitly.
bool restartLoop();
void cancelRestart();
// Every controller commit, including automatic retry, must record its outcome.
// Failure permanently invalidates older restart authority, even after recovery.
void noteModelCommit(ModelCommitResult result);
