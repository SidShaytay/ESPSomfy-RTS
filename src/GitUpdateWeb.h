#pragma once
#include "LoopQueue.h"
// Protocol 2 gitUpdate op: the web UI's view of the one shared GitHub updater.
// loop() only. Status, check, install and cancel use the same controller, trust
// checks, admission and restart path as the LCD; nothing here downloads.
namespace GitUpdateWeb {
void runProtocol(const loop_item_t &item);
}
