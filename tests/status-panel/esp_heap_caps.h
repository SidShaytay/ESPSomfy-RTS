#pragma once
#define MALLOC_CAP_INTERNAL 1
#define MALLOC_CAP_SPIRAM 2
inline unsigned heap_caps_get_free_size(int) { return 10000; }
