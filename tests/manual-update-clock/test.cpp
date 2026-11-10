#include "GitUpdate.h"
#include "ManualUpdateLifecycle.h"
#include <cassert>
#include <cstdint>
#include <cstdio>
static uint64_t clockNow=1000;
uint64_t gitUpdateTransferClockNow(){return clockNow;}
static void prepared_filesystem_uses_the_serialized_runtime_clock(){
 GitUpdateOwnerToken owner;const ManualApplicationSource source{77,9,4096};
 clockNow=3000;assert(gitUpdate.acquireExternal(GitUpdateExternalOwner::manual,owner));
 assert(gitUpdate.beginTransfer(owner,ManualUpdateKind::application,2990));
 clockNow=3001;assert(gitUpdate.prepareApplication(owner,source,2990));
 clockNow=3002;assert(gitUpdate.claimApplicationPreparation(owner,source,2990));
 assert(gitUpdate.transferFilesystem(owner,true));assert(gitUpdate.applicationPrepared(owner));
 clockNow=3003;assert(gitUpdate.finishTransfer(owner,2990,true));
 clockNow=3004;assert(gitUpdate.claimPreparedApplication(owner,source,2990));
 assert(gitUpdate.transferResource(owner,true));assert(gitUpdate.transferResource(owner,false));
 clockNow=3005;assert(gitUpdate.holdTransferRecovery(owner,2990));
 clockNow=3006;assert(gitUpdate.finishTransfer(owner,2990,true));
 clockNow=3007;assert(gitUpdate.claimTransferRecovery(owner,2990));
 assert(gitUpdate.transferFilesystem(owner,false));assert(gitUpdate.holdTransferRestart(owner));
 clockNow=3008;assert(gitUpdate.finishTransfer(owner,2990,true));
 clockNow=3009;assert(gitUpdate.transferRestartReady(owner,2990));
 clockNow=3010;assert(gitUpdate.releaseRestartSource(owner,2990));
 // A delayed exact-source cleanup observation must not retire a live receiver.
 clockNow=4000;assert(gitUpdate.acquireExternal(GitUpdateExternalOwner::manual,owner));
 assert(gitUpdate.beginTransfer(owner,ManualUpdateKind::firmware,3990));
 clockNow=4001;assert(!gitUpdate.claimTransferCleanup(owner,3990).token.generation);
 assert(gitUpdate.claimTransfer(owner,3990));assert(gitUpdate.closeTransfer(owner));assert(gitUpdate.releaseExternal(owner));
 // The new preparation API must still reject an actual backwards clock.
 clockNow=5000;assert(gitUpdate.acquireExternal(GitUpdateExternalOwner::manual,owner));
 assert(gitUpdate.beginTransfer(owner,ManualUpdateKind::application,5000));
 clockNow=4999;assert(!gitUpdate.prepareApplication(owner,source,5001));
 clockNow=5002;assert(gitUpdate.claimTransferCleanup(owner,5002).token.generation==owner.generation);
 assert(gitUpdate.closeTransfer(owner));assert(gitUpdate.releaseExternal(owner));
}
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
 prepared_filesystem_uses_the_serialized_runtime_clock();
 std::puts("Serialized manual transfer clock: reordered caller snapshots survive; real rollback remains refused");
}
