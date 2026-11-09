// Reuse the existing real worker and scripted HTTPS/SDK fixture, not a second
// downloader. The metadata test remains an independently runnable entry point.
#define main metadataWorkerMain
#include "../git-update-worker/worker-test.cpp"
#undef main
static uint64_t installOperation=0;
static unsigned preparations=0,cleanups=0,written=0;
static bool sourceCurrent(uint64_t generation){return generation==installOperation;}
static GitUpdateInstallPreparation prepare(const GitUpdateOwnerToken &token){
  CHECK(!Fake::criticalDepth && token.generation && token.kind==GitUpdateExternalOwner::manual);
  ++preparations;return {true,RadioInstallToken{7}};
}
static bool cleanup(const GitUpdateOwnerToken &,const RadioInstallToken &){CHECK(!Fake::criticalDepth);++cleanups;return true;}
static void imageWritten(const GitUpdateOwnerToken &,const RadioInstallToken &){CHECK(!Fake::criticalDepth);++written;}
static void pump(){gitUpdate.loop();try{worker(nullptr);}catch(const FakeTaskExit &){}gitUpdate.loop();}
int main(){
  fixtures();const std::string image(1000000,'x'); // Correct published digest, invalid ESP image.
  const std::string imageUrl=std::string("https://github.com/")+GitUpdatePolicy::REPOSITORY+"/releases/download/"+tag+"/"+GitUpdatePolicy::FIRMWARE_ASSET;
  manifest["sha256"]=hash(image);release["assets"][0]["digest"]="sha256:"+hash(image);
  Fake::responses[imageUrl].body=image;publish();
  gitUpdate.begin(admit,{sourceCurrent,prepare,cleanup,imageWritten});
  CHECK(gitUpdate.requestCheck()==GitUpdateRequest::accepted);pump();
  const auto candidate=gitUpdate.snapshot().candidate;
  CHECK(gitUpdate.snapshot().canInstall && candidate.generation);
  auto changed=candidate;changed.imageSha[0]=changed.imageSha[0]=='a'?'b':'a';
  uint64_t unchanged=99;CHECK(gitUpdate.requestInstall(changed,unchanged)==GitUpdateRequest::invalidCandidate && unchanged==99);
  CHECK(gitUpdate.requestInstall(candidate,installOperation)==GitUpdateRequest::accepted);
  CHECK(installOperation && preparations==0 && Fake::otaBegins==0);pump();
  const auto result=gitUpdate.snapshot();
  CHECK(result.phase==GitUpdatePhase::error && result.reason==GitUpdateReason::invalidImage);
  CHECK(!preparations && !written && !Fake::otaBegins && !Fake::otaWrites && !Fake::otaActivations);
  CHECK(Fake::activeClients==0 && Fake::activeAllocations==0 && !gitUpdate.busy());
}
