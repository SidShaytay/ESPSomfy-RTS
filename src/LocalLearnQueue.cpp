#include "LocalLearnQueue.h"
namespace LocalUi {
LearnQueued LearnQueue::reserve(LearnPending &source, uint64_t now) {
  if (pending)
    return {LearnQueueReason::busy};
  if (generation == UINT64_MAX)
    return {LearnQueueReason::exhausted};
  const auto reason = source.current(now);
  if (reason != LearnQueueReason::ready)
    return {reason};
  ++generation;
  pending = &source;
  submittedAt = observedAt = now;
  return {LearnQueueReason::ready, generation};
}
bool LearnQueue::detach(uint64_t token, LearnPending &source) {
  if (!active(token) || pending != &source)
    return false;
  pending = nullptr;
  return true;
}
LearnQueueReason LearnQueue::check(uint64_t now) {
  if (now < observedAt || now < submittedAt)
    return LearnQueueReason::clock;
  if (now - submittedAt >= maxAgeMs)
    return LearnQueueReason::expired;
  observedAt = now;
  return pending->current(now);
}
void LearnQueue::refresh(uint64_t now) {
  if (!pending)
    return;
  const auto reason = check(now);
  if (reason == LearnQueueReason::ready)
    return;
  auto *source = pending;
  pending = nullptr;
  source->cancelled(reason); // Source may destroy itself/register another sink.
}
void LearnQueue::run(uint64_t token, uint64_t now) {
  if (!active(token))
    return;
  const auto reason = check(now);
  auto *source = pending;
  pending = nullptr;
  if (reason == LearnQueueReason::ready)
    source->execute(now);
  else
    source->cancelled(reason);
  // Never inspect source or registry state after a callback.
}
} // namespace LocalUi
