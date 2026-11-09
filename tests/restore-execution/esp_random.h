#pragma once
#include <stdint.h>
uint32_t restoreTestRandom();
inline uint32_t esp_random(){return restoreTestRandom();}
