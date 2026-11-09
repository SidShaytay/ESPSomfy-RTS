#pragma once
#include <stdint.h>
#include "ModelCommitResult.h"
// Explicit operator counter corrections only. Automatic TX/RX advancement
// retains its existing algorithm. An uncertain write must never be rolled back
// or replayed from a cached value.
enum class CounterEditState : uint8_t { saved, failed, uncertain, stale };
struct CounterEditResult {
  CounterEditState state = CounterEditState::failed;
  bool saved() const { return state == CounterEditState::saved; }
};
CounterEditResult persistExplicitCounter(uint32_t address,uint16_t code);
// Received/linked codes may advance but never lower an existing identity.
CounterEditResult persistObservedCounter(uint32_t address,uint16_t code,uint16_t aliasHighwater);
enum class CounterQuarantine : uint8_t { none, nvsUnknown, filePending };
enum class CounterEditAdmission : uint8_t { admitted, invalid, busy, blocked };
// Every access is serialized by the owning TX queue lock in production. One
// reservation and at most one quarantined identity: uncertainty blocks further
// explicit edits, while commands for unrelated identities remain available.
class CounterEditSafety {
  uint32_t editingAddress=0,uncertainAddress=0;
  CounterQuarantine cause=CounterQuarantine::none;
public:
  CounterEditAdmission reserve(uint32_t address,bool idle,bool pendingTx,bool restart) {
    if(uncertainAddress) return CounterEditAdmission::blocked;
    if(!address || address>0xffffff) return CounterEditAdmission::invalid;
    if(editingAddress || !idle || pendingTx || restart) return CounterEditAdmission::busy;
    editingAddress=address;
    return CounterEditAdmission::admitted;
  }
  void finish(CounterEditResult result,ModelCommitResult model) {
    if(!editingAddress) return;
    if(result.state==CounterEditState::uncertain || (result.saved() && !model.saved())) {
      uncertainAddress=editingAddress;
      cause=result.saved() ? CounterQuarantine::filePending : CounterQuarantine::nvsUnknown;
    }
    editingAddress=0;
  }
  // Recovery is only serialization of the CURRENT RAM model after verified
  // NVS. It cannot retry/replay a counter write or clear unknown NVS state.
  CounterEditAdmission reserveFileRecovery(bool restart) {
    if(cause==CounterQuarantine::nvsUnknown) return CounterEditAdmission::blocked;
    if(cause!=CounterQuarantine::filePending) return CounterEditAdmission::invalid;
    if(editingAddress || restart) return CounterEditAdmission::busy;
    editingAddress=uncertainAddress;return CounterEditAdmission::admitted;
  }
  void finishFileRecovery(ModelCommitResult model) {
    if(!editingAddress || editingAddress!=uncertainAddress || cause!=CounterQuarantine::filePending) return;
    if(model.saved()) {uncertainAddress=0;cause=CounterQuarantine::none;}
    editingAddress=0;
  }
  uint32_t fileRecoveryAddress() const { return cause==CounterQuarantine::filePending ? uncertainAddress : 0; }
  CounterQuarantine quarantine() const { return cause; }
  bool blocks(uint32_t address) const {
    return address && (address==editingAddress || address==uncertainAddress);
  }
  uint32_t blockedAddress() const { return editingAddress ? editingAddress : uncertainAddress; }
  bool uncertain() const { return uncertainAddress!=0; }
  bool active() const { return editingAddress || uncertainAddress; }
};
static_assert(sizeof(CounterEditSafety)<=12,"Keep counter edit policy bounded");
