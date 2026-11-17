#pragma once
#include "GitUpdate.h"
// Loop-only controller adapter. Startup must initialize the global TLS
// allocator first. No automatic check, install or controller command is issued.
void beginGitUpdateController();
void loopGitUpdateController();
bool gitUpdateControllerPaused();
GitUpdateRequest requestLocalGitUpdateCheck(uint32_t session,uint32_t security,uint64_t &operation);
GitUpdateRequest requestLocalGitUpdateInstall(const GitUpdateCandidate &,uint32_t session,uint32_t security,uint64_t &operation);
bool cancelLocalGitUpdate(uint32_t session,uint32_t security,uint64_t operation);
// Browser (protocol 2 socket) requests. The socket was already authenticated;
// the operation is bound to the current security configuration revision, and a
// confirmed install keeps the same 30-minute window the LCD gets.
GitUpdateRequest requestBrowserGitUpdateCheck(uint64_t &operation);
GitUpdateRequest requestBrowserGitUpdateInstall(const GitUpdateCandidate &,uint64_t &operation);
bool cancelBrowserGitUpdate(uint64_t operation);
// Whether the current operation was started from a browser.
bool gitUpdateBrowserOwned(uint64_t operation);
