#include "../../src/GitUpdate.cpp"
#include <iostream>
static unsigned assertions=0,admissions=0;
#define CHECK(c) do { ++assertions;if(!(c)) { std::cerr<<"FAIL "<<__LINE__<<": " #c "\n";std::exit(1); } } while(0)
static bool allowed=true;
static GitUpdateReadAdmission admit() { ++admissions;return {allowed}; }
static std::string hash(const std::string &data) {
  uint8_t digest[32];CHECK(mbedtls_sha256(reinterpret_cast<const uint8_t *>(data.data()),data.size(),digest,0)==0);
  char hex[65];for(size_t i=0;i<32;i++) snprintf(hex+i*2,3,"%02x",digest[i]);return hex;
}
static std::string stringify(const JsonDocument &doc) { std::string out;serializeJson(doc,out);return out; }
static std::string apiUrl=std::string("https://api.github.com/repos/")+GitUpdatePolicy::REPOSITORY+"/releases/latest";
static std::string tag="v4.0.3";
static JsonDocument release,manifest;
static void publish(std::string body={}) {
  if(body.empty()) body=stringify(manifest);
  release["assets"][1]["size"]=body.size();release["assets"][1]["digest"]="sha256:"+hash(body);
  Fake::responses[apiUrl].body=stringify(release);Fake::responses[apiUrl].requireJsonAccept=true;
  Fake::responses[std::string("https://github.com/")+GitUpdatePolicy::REPOSITORY+"/releases/download/"+tag+"/"+GitUpdatePolicy::MANIFEST_ASSET].body=body;
}
static void fixtures() {
  using namespace GitUpdatePolicy;
  manifest["schema"]=3;manifest["assets_mode"]="bundled";manifest["board"]=BOARD;manifest["layout"]=LAYOUT;
  manifest["compatible_boards"][0]=BOARD;manifest["compatible_boards"][1]=GitUpdateProfile::plusBoard;
  manifest["features"]=15;manifest["flash_bytes"]=FLASH_BYTES;manifest["psram_bytes"]=PSRAM_BYTES;
  manifest["size"]=1000000;manifest["app_compat"]=ESPSOMFY_APP_COMPAT;manifest["version"]=tag;
  manifest["source_sha"]=std::string(40,'a');manifest["asset"]=FIRMWARE_ASSET;manifest["sha256"]=std::string(64,'a');
  manifest["bootloader_sha256"]=gitUpdateBootloaderDigest();manifest["channel"]="stable";
  release["id"]=42;release["url"]=std::string("https://api.github.com/repos/")+REPOSITORY+"/releases/42";
  release["html_url"]=std::string("https://github.com/")+REPOSITORY+"/releases/tag/"+tag;
  release["published_at"]="2026-11-08T12:00:00Z";release["tag_name"]=tag;release["draft"]=false;release["prerelease"]=false;
  release["assets"][0]["name"]=FIRMWARE_ASSET;release["assets"][0]["size"]=1000000;release["assets"][0]["digest"]="sha256:"+std::string(64,'a');
  release["assets"][1]["name"]=MANIFEST_ASSET;publish();
}
static GitUpdateSnapshot run() {
  gitUpdate.loop();
  try { worker(nullptr); } catch(const FakeTaskExit &) {}
  CHECK(!gitUpdate.busy());CHECK(Fake::activeClients==0 && Fake::activeAllocations==0);
  CHECK(Fake::otaBegins==0 && Fake::otaWrites==0 && Fake::otaActivations==0 && Fake::prepareCalls==0);
  return gitUpdate.snapshot();
}
int main(int argc,char **argv) {
  CHECK(argc==2);const std::string mode=argv[1];fixtures();
  if(mode=="createfail") Fake::createFail=true;
  if(mode=="memorynotready") Fake::memoryReady=false;
  if(mode=="entropyfail") Fake::randomWord=0;
  gitUpdate.begin(admit);
  if(mode=="entropyfail") {
    CHECK(gitUpdate.requestCheck()==GitUpdateRequest::unavailable);
    CHECK(gitUpdate.snapshot().reason==GitUpdateReason::freshness);
    CHECK(Fake::randomCalls==8 && Fake::createdTasks==0 && Fake::urls.empty());return 0;
  }
  CHECK(mode=="dev" || mode=="memorynotready" || mode=="createfail" || candidateGeneration!=0);
  if(mode=="dev" || mode=="memorynotready" || mode=="createfail") {
    CHECK(gitUpdate.requestCheck()==GitUpdateRequest::unavailable);
    CHECK(gitUpdate.snapshot().reason==(mode=="dev" ? GitUpdateReason::development : GitUpdateReason::memory));
    CHECK(Fake::urls.empty());return 0;
  }
  gitUpdate.begin(admit);CHECK(Fake::createdTasks==1);
  CHECK(Fake::urls.empty() && Fake::attestationCalls==0);
  if(mode=="expiredcert") Fake::expiredCertificate=true;
  if(mode=="futurecert") Fake::futureCertificate=true;
  if(mode=="expiredintermediate") Fake::expiredIntermediate=true;
  if(mode=="missingverify") Fake::missingVerify=true;
  if(mode=="bundlefailure") {Fake::bundleResult=-77;Fake::bundleFlags=0x4000;}
  if(mode=="notifyfail") Fake::notifyFail=true;
  if(mode=="generationlarge") candidateGeneration=9007199254740992ULL;
  if(mode=="generationmax") candidateGeneration=UINT64_MAX;
  if(mode=="sameversion" || mode=="downgrade") {
    tag=mode=="sameversion" ? "v4.0.2" : "v4.0.1";manifest["version"]=tag;release["tag_name"]=tag;
    release["html_url"]=std::string("https://github.com/")+GitUpdatePolicy::REPOSITORY+"/releases/tag/"+tag;publish();
  }
  if(mode=="allocationpartial") Fake::failAllocationAfter=3;
  if(mode=="duplicateassets") {release["assets"].add<JsonObject>().set(release["assets"][0]);publish();}
  if(mode=="missingdigest") {release["assets"][0]["digest"]=nullptr;publish();}
  if(mode=="boolfeatures") {manifest["features"]=true;publish();}
  if(mode=="floatnumeric") {
    auto body=stringify(manifest);const auto at=body.find("\"size\":1000000");CHECK(at!=std::string::npos);
    body.replace(at,strlen("\"size\":1000000"),"\"size\":1000000.0");publish(body);
  }
  if(mode=="notpublished") {release["published_at"]=nullptr;publish();}
  if(mode=="metadata_budget") {
    release["assets"].clear();for(size_t i=0;i<4000;i++) release["assets"][i]["name"]="x";
    Fake::responses[apiUrl].body=stringify(release);
  }
  if(mode=="admission") allowed=false;
  if(mode=="lowmem") Fake::internalFree=47*1024;
  if(mode=="fragmented") Fake::internalLargest=15*1024;
  if(mode=="clock") Fake::wallClock=0;
  if(mode=="boot") Fake::bootloaderMatches=false;
  if(mode=="assets") Fake::fsCompatible=false;
  if(mode=="layout") Fake::extraAppPartition=true;
  if(mode=="wrongrepo") {release["html_url"]="https://github.com/evil/repo/releases/tag/v4.0.3";publish();}
  if(mode=="draft") {release["draft"]=true;publish();}
  if(mode=="prerelease") {release["prerelease"]=true;publish();}
  if(mode=="nul") {release["tag_name"]=std::string("v4.0.3\0evil",11);publish();}
  if(mode=="types") {manifest["size"]="1000000";publish();}
  if(mode=="features") {manifest["features"]=263;publish();}
  if(mode=="boards") {manifest["compatible_boards"][1]="esp32s3";publish();}
  if(mode=="newweb") {manifest["app_compat"]="sha256:"+std::string(64,'d');publish();}
  if(mode=="wrongmode") {manifest["assets_mode"]="filesystem";publish();}
  if(mode=="missingmode") {manifest.remove("assets_mode");publish();}
  if(mode=="oldschema") {manifest["schema"]=2;publish();}
  if(mode=="huge") {release["assets"][0]["size"]=uint64_t(0x100000000);publish();}
  if(mode=="digest") {release["assets"][1]["digest"]="sha256:"+std::string(64,'d');Fake::responses[apiUrl].body=stringify(release);}
  if(mode=="chunkapi") {Fake::responses[apiUrl].chunked=true;Fake::responses[apiUrl].length=0;}
  if(mode=="chunkmanifest") {
    auto &r=Fake::responses[std::string("https://github.com/")+GitUpdatePolicy::REPOSITORY+"/releases/download/"+tag+"/"+GitUpdatePolicy::MANIFEST_ASSET];r.chunked=true;r.length=0;
  }
  if(mode=="truncated") Fake::responses[apiUrl].truncateAt=10;
  if(mode=="unclean") Fake::responses[apiUrl].clean=false;
  if(mode=="encoding") Fake::responses[apiUrl].encoding="gzip";
  if(mode=="redirect" || mode=="redirectbad" || mode=="redirectloop" || mode=="redirectrepo" || mode=="duplicatelocation" || mode=="longlocation") {
    const auto original=Fake::responses[apiUrl];
    auto &r=Fake::responses[apiUrl];r.status=302;
    r.location=mode=="redirectbad" ? "https://evil.example/a?secret=sentinel" : (mode=="redirectloop" ? apiUrl : apiUrl+"?redirect=1");
    if(mode=="redirect" || mode=="duplicatelocation") Fake::responses[r.location]=original;
    if(mode=="duplicatelocation") r.duplicateLocation=true;
    if(mode=="longlocation") r.location="https://github.com/"+std::string(2048,'x');
    if(mode=="redirectrepo") {
      r.location="https://api.github.com/repos/other/repo/releases/latest";
      release["url"]="https://api.github.com/repos/other/repo/releases/42";
      release["html_url"]="https://github.com/other/repo/releases/tag/v4.0.3";
      Fake::responses[r.location]=original;Fake::responses[r.location].body=stringify(release);
    }
  }
  if(mode=="cancel") Fake::onRead=[] {CHECK(gitUpdate.requestCancel());};
  if(mode=="cancelattest") Fake::onYield=[] {CHECK(gitUpdate.requestCancel());};
  if(mode=="wrapclock") {Fake::now=UINT32_MAX-1000;Fake::onRead=[] {Fake::now+=2000;};}
  if(mode=="timeout") Fake::onRead=[] {Fake::now+=180001;};
  if(mode=="allocation") Fake::allocationFail=true;
  GitUpdateOwnerToken external;
  CHECK(gitUpdate.acquireExternal(GitUpdateExternalOwner::manual,external));
  CHECK(gitUpdate.requestCheck()==GitUpdateRequest::busy);CHECK(gitUpdate.releaseExternal(external));
  CHECK(gitUpdate.requestCheck()==GitUpdateRequest::accepted);
  CHECK(Fake::urls.empty() && Fake::attestationCalls==0 && !Fake::queued);
  auto result=run();
  const bool good=mode=="newweb" || mode=="valid" || mode=="redirect" || mode=="chunkapi" || mode=="prepare" || mode=="generation" || mode=="reboot" || mode=="generationlarge" || mode=="wrapclock" || mode=="externalinvalidates" || mode=="failedrefresh" || mode=="sameversion" || mode=="downgrade" || mode.rfind("changed",0)==0;
  CHECK(result.compatible==good);CHECK(!result.canInstall);
  if(!good) {
    CHECK(result.candidate.generation==0);CHECK(result.phase==GitUpdatePhase::error || result.phase==GitUpdatePhase::cancelled);
    if(mode=="cancel" || mode=="cancelattest") CHECK(result.reason==GitUpdateReason::cancelled);
    if(mode=="wrapclock") {Fake::now=UINT32_MAX-1000;Fake::onRead=[] {Fake::now+=2000;};}
  if(mode=="timeout") CHECK(result.reason==GitUpdateReason::timeout);
    if(mode=="wrongrepo") CHECK(result.reason==GitUpdateReason::repository);
    if(mode=="expiredcert" || mode=="futurecert" || mode=="expiredintermediate") CHECK(result.reason==GitUpdateReason::certificateTime);
    if(mode=="missingverify" || mode=="bundlefailure") CHECK(result.reason==GitUpdateReason::transport);
    if(mode=="redirectloop") CHECK(Fake::urls.size()==4);
    CHECK(std::string(gitUpdateReason(result.reason)).find("sentinel")==std::string::npos);
  } else {
    const bool newer=mode!="sameversion" && mode!="downgrade";
    CHECK(result.phase==(newer ? GitUpdatePhase::available : GitUpdatePhase::current));
    CHECK(result.canPrepare==newer && result.candidate.generation);
    CHECK(std::string(result.candidate.sourceSha)==std::string(40,'a'));
    const auto admitted=result.candidate;
    auto invalid=admitted;invalid.generation=0;
    CHECK(gitUpdate.requestPrepare(invalid)==GitUpdateRequest::invalidCandidate);
    if(!newer) CHECK(gitUpdate.requestPrepare(admitted)==GitUpdateRequest::invalidCandidate);
    if(mode=="externalinvalidates") {
      GitUpdateOwnerToken token;CHECK(gitUpdate.acquireExternal(GitUpdateExternalOwner::restore,token));
      CHECK(gitUpdate.releaseExternal(token));
      CHECK(gitUpdate.requestPrepare(admitted)==GitUpdateRequest::invalidCandidate);
    }
    if(mode=="reboot") {
      // A reboot has no retained candidate or worker; keep the browser token.
      workerTask.store(nullptr);candidateGeneration=0;view={};Fake::randomWord=0x76543210;
      gitUpdate.begin(admit);CHECK(gitUpdate.requestCheck()==GitUpdateRequest::accepted);
      const auto restarted=run();CHECK(restarted.candidate.generation!=admitted.generation);
      CHECK(gitUpdate.requestPrepare(admitted)==GitUpdateRequest::invalidCandidate);
    } else if(mode=="generation") {
      CHECK(gitUpdate.requestCheck()==GitUpdateRequest::accepted);auto second=run();
      CHECK(second.candidate.generation!=admitted.generation);
      CHECK(gitUpdate.requestPrepare(admitted)==GitUpdateRequest::invalidCandidate);
    } else if(mode=="prepare" || mode=="failedrefresh" || mode.rfind("changed",0)==0) {
      if(mode=="changedsource") manifest["source_sha"]=std::string(40,'b');
      if(mode=="changedimage") {manifest["sha256"]=std::string(64,'b');release["assets"][0]["digest"]="sha256:"+std::string(64,'b');}
      if(mode=="changedmanifest") manifest["note"]="new bytes, same firmware identity";
      if(mode=="changedversion") {
        tag="v4.0.4";manifest["version"]=tag;release["tag_name"]=tag;
        release["html_url"]=std::string("https://github.com/")+GitUpdatePolicy::REPOSITORY+"/releases/tag/"+tag;
      }
      publish();if(mode=="failedrefresh") Fake::responses[apiUrl].status=500;
      CHECK(gitUpdate.requestPrepare(admitted)==GitUpdateRequest::accepted);result=run();
      if(mode=="prepare") CHECK(result.phase==GitUpdatePhase::prepared && result.candidate.generation==admitted.generation);
      else if(mode=="failedrefresh") {CHECK(!result.compatible && !result.candidate.generation);CHECK(gitUpdate.requestPrepare(admitted)==GitUpdateRequest::invalidCandidate);}
      else {CHECK(result.phase==GitUpdatePhase::changed);CHECK(result.reason==GitUpdateReason::candidateChanged);CHECK(result.candidate.generation!=admitted.generation);}
    }
  }
  const auto urls=Fake::urls.size();const auto attest=Fake::attestationCalls;
  auto snapshot=gitUpdate.snapshot();JsonDocument json;gitUpdate.status(json.to<JsonObject>());
  CHECK(json["candidateGeneration"].is<const char *>());CHECK(json["candidateGeneration"].as<std::string>()==std::to_string(snapshot.candidate.generation));
  CHECK(Fake::urls.size()==urls && Fake::attestationCalls==attest && !snapshot.canInstall);
  CHECK(Fake::activeClients==0 && Fake::activeAllocations==0);
  std::cout<<mode<<": "<<assertions<<" assertions passed\n";
}
