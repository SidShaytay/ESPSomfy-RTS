#pragma once
#include "ExplicitCounter.h"
// Loop-only complete primary correction. The checked domain owns all alias
// changes and one NVS/file attempt. Publish this operation's model-file outcome
// once only when applied, including a failed/uncertain file. No counter replay.
CounterDomainResult applyPrimaryCounterCorrection(uint8_t shadeId,uint16_t code,
                                                  const PrimaryCounterReview *review=nullptr);
