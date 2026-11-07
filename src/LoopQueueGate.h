#pragma once
#include <atomic>
#include <cstdint>

// Freeze queue admission without blocking an async producer. A producer enters
// before touching the queue and leaves only after its enqueue attempt finishes.
// The loop freezes first, then checks in-flight producers and queue emptiness;
// a racing producer therefore either makes the freeze fail or is refused.
class LoopQueueGate {
  std::atomic<bool> frozen{false};
  std::atomic<uint32_t> producers{0};
public:
  bool enter() {
    ++producers;
    if(!frozen.load()) return true;
    --producers;
    return false;
  }
  void leave() { --producers; }
  template<class Empty> bool freezeIfEmpty(Empty empty) {
    bool expected = false;
    if(!frozen.compare_exchange_strong(expected, true)) return false;
    if(producers.load() == 0 && empty()) return true;
    frozen.store(false);
    return false;
  }
  void resume() { frozen.store(false); }
};
