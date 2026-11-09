#include "GpioPinClaims.h"
#include "LivePinClaims.h"
namespace {LivePinClaims outputs;}
void retainGpioPinClaims(uint64_t pins){outputs.retain(pins);}
uint64_t retainedGpioPinClaims(){return outputs.snapshot();}
