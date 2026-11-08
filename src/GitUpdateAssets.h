#pragma once
#include "GitUpdateAttestation.h"
// Read-only point-in-time check. Future updater ownership must exclude writes.
UpdateAttestation gitUpdateAssetsStatus();
bool gitUpdateAssetsMatch();
