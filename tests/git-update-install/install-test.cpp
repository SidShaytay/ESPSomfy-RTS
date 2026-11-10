// Reuse the existing real worker and scripted HTTPS/SDK fixture, not a second
// downloader. The metadata test remains an independently runnable entry point.
#define main metadataWorkerMain
#include "../git-update-worker/worker-test.cpp"
#undef main
#include <fstream>
#include <iterator>
#ifdef GIT_UPDATE_CONTROLLER_TEST
#include "../git-update-controller/controller-fixture.h"
#else
static uint64_t installOperation=0;
static GitUpdateOwnerToken preparedToken;
static unsigned preparations=0,cleanups=0,written=0;
static uint32_t largestRetryWait=0;
static bool sourceFresh=true,paused=false,cleanupReady=true;
static std::string scenario;
static bool sourceCurrent(uint64_t generation){return sourceFresh && generation==installOperation;}
static GitUpdateInstallPreparation prepare(const GitUpdateOwnerToken &token){
  CHECK(!Fake::criticalDepth && token.generation && token.kind==GitUpdateExternalOwner::manual);
  ++preparations;++Fake::prepareCalls;paused=true;
  preparedToken=token;
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
  uint64_t checkOperation=0;CHECK(gitUpdate.requestCheck(checkOperation)==GitUpdateRequest::accepted && checkOperation);
  CHECK(gitUpdate.snapshot().operation==checkOperation);pump();
  const auto candidate=gitUpdate.snapshot().candidate;
  CHECK(gitUpdate.snapshot().canInstall && candidate.generation);
  auto changed=candidate;changed.imageSha[0]=changed.imageSha[0]=='a'?'b':'a';
  uint64_t unchanged=99;CHECK(gitUpdate.requestInstall(changed,unchanged)==GitUpdateRequest::invalidCandidate && unchanged==99);
  CHECK(gitUpdate.requestInstall(candidate,installOperation)==GitUpdateRequest::accepted);
  CHECK(installOperation!=checkOperation && installOperation!=candidate.generation);
  CHECK(!gitUpdate.requestCancel(checkOperation)); // A completed screen cannot cancel the new install.
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
  if(scenario.rfind("rate",0)==0){
    auto &r=Fake::responses[apiUrl];r.temporaryStatusFailures=1;r.temporaryStatusCode=429;
    if(scenario=="rateAfter")r.retryAfter="2";
    if(scenario=="rateReset"){r.temporaryStatusCode=403;r.rateRemaining="0";r.rateReset=std::to_string(uint64_t(Fake::wallClock)+90);}
    if(scenario=="rateDefault"){r.temporaryStatusCode=429;r.temporaryStatusFailures=2;}
    if(scenario=="rateOrdinary403")r.temporaryStatusCode=403;
    if(scenario=="rateMalformed")r.retryAfter="not-numeric";
    if(scenario=="rateBeyond")r.retryAfter="999999";
    Fake::onYield=[](){const auto wait=gitUpdate.snapshot().retryDelayMs;largestRetryWait=std::max(largestRetryWait,wait);if(scenario=="rateBeyond" && wait)Fake::now+=gitUpdateInstallDurationMs;};
  }
  if(scenario=="retryApi")Fake::responses[apiUrl].temporaryOpenFailures=1;
  if(scenario=="retryManifest")Fake::responses[std::string("https://github.com/")+GitUpdatePolicy::REPOSITORY+"/releases/download/"+tag+"/"+GitUpdatePolicy::MANIFEST_ASSET].temporaryStatusFailures=1;
  if(scenario=="retryImage")Fake::responses[imageUrl].temporaryTruncations=1;
  if(scenario=="retryHeaderEOF")Fake::responses[apiUrl].temporaryHeaderFailures=1;
  if(scenario=="retryBadHeaders"){Fake::responses[apiUrl].length=-1;Fake::responses[apiUrl].encoding="gzip";}
  if(scenario=="retryCert"){Fake::bundleResult=-77;Fake::bundleFlags=0x4000;}
  if(scenario=="retryMalformed")Fake::responses[apiUrl].body="{malformed";
  if(scenario=="retryBounds")Fake::responses[imageUrl].length=0;
  if(scenario=="retryRecordTLS"){Fake::responses[imageUrl].truncateAt=10;Fake::responses[imageUrl].tlsError=-0x7180;Fake::onYield=[](){if(gitUpdate.snapshot().retryDelayMs)Fake::now+=gitUpdateInstallDurationMs;};}
  if(scenario=="retryPermanentTLS"){Fake::responses[apiUrl].openError=ESP_ERR_HTTP_CONNECT;Fake::responses[apiUrl].tlsError=-0x2700;}
  if(scenario=="retryDeadline" || scenario=="retryCancel"){
    Fake::responses[apiUrl].temporaryStatusFailures=10000;
    Fake::onYield=[](){if(!gitUpdate.snapshot().retryDelayMs)return;if(scenario=="retryCancel")CHECK(gitUpdate.requestCancel());else Fake::now+=1000000;};
  }
  for(unsigned i=0;i<5;++i)pump();
  const auto result=gitUpdate.snapshot();
  if(scenario=="valid" || scenario=="retryApi" || scenario=="retryManifest" || scenario=="retryImage" || scenario=="retryHeaderEOF" || scenario=="rateAfter" || scenario=="rateReset" || scenario=="rateDefault" || scenario.compare(0,8,"activate")==0){
    CHECK(preparations==1 && written==1 && paused && gitUpdate.busy());
    CHECK(Fake::otaBegins==1 && Fake::otaEnds==1 && Fake::written.size()==image.size());
    CHECK(std::string(reinterpret_cast<const char*>(Fake::written.data()),Fake::written.size())==image);
    CHECK(Fake::otaActivations==0 && Fake::otaAddress==nextPartition.address);
    if(scenario=="rateAfter" || scenario=="rateReset" || scenario=="rateDefault"){CHECK(result.retryCount==(scenario=="rateDefault"?2:1) && Fake::otaBegins==1);CHECK(largestRetryWait>=(scenario=="rateAfter"?2000u:scenario=="rateReset"?90000u:120000u));return 0;}
    if(scenario=="retryApi" || scenario=="retryManifest" || scenario=="retryImage" || scenario=="retryHeaderEOF"){CHECK(result.retryCount==1 && !result.retryDelayMs && Fake::otaBegins==1 && Fake::otaEnds==1);return 0;}
    if(scenario=="valid")return 0;
    CHECK(!gitUpdate.activateInstall(installOperation) && !Fake::otaActivations);
    GitUpdateOwnerToken restart;CHECK(gitUpdate.acquireRestart(restart,Fake::now,preparedToken));
    if(scenario=="activateFailure")Fake::failActivate=true;
    if(scenario=="activateWrongSlot")nextPartition.address+=0x10000;
    if(scenario=="activateWrongSubtype")++nextPartition.subtype;
    if(scenario=="activateBadReadback")Fake::wrongBootReadback=true;
    const bool selected=gitUpdate.activateInstall(installOperation);
    CHECK(selected==(scenario=="activateOkay"));
    CHECK(gitUpdate.busy() && paused && !cleanups);
    CHECK(!gitUpdate.activateInstall(installOperation));Fake::now+=200000;
    for(unsigned i=0;i<3;++i)gitUpdate.loop();
    CHECK(paused && gitUpdate.busy() && !cleanups);
    CHECK(Fake::otaActivations==((scenario=="activateWrongSlot" || scenario=="activateWrongSubtype")?0u:1u));return 0;
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
  if(scenario.rfind("rate",0)==0){CHECK(!Fake::otaBegins);if(scenario=="rateBeyond")CHECK(result.reason==GitUpdateReason::timeout && result.retryCount==1);else CHECK(!result.retryCount);}
  if(scenario.rfind("retry",0)==0){
    CHECK(!Fake::otaBegins && !Fake::otaActivations);
    if(scenario=="retryDeadline")CHECK(result.reason==GitUpdateReason::timeout && result.retryCount>0 && result.retryCount<3);
    else if(scenario=="retryCancel")CHECK(result.reason==GitUpdateReason::cancelled && result.retryCount==1);
    else CHECK(result.retryCount==0);
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
#endif
