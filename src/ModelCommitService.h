#pragma once
#include "ProtoFrame.h"
struct loop_item_t;
enum class ModelCommitMode : uint8_t {
  save,
  applicationRecovery,
  applicationRecoveryAndRestart
};
// Any task: queue only a fresh serialization of the current model. No edit,
// RF or counter operation is retained for retry. False means not queued.
bool submitModelCommit(const proto_reply_to_t &to = {},
                       ModelCommitMode mode = ModelCommitMode::save);
// Loop only: report the observed save outcome, then restart only if this
// specific request explicitly asked for it and the save was verified.
void runModelCommit(const loop_item_t &item);
