#include "LoopQueueGate.h"
#include <cassert>
#include <atomic>
#include <thread>
int main() {
  LoopQueueGate gate;
  unsigned queued = 0;
  assert(gate.enter()); // producer hasn't completed its enqueue yet
  assert(!gate.freezeIfEmpty([&] { return queued == 0; }));
  ++queued;
  gate.leave();
  assert(!gate.freezeIfEmpty([&] { return queued == 0; }));
  --queued;
  assert(gate.freezeIfEmpty([&] { return queued == 0; }));
  assert(!gate.enter());
  gate.resume();
  assert(gate.enter());
  gate.leave();
  // A producer racing the snapshot either makes freezing fail or is refused.
  for(unsigned i = 0; i < 2000; ++i) {
    LoopQueueGate racing;
    std::atomic<unsigned> count{0};
    std::atomic<bool> go{false};
    bool accepted = false, frozen = false;
    std::thread producer([&] {
      while(!go.load()) {}
      accepted = racing.enter();
      if(accepted) { ++count; racing.leave(); }
    });
    std::thread snapshot([&] {
      while(!go.load()) {}
      frozen = racing.freezeIfEmpty([&] { return count.load() == 0; });
    });
    go = true;
    producer.join(); snapshot.join();
    assert(!(accepted && frozen));
    if(frozen) assert(count == 0 && !racing.enter());
  }
}
