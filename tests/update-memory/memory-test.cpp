#include "GitUpdateMemory.h"
#include "esp_heap_caps.h"
#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <map>
#include <vector>
struct Allocation { size_t bytes;unsigned caps; };
std::map<void *,Allocation> blocks;
static void *(*allocate)(size_t,size_t)=nullptr;
static void (*release)(void *)=nullptr;
static size_t available=0,calls=0,hooks=0;
static bool fail=false;
void *heap_caps_aligned_alloc(size_t alignment,size_t bytes,unsigned caps) {
  ++calls;assert(alignment==alignof(max_align_t));
  if(fail) return nullptr;
  void *p=nullptr;assert(posix_memalign(&p,alignment,bytes)==0);
  memset(p,0xa5,bytes);blocks[p]={bytes,caps};return p;
}
void heap_caps_free(void *p) {
  for(size_t i=0;i<blocks.at(p).bytes;i++) assert(static_cast<unsigned char *>(p)[i]==0);
  blocks.erase(p);free(p);
}
size_t heap_caps_get_total_size(unsigned) { return available; }
int mbedtls_platform_set_calloc_free(void *(*a)(size_t,size_t),void (*f)(void *)) { ++hooks;allocate=a;release=f;return 0; }
int main() {
  assert(!gitUpdateMemoryBegin() && !allocate);
  available=8u*1024*1024;
  assert(gitUpdateMemoryBegin() && gitUpdateMemoryBegin() && hooks==1);
  void *small=allocate(1,128);assert(small);
  assert(blocks.begin()->second.caps==(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT));
  void *large=allocate(2,16384);assert(large);
  for(size_t i=0;i<32768;i++) assert(static_cast<unsigned char *>(large)[i]==0);
  assert(reinterpret_cast<uintptr_t>(large)%alignof(max_align_t)==0);
  assert(gitUpdateMemoryStats().requested>32768+128);
  std::vector<void *> tiny;
  while(void *p=allocate(1,1000)) tiny.push_back(p);
  assert(gitUpdateMemoryStats().internalRequested<=GIT_UPDATE_TLS_INTERNAL_BUDGET);
  assert(gitUpdateMemoryStats().internalRequested>GIT_UPDATE_TLS_INTERNAL_BUDGET-2048);
  for(auto *p:tiny) release(p);
  release(small);release(large);release(nullptr);assert(blocks.empty());
  auto before=gitUpdateMemoryStats();auto beforeCalls=calls;
  assert(!allocate(1,GIT_UPDATE_TLS_BUDGET));assert(calls==beforeCalls);
  assert(!allocate(std::numeric_limits<size_t>::max(),2));
  assert(!allocate(0,2));
  fail=true;assert(!allocate(1,4096));assert(calls==beforeCalls+1);
  assert(gitUpdateMemoryStats().requested==0 && gitUpdateMemoryStats().internalRequested==0);
  assert(gitUpdateMemoryStats().failures>=before.failures+3);
  fail=false;
  void *near=allocate(1,GIT_UPDATE_TLS_BUDGET-256);assert(near);
  assert(!allocate(1,1024));release(near);
  assert(blocks.empty() && gitUpdateMemoryStats().requested==0);
}
