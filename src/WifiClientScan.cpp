#include "WifiClientScan.h"
#include "GitUpdate.h"
void WifiClientScan::loop(WifiScanDriver &driver,uint64_t now) {
  if(running) {
    const auto n=driver.result();if(n==-1)return;
    // No consumer can retain SDK buffers after this synchronous publication.
    driver.broadcast(n);feed.finish(localToken,n>=0,driver);driver.clearResults();
    running=false;localToken=0;
    gitUpdate.releaseExternal(owner);owner={};return;
  }
  // A request waiting on connecting/roaming/maintenance cannot silently start
  // much later after its requester has left. No automatic restart/retry.
  if(wanted && (now<webRequestedAt || now-webRequestedAt>=30000)) {
    wanted=false;driver.broadcast(-2);
  }
  if((!wanted && !feed.pending()) || !driver.mayStart())return;
  GitUpdateOwnerToken acquired;
  if(!gitUpdate.acquireExternal(GitUpdateExternalOwner::configuration,acquired))return;
  owner=acquired;localToken=feed.token();wanted=false;
  running=driver.start();
  if(!running) {
    driver.broadcast(-2);feed.finish(localToken,false,driver);localToken=0;
    // A failed start did not obtain SDK result ownership: never delete another
    // scan's storage or stop another scanner during this cleanup.
    gitUpdate.releaseExternal(owner);owner={};
  }
}
