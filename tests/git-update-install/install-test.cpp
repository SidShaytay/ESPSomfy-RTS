// Reuse the existing real worker and scripted HTTPS/SDK fixture, not a second
// downloader. The metadata test remains an independently runnable entry point.
#define main metadataWorkerMain
#include "../git-update-worker/worker-test.cpp"
#undef main
#include <fstream>
#include <iterator>
static uint64_t installOperation=0;
static unsigned preparations=0,cleanups=0,written=0;
static bool sourceFresh=true,paused=false,cleanupReady=true;
static std::string scenario;
static bool sourceCurrent(uint64_t generation){return sourceFresh && generation==installOperation;}
static GitUpdateInstallPreparation prepare(const GitUpdateOwnerToken &token){
  CHECK(!Fake::criticalDepth && token.generation && token.kind==GitUpdateExternalOwner::manual);
  ++preparations;++Fake::prepareCalls;paused=true;
  if(scenario=="sourcePrepare")sourceFresh=false;
  if(scenario=="notifyWrite")Fake::notifyFail=true;
  if(scenario=="allocationWrite")Fake::failAllocationAfter=Fake::allocationCalls;
  return {true,RadioInstallToken{7}};
}
static bool cleanup(const GitUpdateOwnerToken &,const RadioInstallToken &){CHECK(!Fake::criticalDepth && !Fake::inFlash);++cleanups;if(!cleanupReady)return false;paused=false;return true;}
static void imageWritten(const GitUpdateOwnerToken &,const RadioInstallToken &){CHECK(!Fake::criticalDepth);++written;}
static void pump(){gitUpdate.loop();try{worker(nullptr);}catch(const FakeTaskExit &){}gitUpdate.loop();}
int main(int argc,char **argv){
  CHECK(argc==4);scenario=argv[1];Fake::autoLoop=false;fixtures();
  std::ifstream file(argv[2],std::ios::binary);std::string image{std::istreambuf_iterator<char>(file),{}};
  if(scenario=="invalid")image.assign(1000000,'x');
  manifest["size"]=image.size();manifest["app_compat"]=argv[3];release["assets"][0]["size"]=image.size(); // Correct published digest, invalid ESP image.
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
  CHECK(installOperation && preparations==0 && Fake::otaBegins==0);
  if(scenario=="notifyFail")Fake::notifyFail=true;
  if(scenario=="beginFail")Fake::failBegin=true;
  if(scenario=="endFail")Fake::failEnd=true;
  if(scenario=="writeFail" || scenario=="abortFailure")Fake::failWrite=true;
  if(scenario=="abortFailure")Fake::failAbort=true;
  if(scenario=="cleanupBusy")cleanupReady=false;
  if(scenario=="cancelWrite" || scenario=="cleanupBusy")Fake::onWrite=[](){CHECK(gitUpdate.requestCancel());gitUpdate.loop();CHECK(paused && cleanups==0);};
  if(scenario=="cancelBegin")Fake::onBegin=[](){CHECK(gitUpdate.requestCancel());gitUpdate.loop();CHECK(paused && cleanups==0);};
  if(scenario=="cancelEnd")Fake::onEnd=[](){CHECK(gitUpdate.requestCancel());gitUpdate.loop();CHECK(paused && cleanups==0);};
  if(scenario=="expireWrite")Fake::onWrite=[](){Fake::now+=31000;gitUpdate.loop();CHECK(paused && cleanups==0);};
  if(scenario=="expireEnd")Fake::onEnd=[](){Fake::now+=31000;};
  if(scenario=="sourceWrite")Fake::onWrite=[](){sourceFresh=false;gitUpdate.loop();CHECK(paused && cleanups==0);};
  if(scenario=="cancelStage")Fake::onRead=[](){CHECK(gitUpdate.requestCancel());gitUpdate.loop();CHECK(!preparations && !cleanups);};
  if(scenario=="refuseAdmission")allowed=false;
  if(scenario=="shortImage")Fake::responses[imageUrl].body=image.substr(0,image.size()-1);
  for(unsigned i=0;i<5;++i)pump();
  const auto result=gitUpdate.snapshot();
  if(scenario=="valid"){
    CHECK(preparations==1 && written==1 && paused && gitUpdate.busy());
    CHECK(Fake::otaBegins==1 && Fake::otaEnds==1 && Fake::written.size()==image.size());
    CHECK(std::string(reinterpret_cast<const char*>(Fake::written.data()),Fake::written.size())==image);
    CHECK(Fake::otaActivations==0 && Fake::otaAddress==nextPartition.address);return 0;
  }
  if(scenario=="abortFailure"){
    CHECK(paused && gitUpdate.busy() && cleanups==0 && !written && Fake::otaAborts==1);
    CHECK(!gitUpdate.snapshot().cancellable && !Fake::otaActivations && Fake::activeAllocations>0);
    GitUpdateOwnerToken other;CHECK(!gitUpdate.acquireExternal(GitUpdateExternalOwner::configuration,other));
    CHECK(!gitUpdate.claimTransferCleanup(Fake::now).token.generation);return 0;
  }
  if(scenario=="cleanupBusy"){
    CHECK(paused && gitUpdate.busy() && cleanups>0 && !written);cleanupReady=true;for(unsigned i=0;i<3;++i)pump();
  }
  if(scenario!="invalid"){
    CHECK(!paused && !gitUpdate.busy() && !written && !Fake::otaActivations);
    CHECK(gitUpdate.snapshot().phase==GitUpdatePhase::error || gitUpdate.snapshot().phase==GitUpdatePhase::cancelled);
    CHECK(!Fake::activeClients && !Fake::activeAllocations);return 0;
  }
  CHECK(result.phase==GitUpdatePhase::error && result.reason==GitUpdateReason::invalidImage);
  CHECK(!preparations && !written && !Fake::otaBegins && !Fake::otaWrites && !Fake::otaActivations);
  CHECK(Fake::activeClients==0 && Fake::activeAllocations==0 && !gitUpdate.busy());
}
