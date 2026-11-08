#pragma once
#include <stdint.h>
// Explicit operator counter corrections only. Automatic TX/RX advancement
// retains its existing algorithm. An uncertain write must never be rolled back
// or replayed from a cached value.
enum class CounterEditState : uint8_t { saved, failed, uncertain };
struct CounterEditResult {
  CounterEditState state = CounterEditState::failed;
  bool saved() const { return state == CounterEditState::saved; }
};
CounterEditResult persistExplicitCounter(uint32_t address,uint16_t code);
enum class CounterEditAdmission : uint8_t { admitted, invalid, busy, blocked };
// Every access is serialized by the owning TX queue lock in production. One
// reservation and at most one quarantined identity: uncertainty blocks further
// explicit edits, while commands for unrelated identities remain available.
class CounterEditSafety {
  uint32_t editingAddress=0,uncertainAddress=0;
public:
  CounterEditAdmission reserve(uint32_t address,bool idle,bool pendingTx,bool restart) {
    if(uncertainAddress) return CounterEditAdmission::blocked;
    if(!address || address>0xffffff) return CounterEditAdmission::invalid;
    if(editingAddress || !idle || pendingTx || restart) return CounterEditAdmission::busy;
    editingAddress=address;
    return CounterEditAdmission::admitted;
  }
  void finish(CounterEditResult result) {
    if(!editingAddress) return;
    if(result.state==CounterEditState::uncertain) uncertainAddress=editingAddress;
    editingAddress=0;
  }
  bool blocks(uint32_t address) const {
    return address && (address==editingAddress || address==uncertainAddress);
  }
  bool uncertain() const { return uncertainAddress!=0; }
  bool active() const { return editingAddress || uncertainAddress; }
};
static_assert(sizeof(CounterEditSafety)<=8,"Keep counter edit policy bounded");
