#pragma once
#include <stdint.h>
// Actual GPIO output initialization only adds boot-lifetime physical claims.
// Removing model metadata cannot disconnect the externally wired actuator.
void retainGpioPinClaims(uint64_t);
uint64_t retainedGpioPinClaims();
