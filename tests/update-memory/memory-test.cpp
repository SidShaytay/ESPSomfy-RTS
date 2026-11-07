#include <cassert>
#include <cstdlib>
#include <limits>
#include <unordered_map>
#include "esp_heap_caps.h"
#include "GitUpdateMemory.h"
struct Allocation { size_t size; unsigned caps; };
static std::unordered_map<const void *, Allocation> allocations;
static void *(*allocate)(size_t, size_t);
static void (*deallocate)(void *);
static bool fail = false;
static size_t available = 8 * 1024 * 1024;
void *heap_caps_calloc(size_t n, size_t size, unsigned caps) {
  if(fail) return nullptr;
  void *p = std::calloc(n, size);
  if(p) allocations[p] = {n * size, caps};
  return p;
}
void heap_caps_free(void *p) { allocations.erase(p); std::free(p); }
size_t heap_caps_get_allocated_size(void *p) { return allocations.at(p).size; }
size_t heap_caps_get_total_size(unsigned) { return available; }
bool esp_ptr_external_ram(const void *p) { return allocations.at(p).caps & MALLOC_CAP_SPIRAM; }
int mbedtls_platform_set_calloc_free(void *(*a)(size_t, size_t), void (*f)(void *)) {
  allocate = a; deallocate = f; return 0;
}
int main() {
  available = 0;
  assert(!gitUpdateMemoryBegin());
  assert(!allocate);
  available = 8 * 1024 * 1024 - 65536;
  assert(gitUpdateMemoryBegin());
  auto *small = allocate(1, 128);
  auto *large = allocate(2, 16384);
  assert(small && large);
  assert(allocations[small].caps & MALLOC_CAP_INTERNAL);
  assert(allocations[large].caps & MALLOC_CAP_SPIRAM);
  for(size_t i = 0; i < 32768; ++i) assert(static_cast<unsigned char *>(large)[i] == 0);
  auto stats = gitUpdateMemoryStats();
  assert(stats.psramCurrent == 32768 && stats.psramPeak == 32768 && stats.psramAllocations == 1);
  fail = true;
  assert(!allocate(1, 4096));
  fail = false;
  assert(!allocate(std::numeric_limits<size_t>::max(), 2));
  assert(!allocate(0, 20));
  assert(gitUpdateMemoryStats().failures >= 2);
  deallocate(large); deallocate(small); deallocate(nullptr);
  assert(gitUpdateMemoryStats().psramCurrent == 0);
  assert(gitUpdateMemoryStats().psramPeak == 32768);
  assert(allocations.empty());
}
