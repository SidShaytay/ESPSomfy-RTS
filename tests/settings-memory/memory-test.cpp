#include "LocalSettingsMemory.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <map>
#include "esp_heap_caps.h"
static std::map<void *,size_t> blocks;
static bool failing=false;
static unsigned attempts=0;
void *heap_caps_aligned_alloc(size_t alignment,size_t size,unsigned caps) {
  ++attempts;
  assert(alignment==alignof(max_align_t));
  assert(caps==(MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));
  if(failing) return nullptr;
  void *p=nullptr;assert(posix_memalign(&p,alignment,size)==0);memset(p,0xa5,size);blocks[p]=size;return p;
}
void heap_caps_free(void *p) {
  for(size_t i=0;i<blocks.at(p);i++) assert(static_cast<unsigned char *>(p)[i]==0);
  blocks.erase(p);free(p);
}
int main() {
  auto &memory=LocalUi::Settings::runtimeMemory();
  assert(&memory==&LocalUi::Settings::runtimeMemory());
  assert(memory.used()==0);
  void *p=memory.allocate(64);assert(p);memset(p,'S',64);
  const auto occupied=memory.used();assert(occupied>64 && occupied<LocalUi::Settings::runtimeMemoryBudget);
  const auto before=attempts;
  assert(!memory.allocate(LocalUi::Settings::runtimeMemoryBudget));assert(attempts==before);
  failing=true;assert(!memory.allocate(64));assert(attempts==before+1 && memory.used()==occupied);
  memory.deallocate(p);assert(blocks.empty() && memory.used()==0);
}
