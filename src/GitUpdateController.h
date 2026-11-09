#pragma once
#include "GitUpdate.h"
// Loop-only controller adapter. Startup must initialize the global TLS
// allocator first. No automatic check, install or controller command is issued.
void beginGitUpdateController();
void loopGitUpdateController();
bool gitUpdateControllerPaused();
GitUpdateRequest requestLocalGitUpdateCheck(uint32_t session,uint32_t security);
GitUpdateRequest requestLocalGitUpdateInstall(const GitUpdateCandidate &,uint32_t session,uint32_t security,uint64_t &operation);
bool cancelLocalGitUpdate(uint32_t session,uint32_t security);
