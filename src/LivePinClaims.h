#pragma once
#include <stdint.h>
// Actual hardware initialization attempts only add claims. Configuration/RAM
// changes never release them; construction at a new boot starts a new ledger.
class LivePinClaims {
  uint64_t pins=0;
public:
  LivePinClaims()=default;
  LivePinClaims(const LivePinClaims &)=delete;
  LivePinClaims &operator=(const LivePinClaims &)=delete;
  static constexpr uint64_t pinMask(uint8_t pin){return pin<64?(uint64_t(1)<<pin):0;}
  static constexpr bool contains(uint64_t claims,uint8_t pin){return (claims&pinMask(pin))!=0;}
  void retain(uint64_t);
  uint64_t snapshot()const;
  bool usesPin(uint8_t pin)const{return contains(snapshot(),pin);}
};
static_assert(sizeof(LivePinClaims)==8,"Keep each boot pin ledger at eight bytes");
