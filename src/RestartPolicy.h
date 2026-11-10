#pragma once
#include "RestartTypes.h"
struct RestartObservation {
  uint64_t settingsEpoch=1,modelEpoch=1;
  RestartReason refusal=RestartReason::none;
  bool operationsPending=false;
  uint64_t modelConfigurationRevision=1,operationEpoch=1;
};
// Pure policy. Runtime owns synchronization and supplies a monotonic boot clock.
class RestartPolicy {
  RestartSnapshot value;
  uint64_t highWater=0,deadline=0,generation=0;
  uint32_t delay=0;
  bool clock(uint64_t now);
  RestartReason refusal(const RestartObservation &observation) const;
public:
  RestartSnapshot snapshot() const { return value; }
  RestartSubmission submit(uint32_t delayMs,const RestartObservation &published);
  bool admitQueued(uint64_t now,const RestartObservation &observation,uint64_t expectedGeneration);
  bool observe(uint64_t now,const RestartObservation &observation);
  void cancel(RestartReason reason=RestartReason::cancelled);
  bool due(uint64_t now,const RestartObservation &observation);
  bool take(uint64_t now,const RestartObservation &observation);
};
