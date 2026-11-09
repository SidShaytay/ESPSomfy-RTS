#pragma once
#include <stdint.h>
#include "RestartTypes.h"
enum class ModelCommitState : uint8_t { saved, failed, uncertain };
enum class ModelCommitStage : uint8_t {
  none,
  path,
  open,
  write,
  serialize,
  flush,
  sync,
  close,
  verifyOpen,
  verifyRead,
  verifyBytes,
  verifyClose,
  replace
};
struct ModelCommitResult {
  ModelCommitState state = ModelCommitState::failed;
  ModelCommitStage stage = ModelCommitStage::none;
  bool saved() const { return state == ModelCommitState::saved; }
};

// Completion metadata is separate from the file-only persistence result.
struct ModelCommitReport {
  ModelCommitResult file;
  bool rebooting = false;
  bool restartDeferred = false;
  RestartReason refusal = RestartReason::none;
};
// Safe static descriptions; no record names, paths or counter values.
const char *modelCommitFailureDescription(ModelCommitState state);
