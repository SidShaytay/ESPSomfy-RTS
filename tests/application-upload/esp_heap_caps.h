#pragma once
#include <cstdlib>
#include <cassert>
constexpr unsigned MALLOC_CAP_SPIRAM=1,MALLOC_CAP_8BIT=2;
inline unsigned stagedAllocations=0;
inline bool failStagedAllocation=false;
inline void *heap_caps_malloc(size_t size,unsigned flags){
  assert(flags==(MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));
  if(failStagedAllocation)return nullptr;
  auto *p=malloc(size);if(p)++stagedAllocations;return p;
}
inline void heap_caps_free(void *p){if(p){assert(stagedAllocations);--stagedAllocations;free(p);}}
