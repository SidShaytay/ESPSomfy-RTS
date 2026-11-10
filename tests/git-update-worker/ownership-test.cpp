#include "GitUpdate.h"
#include "ManualUpdateLifecycle.h"
#include <cassert>
#include <cstdio>
#include <atomic>
#include <thread>
#include <vector>
static void applicationFilesystemRetainsSameOwnerWithoutInventingSdkOwnership() {
  GitUpdateOwnerToken own,competing;assert(gitUpdate.acquireExternal(GitUpdateExternalOwner::manual,own));
  assert(gitUpdate.beginTransfer(own,ManualUpdateKind::application,1));assert(gitUpdate.claimTransfer(own,1));
  assert(gitUpdate.transferFilesystem(own,true));assert(!gitUpdate.closeTransfer(own));
  assert(gitUpdate.retireTransfer(own));assert(gitUpdate.finishTransfer(own,2,false));
  auto cleanup=gitUpdate.claimTransferCleanup(2);assert(cleanup.token.generation==own.generation);
  assert(cleanup.filesystemUnavailable&&!cleanup.resourceOpen);assert(!gitUpdate.closeTransfer(own));
  assert(!gitUpdate.releaseExternal(own));assert(!gitUpdate.acquireExternal(GitUpdateExternalOwner::manual,competing));
  auto stale=own;++stale.generation;assert(!gitUpdate.transferFilesystem(stale,false));
  assert(gitUpdate.transferFilesystem(own,false));assert(gitUpdate.closeTransfer(own));assert(gitUpdate.releaseExternal(own));
}
static void preparedApplicationUsesExistingOwnerAndExactSource() {
  GitUpdateOwnerToken own,other;const ManualApplicationSource source{0x1234,17,4096};
  assert(gitUpdate.acquireExternal(GitUpdateExternalOwner::manual,own));assert(gitUpdate.beginTransfer(own,ManualUpdateKind::application,1));
  assert(gitUpdate.prepareApplication(own,source,1));assert(!gitUpdate.claimTransfer(own,2));
  assert(gitUpdate.claimApplicationPreparation(own,source,2));assert(gitUpdate.transferFilesystem(own,true));
  assert(gitUpdate.applicationPrepared(own));assert(gitUpdate.finishTransfer(own,2,true));assert(!gitUpdate.retireTransfer(own));
  assert(!gitUpdate.acquireExternal(GitUpdateExternalOwner::configuration,other));auto wrong=source;++wrong.securityRevision;
  assert(!gitUpdate.claimPreparedApplication(own,wrong,3));assert(gitUpdate.claimPreparedApplication(own,source,3));
  assert(!gitUpdate.cancelApplicationPreparation(own,source));
  assert(gitUpdate.transferFilesystem(own,false));assert(gitUpdate.closeTransfer(own));assert(gitUpdate.releaseExternal(own));
}
static void configurationLease() {
  GitUpdateOwnerToken configuration,competing;
  for(auto kind:{GitUpdateExternalOwner::manual,GitUpdateExternalOwner::restore,GitUpdateExternalOwner::counter}) {
    assert(gitUpdate.acquireExternal(kind,competing));
    assert(!gitUpdate.acquireExternal(GitUpdateExternalOwner::configuration,configuration));
    assert(gitUpdate.releaseExternal(competing));
  }
  assert(gitUpdate.acquireRestart(competing,1));
  assert(!gitUpdate.acquireExternal(GitUpdateExternalOwner::configuration,configuration));
  assert(gitUpdate.releaseExternal(competing));
  assert(gitUpdate.acquireExternal(GitUpdateExternalOwner::configuration,configuration));
  assert(configuration.generation && configuration.kind==GitUpdateExternalOwner::configuration);
  assert(gitUpdate.ownsExternal(configuration) && gitUpdate.busy());
  assert(gitUpdate.requestCheck()==GitUpdateRequest::busy);
  for(auto kind:{GitUpdateExternalOwner::manual,GitUpdateExternalOwner::restore,GitUpdateExternalOwner::counter,GitUpdateExternalOwner::configuration})
    assert(!gitUpdate.acquireExternal(kind,competing));
  assert(!gitUpdate.acquireRestart(competing,2));
  auto wrong=configuration;wrong.kind=GitUpdateExternalOwner::counter;
  assert(!gitUpdate.ownsExternal(wrong) && !gitUpdate.releaseExternal(wrong));
  wrong=configuration;wrong.kind=static_cast<GitUpdateExternalOwner>(255);
  assert(!gitUpdate.ownsExternal(wrong) && !gitUpdate.releaseExternal(wrong));
  wrong=configuration;++wrong.generation;
  assert(!gitUpdate.ownsExternal(wrong) && !gitUpdate.releaseExternal(wrong));
  wrong=configuration;wrong.generation=0;
  assert(!gitUpdate.releaseExternal(wrong));
  assert(gitUpdate.ownsExternal(configuration));
  assert(gitUpdate.releaseExternal(configuration));
  assert(!gitUpdate.releaseExternal(configuration) && !gitUpdate.ownsExternal(configuration));
  assert(gitUpdate.acquireExternal(GitUpdateExternalOwner::configuration,competing));
  assert(competing.generation!=configuration.generation);
  assert(!gitUpdate.releaseExternal(configuration));
  assert(gitUpdate.releaseExternal(competing));
  assert(!gitUpdate.busy());
}
static void cancelledRestartSource() {
  GitUpdateOwnerToken source;
  assert(gitUpdate.acquireExternal(GitUpdateExternalOwner::manual,source));
  assert(gitUpdate.beginTransfer(source,ManualUpdateKind::firmware,1));
  assert(!gitUpdate.releaseRestartSource(source,1));
  assert(gitUpdate.claimTransfer(source,1));assert(gitUpdate.transferResource(source,true));
  assert(!gitUpdate.releaseRestartSource(source,1));
  assert(gitUpdate.transferResource(source,false));assert(gitUpdate.holdTransferRestart(source));
  assert(!gitUpdate.releaseRestartSource(source,1)); // Still owns an I/O claim.
  assert(gitUpdate.finishTransfer(source,1,true));
  auto wrong=source;wrong.kind=GitUpdateExternalOwner::restore;
  assert(!gitUpdate.releaseRestartSource(wrong,1));wrong=source;++wrong.generation;
  assert(!gitUpdate.releaseRestartSource(wrong,1));assert(gitUpdate.ownsExternal(source));
  assert(gitUpdate.releaseRestartSource(source,1));assert(!gitUpdate.busy());
  GitUpdateOwnerToken next;assert(gitUpdate.acquireExternal(GitUpdateExternalOwner::manual,next));
  assert(!gitUpdate.releaseRestartSource(source,1));assert(gitUpdate.ownsExternal(next));
  assert(gitUpdate.releaseExternal(next));
  assert(gitUpdate.acquireExternal(GitUpdateExternalOwner::manual,source));
  assert(gitUpdate.beginTransfer(source,ManualUpdateKind::firmware,1));assert(gitUpdate.claimTransfer(source,1));
  assert(gitUpdate.transferResource(source,true));assert(gitUpdate.transferResource(source,false));
  assert(gitUpdate.holdTransferRestart(source));assert(gitUpdate.finishTransfer(source,1,true));
  assert(!gitUpdate.releaseRestartSource(source,30001));assert(gitUpdate.ownsExternal(source));
  const auto cleanup=gitUpdate.claimTransferCleanup(30001);assert(cleanup.token.generation==source.generation);
  assert(gitUpdate.closeTransfer(source));assert(gitUpdate.releaseExternal(source));
  assert(gitUpdate.acquireExternal(GitUpdateExternalOwner::manual,source));
  assert(gitUpdate.beginTransfer(source,ManualUpdateKind::firmware,1));assert(gitUpdate.claimTransfer(source,1));
  assert(gitUpdate.transferResource(source,true));assert(gitUpdate.transferResource(source,false));
  assert(gitUpdate.holdTransferRestart(source));assert(gitUpdate.finishTransfer(source,1,true));
  assert(gitUpdate.acquireRestart(next,1,source));assert(!gitUpdate.releaseRestartSource(source,1));
  assert(gitUpdate.ownsExternal(next));assert(gitUpdate.releaseExternal(next));
}
static void exactCleanupNeverClaimsReplacementOrForeignSource() {
  GitUpdateOwnerToken previous,current;
  assert(gitUpdate.acquireExternal(GitUpdateExternalOwner::manual,previous));
  assert(gitUpdate.beginTransfer(previous,ManualUpdateKind::application,1));
  assert(gitUpdate.retireTransfer(previous));
  assert(gitUpdate.claimTransferCleanup(previous,2).token.generation==previous.generation);
  assert(gitUpdate.closeTransfer(previous)&&gitUpdate.releaseExternal(previous));
  assert(gitUpdate.acquireExternal(GitUpdateExternalOwner::manual,current));
  assert(gitUpdate.beginTransfer(current,ManualUpdateKind::application,3));
  assert(gitUpdate.retireTransfer(current));
  assert(!gitUpdate.claimTransferCleanup(previous,40000).token.generation);
  auto wrong=current;wrong.kind=GitUpdateExternalOwner::restore;
  assert(!gitUpdate.claimTransferCleanup(wrong,40000).token.generation);
  assert(gitUpdate.claimTransferCleanup(current,4).token.generation==current.generation);
  assert(gitUpdate.closeTransfer(current)&&gitUpdate.releaseExternal(current));
}
int main() {exactCleanupNeverClaimsReplacementOrForeignSource();preparedApplicationUsesExistingOwnerAndExactSource();applicationFilesystemRetainsSameOwnerWithoutInventingSdkOwnership();
  configurationLease();
  cancelledRestartSource();
  auto before=gitUpdate.snapshot();
  assert(!before.busy && !before.canInstall);
  GitUpdateOwnerToken first,second;
  assert(gitUpdate.acquireExternal(GitUpdateExternalOwner::manual,first));
  assert(first.generation && gitUpdate.busy());
  assert(!gitUpdate.acquireExternal(GitUpdateExternalOwner::restore,second));
  assert(gitUpdate.requestCheck()==GitUpdateRequest::busy);
  assert(gitUpdate.releaseExternal(first));
  assert(!gitUpdate.releaseExternal(first));
  assert(gitUpdate.acquireExternal(GitUpdateExternalOwner::restore,second));
  assert(second.generation!=first.generation);
  assert(!gitUpdate.releaseExternal(first));
  assert(gitUpdate.busy());
  auto wrong=second;wrong.kind=GitUpdateExternalOwner::manual;
  assert(!gitUpdate.releaseExternal(wrong));
  assert(gitUpdate.releaseExternal(second));
  assert(!gitUpdate.busy());
  GitUpdateOwnerToken counter;
  assert(gitUpdate.acquireExternal(GitUpdateExternalOwner::counter,counter));
  assert(gitUpdate.ownsExternal(counter));
  assert(!gitUpdate.ownsExternal(first));
  assert(!gitUpdate.acquireExternal(GitUpdateExternalOwner::manual,first));
  assert(!gitUpdate.acquireExternal(GitUpdateExternalOwner::restore,second));
  auto forged=counter;forged.kind=GitUpdateExternalOwner::manual;
  assert(!gitUpdate.ownsExternal(forged) && !gitUpdate.releaseExternal(forged));
  assert(gitUpdate.releaseExternal(counter));
  assert(!gitUpdate.ownsExternal(counter));
  assert(gitUpdate.requestCheck()==GitUpdateRequest::unavailable);
  assert(!gitUpdate.requestCancel());
  GitUpdateOwnerToken upload;
  assert(gitUpdate.acquireExternal(GitUpdateExternalOwner::manual,upload));
  assert(gitUpdate.beginTransfer(upload,ManualUpdateKind::firmware,0));
  assert(!gitUpdate.releaseExternal(upload));
  assert(!gitUpdate.acquireExternal(GitUpdateExternalOwner::counter,counter));
  assert(gitUpdate.claimTransfer(upload,1));
  assert(gitUpdate.transferResource(upload,true));
  assert(!gitUpdate.claimTransferCleanup(30001).token.generation);
  assert(!gitUpdate.transferContinuing(upload));
  assert(gitUpdate.finishTransfer(upload,30002,true));
  auto cleanup=gitUpdate.claimTransferCleanup(30002);
  assert(cleanup.token.generation==upload.generation && cleanup.resourceOpen);
  assert(!gitUpdate.closeTransfer(upload));
  assert(gitUpdate.transferResource(upload,false));
  assert(gitUpdate.closeTransfer(upload));
  assert(gitUpdate.releaseExternal(upload));
  assert(gitUpdate.acquireExternal(GitUpdateExternalOwner::counter,counter));
  assert(!gitUpdate.retireTransfer(upload));
  assert(!gitUpdate.closeTransfer(upload));
  assert(!gitUpdate.releaseExternal(upload));
  assert(gitUpdate.ownsExternal(counter));
  assert(gitUpdate.releaseExternal(counter));
  assert(gitUpdate.acquireExternal(GitUpdateExternalOwner::manual,upload));
  assert(gitUpdate.beginTransfer(upload,ManualUpdateKind::application,0));
  assert(gitUpdate.claimTransfer(upload,1));
  assert(gitUpdate.transferResource(upload,true));
  assert(gitUpdate.transferResource(upload,false));
  assert(gitUpdate.holdTransferRestart(upload));
  assert(gitUpdate.finishTransfer(upload,1,true));
  assert(!gitUpdate.retireTransfer(upload));
  assert(!gitUpdate.releaseExternal(upload));
  assert(!gitUpdate.acquireExternal(GitUpdateExternalOwner::counter,counter));
  cleanup=gitUpdate.claimTransferCleanup(30001);
  assert(cleanup.token.generation==upload.generation && !cleanup.resourceOpen);
  assert(gitUpdate.closeTransfer(upload));assert(gitUpdate.releaseExternal(upload));
  GitUpdateOwnerToken source,shutdown;
  assert(gitUpdate.acquireExternal(GitUpdateExternalOwner::manual,source));
  assert(gitUpdate.beginTransfer(source,ManualUpdateKind::firmware,0));
  assert(gitUpdate.claimTransfer(source,1));assert(gitUpdate.transferResource(source,true));
  assert(gitUpdate.transferResource(source,false));assert(gitUpdate.holdTransferRestart(source));
  assert(!gitUpdate.acquireRestart(shutdown,1,source)); // Claimed SDK cleanup still excludes shutdown.
  assert(gitUpdate.finishTransfer(source,1,true));
  assert(gitUpdate.acquireRestart(shutdown,2,source));
  assert(shutdown.kind==GitUpdateExternalOwner::restart && shutdown.generation!=source.generation);
  assert(!gitUpdate.claimTransferCleanup(UINT64_MAX).token.generation);
  assert(!gitUpdate.retireTransfer(source) && !gitUpdate.releaseExternal(source));
  assert(gitUpdate.ownsExternal(shutdown));
  assert(!gitUpdate.acquireExternal(GitUpdateExternalOwner::counter,counter));
  assert(gitUpdate.releaseExternal(shutdown)); // Explicit refusal before actual shutdown only.
  assert(!gitUpdate.acquireRestart(shutdown,3,source)); // No fallback to owner-none.
  assert(gitUpdate.acquireRestart(shutdown,3));assert(gitUpdate.releaseExternal(shutdown));
  std::atomic<unsigned> inside{0},completed{0};std::vector<std::thread> threads;
  for(unsigned n=0;n<4;n++) threads.emplace_back([&,n] {
    for(unsigned i=0;i<1000;i++) {
      GitUpdateOwnerToken token;
      while(!gitUpdate.acquireExternal(n%2?GitUpdateExternalOwner::configuration:GitUpdateExternalOwner::manual,token)) std::this_thread::yield();
      assert(inside.fetch_add(1)==0 && gitUpdate.snapshot().busy);
      auto wrong=token;++wrong.generation;assert(!gitUpdate.releaseExternal(wrong));
      assert(inside.fetch_sub(1)==1);assert(gitUpdate.releaseExternal(token));++completed;
    }
  });
  for(auto &thread:threads) thread.join();
  assert(completed==4000 && inside==0 && !gitUpdate.busy());
  std::puts("Configuration owner exclusivity + 4000 mixed threaded claims passed");
}
