#pragma once
#include "RestartTypes.h"
#include "ModelCommitResult.h"
// Loop-owned immediate admission: checks settings/queue and commits the current
// dirty model without replaying edits, counters, RF or GPIO commands.
RestartResult requestRestart(uint32_t delayMs=500);
// Any task: one bounded mailbox, never a claim that restart was admitted.
RestartSubmission submitRestart(uint32_t delayMs=500);
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
