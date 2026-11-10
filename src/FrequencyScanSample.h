#pragma once
#include <stdint.h>
namespace FrequencyScan {
struct Sample {float frequency=0,peakFrequency=0;int16_t rssi=-100,peakRssi=-100;};
}
