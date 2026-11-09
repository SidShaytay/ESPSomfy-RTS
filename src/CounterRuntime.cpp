#include "CounterRuntime.h"
#include "outputs/ProtoSocket.h"
namespace {
CounterDomainResult complete(CounterDomainResult result) {
  if(result.applied)protoSocket.modelPersistence({result.file},0);
  return result;
}
}
CounterDomainResult applyPrimaryCounterCorrection(uint8_t shadeId,uint16_t code) {
  return complete(correctExplicitCounter(shadeId,code));
}
CounterDomainResult applyPrimaryCounterCorrection(uint8_t shadeId,uint16_t code,
    const PrimaryCounterReview &review,CounterCorrectionWitness &witness) {
  return complete(correctReviewedPrimaryCounter(shadeId,code,review,witness));
}
