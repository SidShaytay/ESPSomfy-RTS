#pragma once
#include "WiFi.h"
struct EthernetFake {
  Address localIP()const{return {0xc0000201u};}
  unsigned linkSpeed()const{return 100;}
  bool fullDuplex()const{return true;}
};
inline EthernetFake ETH;
