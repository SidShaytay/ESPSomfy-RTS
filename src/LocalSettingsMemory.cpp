#ifdef ESPSOMFY_LOCAL_UI
#include "LocalSettingsMemory.h"
#include <esp_heap_caps.h>
namespace LocalUi { namespace Settings {
namespace {
void *acquire(size_t size) {
  return heap_caps_aligned_alloc(alignof(max_align_t),size,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
}
void release(void *p) { heap_caps_free(p); }
}
JsonAllocator &runtimeMemory() {
  static JsonAllocator memory(runtimeMemoryBudget,acquire,release);
  return memory;
}
} }
#endif
