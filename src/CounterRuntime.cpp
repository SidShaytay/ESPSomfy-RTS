#include "CounterRuntime.h"
#include "outputs/ProtoSocket.h"
CounterDomainResult applyPrimaryCounterCorrection(uint8_t shadeId,uint16_t code,
                                                  const PrimaryCounterReview *review) {
  const auto result=review ? correctReviewedPrimaryCounter(shadeId,code,*review)
                           : correctExplicitCounter(shadeId,code);
  if(result.applied)protoSocket.modelPersistence({result.file},0);
  return result;
}
