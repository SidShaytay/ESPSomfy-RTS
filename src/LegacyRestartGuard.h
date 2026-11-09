#pragma once
#include "GitUpdate.h"
#include "RestartTypes.h"
// Loop-only legacy admission guards. Owner/source exclusion is not model or
// settings durability certification; the full coordinator remains separate.
RestartResult legacyGenericRestartAdmission();
RestartResult reserveLegacyRestart(GitUpdateOwnerToken &reservation);

// Uses the existing legacy timer; performs no shutdown on admission refusal.
bool legacyRestartLoop();
