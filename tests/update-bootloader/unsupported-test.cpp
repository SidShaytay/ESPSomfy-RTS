#include "GitUpdateBootloader.h"
#include "GitUpdateAssets.h"
#include <cassert>
int main() {
  assert(gitUpdateBootloaderStatus()==UpdateAttestation::unsupported);
  assert(gitUpdateAssetsStatus()==UpdateAttestation::unsupported);
  assert(!gitUpdateBootloaderMatches() && !gitUpdateAssetsMatch());
  assert(gitUpdateBootloaderIdentity()[0]==0 && gitUpdateBootloaderDigest()[0]==0);
}
