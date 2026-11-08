#pragma once
#include <cstddef>
struct GitUpdateMemoryStats {
  size_t requested=0,peak=0,internalRequested=0,internalPeak=0,allocations=0,failures=0;
};
// Requested bytes include our aligned bookkeeping, not SDK heap overhead.
constexpr size_t GIT_UPDATE_TLS_BUDGET=512u*1024;
constexpr size_t GIT_UPDATE_TLS_INTERNAL_BUDGET=32u*1024;
// Install ONCE before ANY TLS allocator use, never around a handshake. Earlier
// TLS allocations must never be freed through this new allocation/free pair.
// This function is intentionally not invoked by current firmware startup.
bool gitUpdateMemoryBegin();
bool gitUpdateMemoryReady();
GitUpdateMemoryStats gitUpdateMemoryStats();
