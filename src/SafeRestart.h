#pragma once
#include "RestartTypes.h"
#include "ModelCommitResult.h"
#include "GitUpdateOwner.h"
// Loop-owned immediate admission: checks settings/queue and commits the current
// dirty model without replaying edits, counters, RF or GPIO commands.
RestartResult requestRestart(uint32_t delayMs=500);
// Any task: cached-only bounded mailbox, never a claim of admission. Its
// maximum 60 s age includes the cached observation age; loop revalidates it.
RestartSubmission submitRestart(uint32_t delayMs=500);
// Exact completed transfer source; never falls back to a generic restart.
RestartResult requestOwnedRestart(const GitUpdateOwnerToken &,uint32_t delayMs=500);
RestartSubmission submitOwnedRestart(const GitUpdateOwnerToken &,uint32_t delayMs=500);
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
