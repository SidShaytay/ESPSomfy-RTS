#pragma once
#include <atomic>
#include <cstdint>

// Async callers publish one absolute deadline, never the loop's multi-field
// reboot state. Concurrent requests coalesce to the earliest pending deadline. The latch
// stays set after consumption so an update gate cannot reopen before reboot.
class RebootRequest {
  static constexpr uint32_t NONE = UINT32_MAX;
  std::atomic<uint32_t> pending{NONE};
  std::atomic<bool> latched{false};
public:
  void request(uint32_t now, uint32_t delay) {
    if(delay > INT32_MAX) delay = INT32_MAX;
    uint32_t deadline = now + delay;
    // Reserve a single sentinel; that one deadline fires 1ms earlier.
    if(deadline == NONE) --deadline;
    latched.store(true);
    uint32_t previous = pending.load();
    while((previous == NONE || static_cast<int32_t>(deadline - previous) < 0) &&
      !pending.compare_exchange_weak(previous, deadline)) {}
  }
  bool take(uint32_t &deadline) {
    deadline = pending.exchange(NONE);
    return deadline != NONE;
  }
  bool requested() const { return latched.load(); }
};
