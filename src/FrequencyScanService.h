#pragma once
#include "FrequencyScanSession.h"
namespace FrequencyScan {
// All functions are loop-owned. Socket tasks only parse/copy a token to FIFO.
Source webSource(uint32_t client);
Source localSource(uint32_t session,uint32_t configuration);
bool begin(const Source &,FrequencyScanToken &);
bool stop(const Source &,const FrequencyScanToken &);
void poll();
Snapshot status();
bool peak(const Source &,const FrequencyScanToken &,float &frequency);
}
