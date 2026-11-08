#include "GitUpdateMemory.h"
#if defined(ESPSOMFY_LILYGO_T_EMBED_CC1101)
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <new>
#include <esp_heap_caps.h>
#include <mbedtls/platform.h>
namespace {
struct alignas(max_align_t) Block { size_t bytes;bool internal; };
std::atomic<size_t> requested{0},peak{0},internal{0},internalPeak{0},allocations{0},failures{0};
std::atomic<unsigned> initialized{0}; // 0 not installed, 1 installing, 2 ready
bool reserve(std::atomic<size_t> &counter,size_t bytes,size_t limit) {
  if(bytes>limit) return false;
  size_t current=counter.load();
  do { if(current>limit-bytes) return false; }
  while(!counter.compare_exchange_weak(current,current+bytes));
  return true;
}
void updatePeak(std::atomic<size_t> &maximum,size_t value) {
  size_t before=maximum.load();
  while(before<value && !maximum.compare_exchange_weak(before,value)) {}
}
void *tlsCalloc(size_t count,size_t size) {
  if(!count || !size) return nullptr;
  if(count>std::numeric_limits<size_t>::max()/size) { ++failures;return nullptr; }
  const size_t payload=count*size;
  if(payload>std::numeric_limits<size_t>::max()-sizeof(Block)) { ++failures;return nullptr; }
  const size_t bytes=sizeof(Block)+payload;
  const bool small=payload<1024;
  if(!reserve(requested,bytes,GIT_UPDATE_TLS_BUDGET)) { ++failures;return nullptr; }
  if(small && !reserve(internal,bytes,GIT_UPDATE_TLS_INTERNAL_BUDGET)) {
    requested.fetch_sub(bytes);++failures;return nullptr;
  }
  void *raw=heap_caps_aligned_alloc(alignof(max_align_t),bytes,
    MALLOC_CAP_8BIT|(small ? MALLOC_CAP_INTERNAL : MALLOC_CAP_SPIRAM));
  if(!raw) {
    if(small) internal.fetch_sub(bytes);
    requested.fetch_sub(bytes);++failures;return nullptr;
  }
  // Use one aligned allocation/free pair for all mbedTLS sessions. Never fall
  // back to internal RAM after a required PSRAM allocation fails.
  memset(raw,0,bytes);
  auto *block=new(raw) Block{bytes,small};
  updatePeak(peak,requested.load());updatePeak(internalPeak,internal.load());
  ++allocations;
  return block+1;
}
void tlsFree(void *p) {
  if(!p) return;
  auto *block=static_cast<Block *>(p)-1;
  const size_t bytes=block->bytes;
  const bool small=block->internal;
  auto *out=reinterpret_cast<volatile uint8_t *>(block);
  for(size_t n=0;n<bytes;n++) out[n]=0;
  heap_caps_free(block);
  if(small) internal.fetch_sub(bytes);
  requested.fetch_sub(bytes);
}
}
bool gitUpdateMemoryBegin() {
  if(initialized.load()==2) return true;
  if(heap_caps_get_total_size(MALLOC_CAP_SPIRAM)<4u*1024*1024) return false;
  unsigned expected=0;
  if(!initialized.compare_exchange_strong(expected,1)) return false;
  const bool ready=mbedtls_platform_set_calloc_free(tlsCalloc,tlsFree)==0;
  initialized.store(ready ? 2 : 0);
  return ready;
}
bool gitUpdateMemoryReady() { return initialized.load()==2; }
GitUpdateMemoryStats gitUpdateMemoryStats() {
  return {requested.load(),peak.load(),internal.load(),internalPeak.load(),allocations.load(),failures.load()};
}
#else
bool gitUpdateMemoryBegin() { return false; }
bool gitUpdateMemoryReady() { return false; }
GitUpdateMemoryStats gitUpdateMemoryStats() { return {}; }
#endif
