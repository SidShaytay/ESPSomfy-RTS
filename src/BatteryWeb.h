#pragma once
#include "LoopQueue.h"
// Protocol 2 battery op: the gauge reading and the optional charge limit. loop() only.
namespace BatteryWeb {
void runProtocol(const loop_item_t &item);
}
