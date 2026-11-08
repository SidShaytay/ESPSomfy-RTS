#pragma once
#include <stddef.h>
#include <stdlib.h>
#include <assert.h>
#include <map>
#include <string.h>
#define MALLOC_CAP_SPIRAM 1
#define MALLOC_CAP_8BIT 2
extern bool hasPsram, allocateOK;
extern void *allocated;
#ifdef LOCAL_SETTINGS_HOST_TEST
extern bool settingsAllocateOK;
extern std::map<void *,size_t> settingsAllocations;
inline void *heap_caps_aligned_alloc(size_t alignment,size_t size,int caps) {
  assert(alignment==alignof(max_align_t) && caps==(MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));
  if(!settingsAllocateOK) return nullptr;
  void *p=nullptr;assert(posix_memalign(&p,alignment,size)==0);
  memset(p,0xa5,size);settingsAllocations[p]=size;return p;
}
#endif
inline size_t heap_caps_get_total_size(int caps) {
  assert(caps == MALLOC_CAP_SPIRAM);
  return hasPsram ? 8 * 1024 * 1024 : 0;
}
inline void *heap_caps_malloc(size_t size, int caps) {
  assert((size == 108800 || size == 108866) && caps == (MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if(!allocateOK)
    return nullptr;
  assert(!allocated);
  return allocated = malloc(size);
}

inline void heap_caps_free(void *p) {
  if(p==allocated) {free(p);allocated=nullptr;return;}
#ifdef LOCAL_SETTINGS_HOST_TEST
  for(size_t i=0;i<settingsAllocations.at(p);i++) assert(static_cast<unsigned char *>(p)[i]==0);
  settingsAllocations.erase(p);free(p);
#else
  assert(false);
#endif
}
