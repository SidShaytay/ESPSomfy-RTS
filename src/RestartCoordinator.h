#pragma once
#include "RestartPolicy.h"
#include "StorageLease.h"
#include "ModelCommitResult.h"
struct RestartWork {
  uint64_t intent=0;
  StorageLease storage;
  RestartReason refusal=RestartReason::none;
  bool ready() const { return intent && storage.generation && refusal==RestartReason::none; }
};
// Pure two-phase coordinator. Protect every method with ONE shared runtime
// mutex; release that mutex for all model/SDK I/O while retaining Work.storage.
// Every actual model commit records its outcome before work is completed.
class RestartCoordinator {
  StorageLeaseGate &storage;
  RestartPolicy policy;
  RestartObservation published{1,1,RestartReason::notReady,false};
  uint64_t modelEpoch=1,inFlight=0;
  bool modelRecovery=false,modelExhausted=false;
  RestartObservation current(RestartObservation observation) const;
  RestartWork reserve();
  bool owns(const RestartWork &work) const;
public:
  explicit RestartCoordinator(StorageLeaseGate &gate):storage(gate) {}
  RestartCoordinator(const RestartCoordinator &)=delete;
  RestartCoordinator &operator=(const RestartCoordinator &)=delete;
  RestartSnapshot snapshot() const { return policy.snapshot(); }
  bool requiresModelRecovery() const { return modelRecovery; }
  bool observe(uint64_t now,RestartObservation observation);
  RestartSubmission submit(uint32_t delayMs);
  RestartWork begin(uint32_t delayMs,uint64_t now,RestartObservation observation);
  RestartWork prepareQueued(uint64_t now,RestartObservation observation);
  RestartResult completeAdmission(const RestartWork &work,uint64_t now,RestartObservation observation,RestartReason ioFailure=RestartReason::none);
  RestartWork prepareFinal(uint64_t now,RestartObservation observation);
  bool completeFinal(const RestartWork &work,uint64_t now,RestartObservation observation,RestartReason ioFailure=RestartReason::none);
  bool release(const RestartWork &work);
  void cancel(RestartReason reason=RestartReason::cancelled) { policy.cancel(reason); }
  void modelCommitted(ModelCommitResult result);
  // Loop only after fresh manual admission and existing GitUpdate owner gate.
  // No routing or Update.begin is enabled by this resource capability.
  bool acquireUpdate(RestartObservation observation,StorageLease &token);
};
