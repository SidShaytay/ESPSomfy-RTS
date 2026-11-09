#pragma once
#include <stdint.h>
#include "ModelCommitResult.h"
// Explicit loop-owned corrections, including a linked remote's initial code.
// No radio operation and no stale counter rollback/replay occurs here.
enum class CounterDomainState : uint8_t {
  saved, invalid, missing, full, busy, quarantined,
  nvsFailed, nvsUncertain, modelFailed, modelUncertain, stale
};
struct CounterDomainResult {
  CounterDomainState state=CounterDomainState::invalid;
  bool applied=false;
  ModelCommitResult file;
  bool saved() const {return state==CounterDomainState::saved;}
};
static_assert(sizeof(CounterDomainResult)<=8,"Keep explicit counter outcomes bounded");
CounterDomainResult correctExplicitCounter(uint8_t shadeId,uint16_t code);
// Linking is received/observed learning: never lowers retained NVS/live aliases.
CounterDomainResult linkExplicitRemote(uint8_t shadeId,uint32_t address,uint16_t code);
// Only current known-NVS/file-pending state is recoverable. Never writes NVS.
CounterDomainResult recoverExplicitCounterFile();
