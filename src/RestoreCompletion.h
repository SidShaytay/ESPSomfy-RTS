#pragma once
#include "RestoreResult.h"
// loop() only. Facts from one completed adoption attempt; no retry, owner or
// restart authority. Notifications cannot control resource cleanup.
void completeRestoreModel(const RestoreApplyResult &);
