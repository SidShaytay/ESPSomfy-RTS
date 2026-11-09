#pragma once
#include <stdint.h>
namespace LocalUi {
enum class LearnQueueReason : uint8_t {
  ready,
  busy,
  queueFull,
  exhausted,
  expired,
  clock,
  locked,
  stale,
  candidate
};
struct LearnQueued {
  LearnQueueReason reason = LearnQueueReason::busy;
  uint64_t token = 0;
};
class LearnPending {
public:
  virtual ~LearnPending() = default;
  virtual bool usesRadio() const { return false; }
  // Read-only lifetime predicate: MUST NOT close/free its containing session or
  // invoke callbacks. Candidate validation may erase the cache, not the
  // session.
  virtual LearnQueueReason current(uint64_t now) = 0;
  virtual void execute(uint64_t now) = 0;
  virtual void cancelled(LearnQueueReason) = 0;
};
// One loop-owned borrowed PSRAM intent, never a model pointer or JSON request.
// Owner MUST detach before destroying the sink. All callbacks see a retired
// registry, may destroy the sink, and are the final operation on that pointer.
class LearnQueue {
  friend struct LearnQueueTestAccess;
  LearnPending *pending = nullptr;
  uint64_t generation = 0, submittedAt = 0, observedAt = 0;
  LearnQueueReason check(uint64_t);

public:
  static constexpr uint64_t maxAgeMs = 5000;
  LearnQueued reserve(LearnPending &, uint64_t);
  bool detach(uint64_t, LearnPending &);
  bool active() const { return pending != nullptr; }
  bool active(uint64_t token) const {
    return token && token == generation && pending;
  }
  void refresh(uint64_t);
  void run(uint64_t, uint64_t);
};
static_assert(sizeof(LearnQueue) <= 32, "One bounded loop marker registry");
LearnQueued submitLocalLearn(LearnPending &, uint64_t now);
bool cancelLocalLearn(uint64_t, LearnPending &);
void refreshLocalLearn();
bool localLearnTokenActive(uint64_t);
void runLocalLearn(uint64_t);
} // namespace LocalUi
