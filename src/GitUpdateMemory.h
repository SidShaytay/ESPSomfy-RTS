#pragma once
#include <cstddef>
struct GitUpdateMemoryStats {
  size_t psramCurrent;
  size_t psramPeak;
  size_t psramAllocations;
  size_t failures;
};
// Install once, before networking starts. Never switch allocators around a
// handshake: concurrent TLS sessions must use the same allocation/free pair.
bool gitUpdateMemoryBegin();
bool gitUpdateMemoryReady();
GitUpdateMemoryStats gitUpdateMemoryStats();
