// Exercise the production worker/downloader/verifier; no socket or flash boundary.
#define main metadataWorkerMain
#include "worker-test.cpp"
#undef main
#include <fstream>
#include <iterator>
static unsigned hookCalls=0;
static bool current(uint64_t){++hookCalls;return true;}
static GitUpdateInstallPreparation prepareInstall(const GitUpdateOwnerToken &){++hookCalls;return {};}
static bool cleanupInstall(const GitUpdateOwnerToken &,const RadioInstallToken &){++hookCalls;return true;}
static void writtenInstall(const GitUpdateOwnerToken &,const RadioInstallToken &){++hookCalls;}
int main(int argc,char **argv){
  CHECK(argc==4);const std::string scenario=argv[1];fixtures();
  std::ifstream stream(argv[2],std::ios::binary);std::string image{std::istreambuf_iterator<char>(stream),{}};
  if(scenario=="same" || scenario=="older"){
    tag=scenario=="same"?"v4.0.2":"v4.0.1";
    // The runner supplies a structurally valid image with the same exact tag.
    manifest["version"]=tag;release["tag_name"]=tag;
    release["html_url"]=std::string("https://github.com/")+GitUpdatePolicy::REPOSITORY+"/releases/tag/"+tag;
  }
  manifest["size"]=image.size();manifest["app_compat"]=argv[3];manifest["sha256"]=hash(image);
  release["assets"][0]["size"]=image.size();release["assets"][0]["digest"]="sha256:"+hash(image);publish();
  const auto imageUrl=std::string("https://github.com/")+GitUpdatePolicy::REPOSITORY+"/releases/download/"+tag+"/"+GitUpdatePolicy::FIRMWARE_ASSET;
  Fake::responses[imageUrl].body=image;
  gitUpdate.begin(admit,{current,prepareInstall,cleanupInstall,writtenInstall});
  CHECK(gitUpdate.requestAuditStage({})==GitUpdateRequest::invalidCandidate);
  CHECK(gitUpdate.requestCheck()==GitUpdateRequest::accepted);const auto checked=run();
  CHECK(checked.compatible && checked.candidate.generation);
  const auto candidate=checked.candidate;
  if(scenario=="same" || scenario=="older")CHECK(!checked.canPrepare && !checked.canInstall);
  auto invalid=candidate;++invalid.size;CHECK(gitUpdate.requestAuditStage(invalid)==GitUpdateRequest::invalidCandidate);
  if(scenario=="changedsource"){manifest["source_sha"]=std::string(40,'b');publish();}
  if(scenario=="changedmanifest"){manifest["note"]="changed exact metadata";publish();}
  if(scenario=="transport")Fake::responses[imageUrl].openError=ESP_ERR_HTTP_CONNECT;
  if(scenario=="invalidimage")Fake::responses[imageUrl].body[0]=0;
  if(scenario=="cancel")Fake::onRead=[&](){if(gitUpdate.snapshot().transport.download==GitUpdateDownload::image)CHECK(gitUpdate.requestCancel());};
  if(scenario=="cancelverify")Fake::onYield=[](){if(gitUpdate.snapshot().phase==GitUpdatePhase::verifying)CHECK(gitUpdate.requestCancel());};
  if(scenario=="admission")allowed=false;
  CHECK(gitUpdate.requestAuditStage(candidate)==GitUpdateRequest::accepted);
  CHECK(owner==Owner::prepare && admissionPending==5 && !installLease && !installImage);
  CHECK(gitUpdate.requestCheck()==GitUpdateRequest::busy);
  if(scenario=="notifyfail")Fake::notifyFail=true;
  if(scenario=="idletimeout")Fake::notificationTimeouts=1;
  const auto result=run();
  CHECK(!installLease && !installImage && !admissionPending && !hookCalls);
  CHECK(!Fake::otaBegins && !Fake::otaWrites && !Fake::otaEnds && !Fake::otaActivations && !Fake::prepareCalls);
  CHECK(Fake::activeClients==0 && Fake::activeAllocations==0);
  const bool downloaded=std::find(Fake::urls.begin(),Fake::urls.end(),imageUrl)!=Fake::urls.end();
  if(scenario=="valid" || scenario=="same" || scenario=="older" || scenario=="idletimeout"){
    CHECK(result.phase==GitUpdatePhase::prepared && result.reason==GitUpdateReason::none && downloaded);
    CHECK(result.transport.download==GitUpdateDownload::image && result.transport.stage==GitUpdateTransportStage::complete);
    CHECK(result.candidate.generation==candidate.generation && result.progress==100 && !result.cancellable);
    if(scenario=="idletimeout")CHECK(Fake::observedNotificationTimeouts==1);
    if(scenario=="same" || scenario=="older"){
      CHECK(!result.canPrepare && !result.canInstall);
      uint64_t untouched=1;CHECK(gitUpdate.requestInstall(candidate,untouched)==GitUpdateRequest::invalidCandidate && untouched==1);
    }
  }else if(scenario=="changedsource" || scenario=="changedmanifest"){
    CHECK(result.phase==GitUpdatePhase::changed && result.reason==GitUpdateReason::candidateChanged && !downloaded);
  }else{
    CHECK(result.phase==GitUpdatePhase::cancelled || result.phase==GitUpdatePhase::error);
    if(scenario=="cancel" || scenario=="cancelverify")CHECK(result.reason==GitUpdateReason::cancelled);
    if(scenario=="invalidimage")CHECK(result.reason==GitUpdateReason::invalidImage);
    if(scenario=="transport")CHECK(result.reason==GitUpdateReason::transport);
    if(scenario=="admission" || scenario=="notifyfail")CHECK(result.reason==GitUpdateReason::admission && !downloaded);
  }
  CHECK(admissions>=2);std::cout<<scenario<<": audit stage full-image/no-install passed\n";
}
