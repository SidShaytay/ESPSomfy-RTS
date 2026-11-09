#include "GitUpdate.h"
#include "ManualUpdateLifecycle.h"
#include <cassert>
#include <cstdio>
#include <atomic>
#include <thread>
#include <vector>
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
int main() {
  configurationLease();
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
