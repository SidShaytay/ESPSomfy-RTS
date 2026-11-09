#pragma once
#include <stdint.h>
#include "ModelCommitResult.h"
#include "PrimaryCounterReview.h"
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
// Loop-owned read-only review. Failure clears out. No physical command, NVS
// write or model/history mutation. Stale means the exact reviewed state changed.
CounterDomainState reviewPrimaryCounter(uint8_t shadeId,PrimaryCounterReview &out);
// Read-only live aliases/quarantine check, no NVS or ownership mutation.
bool primaryCounterReviewCurrent(uint8_t shadeId,const PrimaryCounterReview &expected);
// Final grant-consumption witness. Read-only: no allocation, NVS, authority
// mutation or callbacks which can destroy the borrowed review/session.
class CounterCorrectionWitness {
public:
  virtual ~CounterCorrectionWitness() = default;
  virtual bool current() = 0;
};
CounterDomainResult correctReviewedPrimaryCounter(uint8_t shadeId,uint16_t code,
                                                  const PrimaryCounterReview &expected,CounterCorrectionWitness &witness);
// Linking is received/observed learning: never lowers retained NVS/live aliases.
CounterDomainResult linkExplicitRemote(uint8_t shadeId,uint32_t address,uint16_t code);
// Only current known-NVS/file-pending state is recoverable. Never writes NVS.
CounterDomainResult recoverExplicitCounterFile();
