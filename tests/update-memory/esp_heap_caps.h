#pragma once
#include <cstddef>
#define MALLOC_CAP_SPIRAM 1
#define MALLOC_CAP_INTERNAL 2
#define MALLOC_CAP_8BIT 4
void *heap_caps_aligned_alloc(size_t,size_t,unsigned);
void heap_caps_free(void *);
size_t heap_caps_get_total_size(unsigned);
