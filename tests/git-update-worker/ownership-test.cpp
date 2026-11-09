#include "GitUpdate.h"
#include "ManualUpdateLifecycle.h"
#include <cassert>
#include <atomic>
#include <thread>
#include <vector>
int main() {
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
  for(unsigned n=0;n<4;n++) threads.emplace_back([&] {
    for(unsigned i=0;i<1000;i++) {
      GitUpdateOwnerToken token;
      while(!gitUpdate.acquireExternal(GitUpdateExternalOwner::manual,token)) std::this_thread::yield();
      assert(inside.fetch_add(1)==0 && gitUpdate.snapshot().busy);
      auto wrong=token;++wrong.generation;assert(!gitUpdate.releaseExternal(wrong));
      assert(inside.fetch_sub(1)==1);assert(gitUpdate.releaseExternal(token));++completed;
    }
  });
  for(auto &thread:threads) thread.join();
  assert(completed==4000 && inside==0 && !gitUpdate.busy());
}
