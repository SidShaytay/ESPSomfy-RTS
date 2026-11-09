#include "WifiScanFeed.h"
uint64_t WifiScanFeed::request(Completion callback,void *user) {
  if(active || !callback || !user)return 0;
  const auto next=nextGeneration(last);if(!next)return 0;
  last=active=next;completed=callback;context=user;return active;
}
bool WifiScanFeed::cancel(uint64_t token) {
  if(!token || token!=active)return false;
  active=0;completed=nullptr;context=nullptr;return true;
}
void WifiScanFeed::finish(uint64_t token,bool success,const WifiScanReader &reader) {
  if(!token || token!=active)return;
  const auto callback=completed;auto *user=context;
  cancel(token); // Retire the old source before a callback can register again.
  callback(user,token,success,reader);
}
WifiScanFeed &wifiScanFeed(){static WifiScanFeed feed;return feed;}
