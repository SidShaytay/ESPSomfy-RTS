#include "GitUpdate.h"
#include "ManualUpdateLifecycle.h"
#include <cassert>
#include <cstdint>
#include <cstdio>
static uint64_t clockNow=1000;
uint64_t gitUpdateTransferClockNow(){return clockNow;}
int main(){
 GitUpdateOwnerToken owner;
 assert(gitUpdate.acquireExternal(GitUpdateExternalOwner::manual,owner));
 assert(gitUpdate.beginTransfer(owner,ManualUpdateKind::firmware,1000));
 assert(gitUpdate.claimTransfer(owner,1000));assert(gitUpdate.transferResource(owner,true));
 // A callback sampled1001 before waiting for the service lock. Cleanup sampled
 //1002 and acquired it first. The actual clock never moved backward.
 clockNow=1002;assert(!gitUpdate.claimTransferCleanup(1002).token.generation);
 clockNow=1003;assert(gitUpdate.transferContinuing(owner,1001));
 assert(gitUpdate.transferResource(owner,false));assert(gitUpdate.holdTransferRestart(owner));
 clockNow=1004;assert(gitUpdate.finishTransfer(owner,1001,true));
 clockNow=1005;assert(gitUpdate.releaseRestartSource(owner,1001));
 assert(gitUpdate.acquireExternal(GitUpdateExternalOwner::manual,owner));
 clockNow=2000;assert(gitUpdate.beginTransfer(owner,ManualUpdateKind::firmware,2000));
 // A genuine runtime clock rollback must still retire the source.
 clockNow=1999;assert(!gitUpdate.claimTransfer(owner,2001));
 clockNow=2002;assert(gitUpdate.claimTransferCleanup(2002).token.generation==owner.generation);
 assert(gitUpdate.closeTransfer(owner));assert(gitUpdate.releaseExternal(owner));
 std::puts("Serialized manual transfer clock: reordered caller snapshots survive; real rollback remains refused");
}
