#pragma once
#include <stdint.h>
enum class RestartReason : uint8_t {
  none, settingsUncertain, modelFailed, modelUncertain, queuedOperations,
  pending, stale, cancelled, exhausted, clock, invalidRequest, notReady, persistenceBusy, counterUncertain, activeMovement, unknownGroup, securityUnavailable
};
struct RestartResult {
  bool admitted=false;
  RestartReason reason=RestartReason::none;
};
enum class RestartSubmission : uint8_t { queued, busy, refused };
enum class RestartPhase : uint8_t { idle, queued, admitted, refused, cancelled, restarting };
struct RestartSnapshot {
  RestartPhase phase=RestartPhase::idle;
  RestartReason reason=RestartReason::none;
  uint64_t settingsEpoch=0,modelEpoch=0,generation=0;
  uint64_t modelConfigurationRevision=0,operationEpoch=0;
  bool pending() const { return phase==RestartPhase::queued || phase==RestartPhase::admitted || phase==RestartPhase::restarting; }
};
const char *restartReason(RestartReason reason);
