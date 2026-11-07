#pragma once
#include <cstdint>
// A successful boot must service the loop with every prerequisite continuously
// healthy. Uptime alone can accept a disconnected or broken installation.
class GitUpdateHealth {
  bool observing = false;
  uint32_t since = 0;
public:
  bool proven(bool healthy, uint32_t now, uint32_t duration) {
    if(!healthy) { observing = false; return false; }
    if(!observing) { observing = true; since = now; }
    return static_cast<uint32_t>(now - since) >= duration;
  }
};
