#pragma once
#include <stdint.h>
namespace LocalUi {
enum class DispatchResult : uint8_t;
// Loop-owned only. A marker retains no model pointers or executable request;
// the adapter owns one bounded immutable admission context until completion.
constexpr uint64_t controlQueueMaxAgeMs=5000;
void refreshLocalControl();
bool localControlTokenActive(uint64_t token);
void runLocalControl(uint64_t token);
bool localControlPending();
bool cancelUnsubmittedLocalControl();
bool takeLocalControlResult(DispatchResult &);
}
