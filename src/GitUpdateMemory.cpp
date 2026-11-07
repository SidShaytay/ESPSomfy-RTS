#include "GitUpdateMemory.h"
#if defined(ESPSOMFY_LILYGO_T_EMBED_CC1101_PLUS)
#include <atomic>
#include <limits>
#include <esp_heap_caps.h>
#include <esp_memory_utils.h>
#include <mbedtls/platform.h>

namespace {
std::atomic<size_t> current{0}, peak{0}, allocations{0}, failures{0};
bool initialized = false;
void *tlsCalloc(size_t count, size_t size) {
  if(!count || !size) return nullptr;
  if(count > std::numeric_limits<size_t>::max() / size) {
    ++failures;
    return nullptr;
  }
  const size_t bytes = count * size;
  const bool external = bytes >= 1024;
  void *ptr = heap_caps_calloc(count, size,
    MALLOC_CAP_8BIT | (external ? MALLOC_CAP_SPIRAM : MALLOC_CAP_INTERNAL));
  // Deliberately do not fall back to internal RAM on PSRAM exhaustion.
  if(!ptr) { ++failures; return nullptr; }
  if(external) {
    const size_t live = current.fetch_add(heap_caps_get_allocated_size(ptr)) + heap_caps_get_allocated_size(ptr);
    size_t previous = peak.load();
    while(previous < live && !peak.compare_exchange_weak(previous, live)) {}
    ++allocations;
  }
  return ptr;
}
void tlsFree(void *ptr) {
  if(!ptr) return;
  if(esp_ptr_external_ram(ptr)) current.fetch_sub(heap_caps_get_allocated_size(ptr));
  heap_caps_free(ptr);
}
}
bool gitUpdateMemoryBegin() {
  if(initialized) return true;
  // Allocator-managed capacity is smaller than physical PSRAM. The updater
  // checks the physical 8 MiB separately and must successfully stage its image.
  if(heap_caps_get_total_size(MALLOC_CAP_SPIRAM) < 4u * 1024 * 1024) return false;
  // The pinned binary SDK actually exports this hook (verified by nm and the
  // linked ELF). Application CONFIG_MBEDTLS_* flags cannot reconfigure it.
  initialized = mbedtls_platform_set_calloc_free(tlsCalloc, tlsFree) == 0;
  return initialized;
}
GitUpdateMemoryStats gitUpdateMemoryStats() {
  return {current.load(), peak.load(), allocations.load(), failures.load()};
}
bool gitUpdateMemoryReady() { return initialized; }
#else
bool gitUpdateMemoryBegin() { return false; }
bool gitUpdateMemoryReady() { return false; }
GitUpdateMemoryStats gitUpdateMemoryStats() { return {0, 0, 0, 0}; }
#endif
