#pragma once
#include <stddef.h>
#include <stdlib.h>
#include <assert.h>
#define MALLOC_CAP_SPIRAM 1
#define MALLOC_CAP_8BIT 2
extern bool hasPsram, allocateOK;
extern void *allocated;
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
  assert(p == allocated);
  free(p);
  allocated = nullptr;
}
