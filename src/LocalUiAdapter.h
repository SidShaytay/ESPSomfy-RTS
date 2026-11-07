#pragma once
#include "LocalUi.h"
namespace LocalUi {
Model snapshotModel(bool running);
uint8_t loadBrightness();
bool saveBrightness(uint8_t value);
bool dispatch(const Command &);
} // namespace LocalUi
