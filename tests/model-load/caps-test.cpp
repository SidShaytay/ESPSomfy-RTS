#include "ModelLoadSnapshot.h"
#include "esp_heap_caps.h"
#include <cassert>
static unsigned allocations=0;
void *heap_caps_malloc(size_t length,unsigned caps){assert(length==1024&&caps==(MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));++allocations;return nullptr;}
int main(){assert(!modelLoadIo().allocate(1024)&&allocations==1);}
