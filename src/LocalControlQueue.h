#pragma once
#include <stdint.h>
struct somfy_tx_event_t;
namespace LocalUi {
enum class DispatchResult : uint8_t;
// Loop-owned only. A marker retains no model pointers or executable request;
// the adapter owns one bounded immutable admission context until completion.
constexpr uint64_t controlQueueMaxAgeMs=5000;
void refreshLocalControl();
// Existing model-loop consumer calls this AFTER processing each radio outcome.
void localControlTxEvent(const somfy_tx_event_t &);
bool localControlTokenActive(uint64_t token);
void runLocalControl(uint64_t token);
bool localControlPending();
bool cancelUnsubmittedLocalControl();
bool takeLocalControlResult(DispatchResult &);
}
