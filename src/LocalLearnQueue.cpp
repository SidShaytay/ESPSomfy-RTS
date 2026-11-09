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
#ifdef ESPSOMFY_LOCAL_UI
#include "LoopQueue.h"
#include <esp_timer.h>
extern LoopQueue loopQueue;
namespace LocalUi {
namespace {
LearnQueue registry;
uint64_t queueNow() { return uint64_t(esp_timer_get_time()) / 1000; }
} // namespace
LearnQueued submitLocalLearn(LearnPending &source, uint64_t now) {
  auto result = registry.reserve(source, now);
  if (result.reason != LearnQueueReason::ready)
    return result;
  loop_item_t item;
  item.type = loop_item_type_t::local_advanced;
  item.localToken = result.token;
  item.localRadio = false;
  if (!loopQueue.submit(item)) {
    registry.detach(result.token, source);
    return {LearnQueueReason::queueFull};
  }
  return result;
}
bool cancelLocalLearn(uint64_t token, LearnPending &source) {
  return registry.detach(token, source);
}
void refreshLocalLearn() { registry.refresh(queueNow()); }
bool localLearnTokenActive(uint64_t token) { return registry.active(token); }
void runLocalLearn(uint64_t token) { registry.run(token, queueNow()); }
} // namespace LocalUi
#endif
