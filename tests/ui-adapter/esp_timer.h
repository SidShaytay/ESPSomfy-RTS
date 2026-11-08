#pragma once
#include <stdint.h>
extern uint64_t authTestNow;
inline int64_t esp_timer_get_time() { return int64_t(authTestNow*1000); }
