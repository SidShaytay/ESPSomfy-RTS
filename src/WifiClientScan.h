#pragma once
#include "WifiScanFeed.h"
#include "GitUpdateOwner.h"
// Physical operations belong to the network loop. Results are borrowed until
// clearResults; a local subscriber receives only its captured scan generation.
class WifiScanDriver : public WifiScanReader {
public:
  virtual bool mayStart() const=0;
  virtual bool start()=0;
  // -1 means still running; -2 means failed; nonnegative is the result count.
  virtual int result()=0;
  virtual void broadcast(int)=0;
  virtual void clearResults()=0;
};
class WifiClientScan {
  WifiScanFeed &feed;
  GitUpdateOwnerToken owner;
  uint64_t localToken=0,webRequestedAt=0;
  bool wanted=false,running=false;
public:
  explicit WifiClientScan(WifiScanFeed &source):feed(source){}
  WifiClientScan(const WifiClientScan &)=delete;
  WifiClientScan &operator=(const WifiClientScan &)=delete;
  void requestWeb(uint64_t now){if(!running && !wanted){wanted=true;webRequestedAt=now;}}
  bool active() const{return running;}
  bool pending() const{return wanted || feed.pending();}
  void loop(WifiScanDriver &,uint64_t now);
};
