#pragma once
#include <atomic>
#include "SomfyFrame.h"
#include "TxBuffer.h"
// The loop publishes the fixed hooks before active=true. Other tasks only
// read active/radioAllowed; permission is bound to the service's task and call.
namespace LiftCalibrationGate {
inline std::atomic<bool> active{false};
inline bool (*permitted)() = nullptr;
inline bool modelSaving = false; // loop-owned, never a client-supplied flag
inline void (*txObserver)(const somfy_tx_event_t &) = nullptr;
inline void (*frameObserver)(const somfy_frame_t &) = nullptr;
inline bool radioAllowed() {
  return !active.load(std::memory_order_acquire) || (permitted && permitted());
}
}
