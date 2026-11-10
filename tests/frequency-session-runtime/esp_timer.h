#pragma once
#include <stdint.h>
inline uint64_t scanNow=100;
inline int64_t esp_timer_get_time(){return int64_t(scanNow*1000);}
