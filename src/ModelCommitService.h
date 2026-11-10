#pragma once
#include "ModelCommitResult.h"
#include "ProtoFrame.h"
#include "GitUpdate.h"
struct loop_item_t;
enum class ModelCommitMode : uint8_t {
  save,
  applicationRecovery,
  applicationRecoveryAndRestart,
  // Deliberate browser COMMIT only; generic metadata/file retries stay save.
  counterFileRecovery,
  // Initial filesystem preparation stays at the FIFO tail, never recovery priority.
  applicationPreparation
};
// Any task: queue only a fresh serialization of the current model. No edit,
// RF or counter operation is retained for retry. False means not queued.
bool submitModelCommit(const proto_reply_to_t &to = {},
                       ModelCommitMode mode = ModelCommitMode::save,
                       const GitUpdateOwnerToken &owner = {});
// Loop only: report the observed save outcome, then restart only if this
// specific request explicitly asked for it and the save was verified.
void runModelCommit(const loop_item_t &item);

// Loop only: perform one admitted commit and its standard notifications. A
// direct local caller must first check pending()==0 to preserve accepted FIFO.
ModelCommitReport
applyModelCommit(const proto_reply_to_t &to = {},
                 ModelCommitMode mode = ModelCommitMode::save);
