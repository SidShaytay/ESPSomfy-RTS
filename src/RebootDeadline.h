#pragma once
#include <cstdint>
// Reboot delays are bounded to less than half the millis() range.
inline bool rebootDeadlineReached(uint32_t now, uint32_t deadline) {
  return static_cast<int32_t>(now - deadline) >= 0;
}

// Preserve an earlier scheduled reboot, including one already due. Values are
// relative to the same millis() era and delays are bounded below half its range.
inline uint32_t earlierRebootDeadline(uint32_t now, uint32_t delay, bool active, uint32_t existing) {
  const uint32_t requested = now + delay;
  return active && static_cast<int32_t>(existing - requested) <= 0 ? existing : requested;
}
