#pragma once
// Keep the established String/byte stubs; the worker owns the fixture clock.
#define millis unusedControllerFixtureMillis
#include "../../test/stubs/Arduino.h"
#undef millis
