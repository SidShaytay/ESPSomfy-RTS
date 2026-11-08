#include "GitUpdate.h"
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
  assert(gitUpdate.requestCheck()==GitUpdateRequest::unavailable);
  assert(!gitUpdate.requestCancel());
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
