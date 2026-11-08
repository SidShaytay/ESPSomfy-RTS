#pragma once
#include <stddef.h>
#define MALLOC_CAP_SPIRAM 0x400
#define MALLOC_CAP_8BIT 0x004
void *heap_caps_aligned_alloc(size_t alignment,size_t size,unsigned caps);
void heap_caps_free(void *);
