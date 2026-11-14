#pragma once
#include <stdint.h>
namespace LocalUi {
// Keep provenance through raw-button debounce and the release tick.
inline bool syntheticSeen = false;
inline uint32_t syntheticAt = 0;
inline void noteAutomatedInput(uint32_t now) { syntheticSeen = true; syntheticAt = now; }
inline bool automatedInput(uint32_t now) { return syntheticSeen && uint32_t(now - syntheticAt) < 1000; }
}
