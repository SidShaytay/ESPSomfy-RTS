#ifdef ESPSOMFY_LOCAL_UI
#include "LocalLearnQueue.h"
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
  item.localRadio = source.usesRadio();
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
