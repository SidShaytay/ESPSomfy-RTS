#pragma once
#include <cstdint>
uint32_t applicationTestRandom();
inline uint32_t esp_random(){return applicationTestRandom();}
