#pragma once
#include "SomfyFrame.h"
// Loop-owned fresh model admission. No counter read/write, RF, GPIO or state
// mutation. The any-task TX sink independently checks the exact identity.
bool counterCommandAllowed(uint32_t address,somfy_commands command);
