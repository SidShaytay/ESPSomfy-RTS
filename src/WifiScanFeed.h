#pragma once
#include <stdint.h>
// Loop-owned, ephemeral results: a consumer may copy bounded rows during its
// callback, but must not retain this reader or any SDK pointers.
struct WifiScanNetwork {
  char ssid[33]={};
  uint8_t bssid[6]={};
  int16_t rssi=0;
  uint8_t channel=0,authentication=0;
};
static_assert(sizeof(WifiScanNetwork)<=44,"Bound copied WiFi network rows");
class WifiScanReader {
public:
  virtual ~WifiScanReader()=default;
  virtual uint16_t count() const=0;
  virtual bool read(uint16_t,WifiScanNetwork &) const=0;
};
class WifiScanFeed {
public:
  using Completion=void (*)(void *,uint64_t,bool,const WifiScanReader &);
  static constexpr uint64_t nextGeneration(uint64_t current){return current==UINT64_MAX?0:current+1;}
private:
  Completion completed=nullptr;
  void *context=nullptr;
  uint64_t last=0,active=0;
public:
  WifiScanFeed()=default;
  WifiScanFeed(const WifiScanFeed &)=delete;
  WifiScanFeed &operator=(const WifiScanFeed &)=delete;
  uint64_t request(Completion callback,void *user);
  bool cancel(uint64_t token);
  bool pending() const {return active!=0;}
  uint64_t token() const {return active;}
  // The network service captures the token WHEN starting the physical scan.
  // A cancelled/replaced subscriber cannot inherit an old SDK completion.
  void finish(uint64_t token,bool success,const WifiScanReader &);
};
WifiScanFeed &wifiScanFeed();
