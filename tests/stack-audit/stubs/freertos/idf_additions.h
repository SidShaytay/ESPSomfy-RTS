#pragma once
#include "task.h"
inline TaskHandle_t xTaskGetIdleTaskHandleForCore(int core){return reinterpret_cast<void*>(uintptr_t(core+1));}
