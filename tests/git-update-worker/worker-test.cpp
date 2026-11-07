#include "../../src/GitUpdate.cpp"
#include <iostream>

rebootDelay_t rebootDelay;
static unsigned assertions=0;
#define CHECK(condition) do { assertions++;if(!(condition)) { std::cerr<<"FAIL "<<__LINE__<<": " #condition "\n";std::exit(1); } } while(0)
static std::string hash(const std::string &data) {
  unsigned char digest[32];mbedtls_sha256(reinterpret_cast<const unsigned char *>(data.data()),data.size(),digest,0);
  char result[65];for(size_t i=0;i<32;i++) snprintf(result+i*2,3,"%02x",digest[i]);return result;
}
static std::string stringify(const JsonDocument &json) { std::string result;serializeJson(json,result);return result; }
static std::string tag="v4.0.3";
static std::string apiUrl=std::string("https://api.github.com/repos/")+GitUpdatePolicy::REPOSITORY+"/releases/latest";
static std::string manifestUrl=assetUrl(tag,GitUpdatePolicy::MANIFEST_ASSET);
static std::string firmwareUrl=assetUrl(tag,GitUpdatePolicy::FIRMWARE_ASSET);
static std::string firmwareBody;
static bool prepare() {
  Fake::prepareCalls++;
  if(Fake::cancelledDuringPrepare) CHECK(gitUpdate.requestCancel());
  return true;
}
static void fixtures() {
  using namespace GitUpdatePolicy;
  Manifest m;m.schema=1;m.board=BOARD;m.layout=LAYOUT;m.flashBytes=FLASH_BYTES;m.psramBytes=PSRAM_BYTES;
  m.appCompat=ESPSOMFY_APP_COMPAT;m.version=tag;m.sourceSha=std::string(40,'a');m.asset=FIRMWARE_ASSET;m.channel="stable";m.bootloaderSha=gitUpdateBootloaderDigest();
  firmwareBody=std::string(16000,'x');
  esp_image_header_t header{};header.magic=ESP_IMAGE_HEADER_MAGIC;header.chip_id=GitUpdateProfile::chipId;
  std::memcpy(firmwareBody.data(),&header,sizeof(header));
  const std::string marker=identity(m);std::memcpy(firmwareBody.data()+100,marker.c_str(),marker.size()+1);
  m.size=firmwareBody.size();m.sha256=hash(firmwareBody);
  JsonDocument manifest;
  manifest["schema"]=1;manifest["board"]=m.board;manifest["layout"]=m.layout;
  manifest["flash_bytes"]=m.flashBytes;manifest["psram_bytes"]=m.psramBytes;manifest["size"]=m.size;
  manifest["app_compat"]=m.appCompat;manifest["version"]=m.version;manifest["source_sha"]=m.sourceSha;
  manifest["asset"]=m.asset;manifest["sha256"]=m.sha256;manifest["channel"]=m.channel;manifest["bootloader_sha256"]=m.bootloaderSha;
  const std::string manifestBody=stringify(manifest);
  JsonDocument release;
  release["tag_name"]=tag;release["draft"]=false;release["prerelease"]=false;
  JsonObject binary=release["assets"].to<JsonArray>().add<JsonObject>();
  binary["name"]=FIRMWARE_ASSET;binary["size"]=m.size;binary["digest"]="sha256:"+m.sha256;
  JsonObject metadata=release["assets"].add<JsonObject>();
  metadata["name"]=MANIFEST_ASSET;metadata["size"]=manifestBody.size();metadata["digest"]="sha256:"+hash(manifestBody);
  Fake::responses[apiUrl].body=stringify(release);
  Fake::responses[apiUrl].requireJsonAccept=true;
  Fake::responses[manifestUrl].body=manifestBody;
  Fake::responses[firmwareUrl].body=firmwareBody;
}
static void reset() {
  owner.store(0);cancelled.store(false);preparation.store(0);operationDone.store(false);quiesced=false;
  snapshot=Snapshot();task=nullptr;prepareCallback=nullptr;candidate=GitUpdatePolicy::Manifest();firmwareDigest.clear();
  Fake::now=1000;Fake::wallClock=1760000000;Fake::queued=false;Fake::notified=0;
  Fake::memoryReady=true;Fake::fsCompatible=true;Fake::bootloaderMatches=true;
  Fake::failBegin=false;Fake::failWrite=false;Fake::failEnd=false;Fake::failActivate=false;Fake::allocationFail=false;
  Fake::cancelledDuringPrepare=false;Fake::cancelDuringWrite=false;Fake::cancelAcceptedDuringWrite=false;
  Fake::extraAppPartition=false;Fake::extraOtaPartition=false;
  Fake::prepareCalls=0;Fake::otaBegins=0;Fake::otaWrites=0;Fake::otaEnds=0;Fake::otaActivations=0;Fake::otaAborts=0;
  Fake::autoLoop=true;Fake::onYield=nullptr;Fake::onRead=nullptr;Fake::psramPhysical=8388608;Fake::flashPhysical=GitUpdateProfile::flashBytes;
  Fake::written.clear();Fake::responses.clear();Fake::urls.clear();Fake::seenAccept.clear();
  rebootDelay={};runningPartition={GitUpdateProfile::app0Address,GitUpdateProfile::appBytes};
  nextPartition={GitUpdateProfile::app1Address,GitUpdateProfile::appBytes,ESP_PARTITION_TYPE_APP,ESP_PARTITION_SUBTYPE_APP_OTA_1};
  fsPartition={GitUpdateProfile::fsAddress,GitUpdateProfile::fsBytes,ESP_PARTITION_TYPE_DATA,ESP_PARTITION_SUBTYPE_DATA_SPIFFS};
  nvsPartition={GitUpdateProfile::nvsAddress,GitUpdateProfile::nvsBytes,ESP_PARTITION_TYPE_DATA,ESP_PARTITION_SUBTYPE_DATA_NVS};
  otaPartition={0xe000,0x2000,ESP_PARTITION_TYPE_DATA,ESP_PARTITION_SUBTYPE_DATA_OTA};fakeImageState=ESP_OTA_IMG_VALID;
  fixtures();gitUpdate.begin(prepare);
}
static void run() { try { worker(nullptr); } catch(FakeTaskExit &) {} gitUpdate.loop(); }
static std::string currentState() { JsonDocument json;gitUpdate.status(json.to<JsonObject>());return json["state"].as<std::string>(); }
static void available() { CHECK(gitUpdate.requestCheck());run();CHECK(currentState()=="available");CHECK(!gitUpdate.busy()); }
static void installNow() { CHECK(gitUpdate.requestInstall());run(); }
static void check_success_headers_and_manual_exclusion() {
  reset();CHECK(gitUpdate.beginManualUpdate());CHECK(!gitUpdate.requestCheck());gitUpdate.endManualUpdate();
  CHECK(gitUpdate.requestCheck());CHECK(!gitUpdate.beginManualUpdate());run();
  CHECK(currentState()=="available");CHECK(Fake::seenAccept[apiUrl]=="application/vnd.github+json");
  CHECK(Fake::seenAccept[manifestUrl]=="application/octet-stream");CHECK(!gitUpdate.busy());
  CHECK(Fake::otaBegins==0);CHECK(!rebootDelay.reboot);
  JsonDocument status;gitUpdate.status(status.to<JsonObject>());CHECK(status["workerStackMinimumBytes"].as<unsigned>()==8192);
  CHECK(status["lifetimeMinimumInternalFree"].as<unsigned>()==70000);
  CHECK(status["bootloaderIdentity"].as<std::string>().find("ESPSOMFY_BOOT_ID:")==0);
}
static void verified_connection_and_host_failures_never_write() {
  reset();Fake::responses[apiUrl].connection=false;CHECK(gitUpdate.requestCheck());run();CHECK(currentState()=="error");CHECK(Fake::otaBegins==0);
  for(const char *url:{"http://github.com/a","https://github.com.evil/a","https://127.0.0.1/a"}) {
    reset();Fake::responses[apiUrl].status=302;Fake::responses[apiUrl].location=url;
    CHECK(gitUpdate.requestCheck());run();CHECK(currentState()=="error");CHECK(Fake::urls.size()==1);CHECK(Fake::otaBegins==0);
  }
  reset();const auto good=Fake::responses[manifestUrl];
  const std::string cdn="https://release-assets.githubusercontent.com/path?sig=ignored";
  Fake::responses[cdn]=good;Fake::responses[manifestUrl].status=302;Fake::responses[manifestUrl].location=cdn;
  available();CHECK(Fake::seenAccept[cdn]=="application/octet-stream");
}
static void transfer_faults_leave_active_slot_untouched() {
  reset();available();Fake::responses[firmwareUrl].truncateAt=5000;installNow();CHECK(currentState()=="error");CHECK(Fake::otaBegins==0);CHECK(!rebootDelay.reboot);
  reset();available();Fake::responses[firmwareUrl].length=0x330001;installNow();CHECK(currentState()=="error");CHECK(Fake::otaBegins==0);
  reset();available();Fake::responses[firmwareUrl].chunked=true;installNow();CHECK(currentState()=="error");CHECK(Fake::otaBegins==0);
  reset();available();Fake::responses[firmwareUrl].body[7000]^=1;installNow();CHECK(currentState()=="error");CHECK(Fake::otaBegins==0);
  reset();available();Fake::responses[firmwareUrl].encoding="gzip";installNow();CHECK(currentState()=="error");CHECK(Fake::otaBegins==0);
  reset();Fake::responses[apiUrl].chunked=true;Fake::responses[apiUrl].length=0;available();
}
static void updateManifest(const std::function<void(JsonDocument &)> &change) {
  JsonDocument manifest,release;
  CHECK(!deserializeJson(manifest,Fake::responses[manifestUrl].body));
  CHECK(!deserializeJson(release,Fake::responses[apiUrl].body));
  change(manifest);
  auto &body=Fake::responses[manifestUrl].body;
  body=stringify(manifest);
  release["assets"][1]["size"]=body.size();release["assets"][1]["digest"]="sha256:"+hash(body);
  Fake::responses[apiUrl].body=stringify(release);
}
static void authenticated_but_incompatible_images_never_write() {
  for(int which=0;which<7;which++) {
    reset();
    updateManifest([&](JsonDocument &m) {
      if(which==0) m["board"]=std::string(GitUpdatePolicy::BOARD)=="lilygo-t-embed-cc1101-plus" ? "lilygo-t-embed-cc1101" : "lilygo-t-embed-cc1101-plus";
      if(which==1) m["layout"]="huge_app";
      if(which==2) m["app_compat"]="different-web-assets";
      if(which==3) m["bootloader_sha256"]=std::string(64,'c');
      if(which==4) m["source_sha"]="not-a-git-revision";
      if(which==5) m["asset"]=std::string(GitUpdatePolicy::FIRMWARE_ASSET)=="firmware-lilygo-t-embed-cc1101-plus.bin" ? "firmware-esp32p4.bin" : "firmware-lilygo-t-embed-cc1101-plus.bin";
      if(which==6) m["psram_bytes"]=0;
    });
    CHECK(gitUpdate.requestCheck());run();CHECK(currentState()=="error");CHECK(Fake::otaBegins==0);
  }
  // The network digest can be internally consistent while the binary embeds a
  // generic or unrelated board marker. Identity is an independent requirement.
  reset();available();Fake::responses[firmwareUrl].body[100]='Z';
  const auto changedDigest=hash(Fake::responses[firmwareUrl].body);
  updateManifest([&](JsonDocument &m) { m["sha256"]=changedDigest; });
  JsonDocument release;CHECK(!deserializeJson(release,Fake::responses[apiUrl].body));
  release["assets"][0]["digest"]="sha256:"+changedDigest;Fake::responses[apiUrl].body=stringify(release);
  installNow();CHECK(currentState()=="error");CHECK(Fake::otaBegins==0);
  reset();available();
  esp_image_header_t wrongChip;memcpy(&wrongChip,Fake::responses[firmwareUrl].body.data(),sizeof(wrongChip));
  wrongChip.chip_id=GitUpdateProfile::chipId==9 ? 18 : 9;
  memcpy(Fake::responses[firmwareUrl].body.data(),&wrongChip,sizeof(wrongChip));
  const auto wrongChipDigest=hash(Fake::responses[firmwareUrl].body);
  updateManifest([&](JsonDocument &m) { m["sha256"]=wrongChipDigest; });
  CHECK(!deserializeJson(release,Fake::responses[apiUrl].body));
  release["assets"][0]["digest"]="sha256:"+wrongChipDigest;Fake::responses[apiUrl].body=stringify(release);
  installNow();CHECK(currentState()=="error");CHECK(Fake::otaBegins==0);
}
static void check_environment_and_allocator_fail_closed() {
  reset();Fake::wallClock=0;CHECK(gitUpdate.requestCheck());run();CHECK(currentState()=="error");CHECK(Fake::urls.empty());
  reset();Fake::bootloaderMatches=false;CHECK(gitUpdate.requestCheck());run();CHECK(currentState()=="error");CHECK(Fake::urls.empty());
  reset();Fake::memoryReady=false;CHECK(gitUpdate.requestCheck());run();CHECK(currentState()=="error");CHECK(Fake::urls.empty());
  reset();Fake::psramPhysical=33554432;available();
  reset();Fake::flashPhysical/=2;CHECK(gitUpdate.requestCheck());run();CHECK(currentState()=="error");CHECK(Fake::urls.empty());
  reset();Fake::psramPhysical=4194304;CHECK(gitUpdate.requestCheck());run();CHECK(currentState()=="error");CHECK(Fake::urls.empty());
  reset();Fake::fsCompatible=false;task=nullptr;gitUpdate.begin(prepare);CHECK(gitUpdate.requestCheck());run();CHECK(currentState()=="error");CHECK(Fake::urls.empty());
  reset();nextPartition.address=0x10000;CHECK(gitUpdate.requestCheck());run();CHECK(currentState()=="error");CHECK(Fake::urls.empty());
  reset();fakeImageState=ESP_OTA_IMG_PENDING_VERIFY;CHECK(gitUpdate.requestCheck());run();CHECK(currentState()=="error");CHECK(Fake::urls.empty());
  reset();Fake::allocationFail=true;CHECK(gitUpdate.requestCheck());run();CHECK(currentState()=="error");CHECK(Fake::otaBegins==0);
}
static void altered_partition_semantics_reject_before_https() {
  for(unsigned which=0;which<8;which++) {
    reset();
    if(which==0) otaPartition.address+=0x1000;
    if(which==1) otaPartition.size-=0x1000;
    if(which==2) { runningPartition.subtype=ESP_PARTITION_SUBTYPE_APP_OTA_1;nextPartition.subtype=ESP_PARTITION_SUBTYPE_APP_OTA_0; }
    if(which==3) runningPartition.type=ESP_PARTITION_TYPE_DATA;
    if(which==4) Fake::extraAppPartition=true;
    if(which==5) Fake::extraOtaPartition=true;
    if(which==6) fsPartition.size+=0x1000;
    if(which==7) nvsPartition.address+=0x1000;
    CHECK(gitUpdate.requestCheck());run();CHECK(currentState()=="error");CHECK(Fake::urls.empty());CHECK(Fake::otaBegins==0);
  }
}
static void cancel_before_and_during_preparation_preserves_slot() {
  reset();CHECK(gitUpdate.requestCheck());CHECK(gitUpdate.requestCancel());run();CHECK(currentState()=="cancelled");CHECK(Fake::otaBegins==0);CHECK(!gitUpdate.busy());
  reset();available();Fake::onRead=[] { if(currentState()=="downloading") { CHECK(gitUpdate.requestCancel());Fake::onRead=nullptr; } };
  installNow();CHECK(currentState()=="cancelled");CHECK(Fake::otaBegins==0);CHECK(Fake::prepareCalls==0);
  reset();available();Fake::cancelledDuringPrepare=true;installNow();CHECK(currentState()=="cancelled");CHECK(Fake::otaBegins==0);CHECK(Fake::prepareCalls==1);CHECK(rebootDelay.reboot);CHECK(gitUpdate.busy());
  reset();available();Fake::autoLoop=false;installNow();CHECK(currentState()=="error");CHECK(Fake::otaBegins==0);CHECK(Fake::prepareCalls==0);gitUpdate.loop();CHECK(Fake::prepareCalls==0);
}
static void ota_failures_never_activate_and_resume_with_reboot() {
  for(int which=0;which<4;which++) {
    reset();available();
    Fake::failBegin=which==0;Fake::failWrite=which==1;Fake::failEnd=which==2;Fake::failActivate=which==3;
    installNow();CHECK(currentState()=="error");CHECK(Fake::prepareCalls==1);CHECK(rebootDelay.reboot);CHECK(gitUpdate.busy());
    CHECK(Fake::otaActivations==(which==3 ? 1u : 0u));
    if(which==1) CHECK(Fake::otaAborts==1);
  }
}
static void success_writes_only_inactive_slot_and_refuses_late_cancel() {
  reset();available();Fake::cancelDuringWrite=true;installNow();
  CHECK(currentState()=="rebooting");CHECK(Fake::otaAddress==GitUpdateProfile::app1Address);CHECK(Fake::otaBegins==1);CHECK(Fake::otaEnds==1);CHECK(Fake::otaActivations==1);
  CHECK(Fake::written==std::vector<uint8_t>(firmwareBody.begin(),firmwareBody.end()));
  CHECK(!Fake::cancelAcceptedDuringWrite);CHECK(!gitUpdate.requestCancel());CHECK(!gitUpdate.requestInstall());CHECK(rebootDelay.reboot);
}
int main() {
  check_success_headers_and_manual_exclusion();verified_connection_and_host_failures_never_write();
  transfer_faults_leave_active_slot_untouched();authenticated_but_incompatible_images_never_write();check_environment_and_allocator_fail_closed();
  altered_partition_semantics_reject_before_https();cancel_before_and_during_preparation_preserves_slot();ota_failures_never_activate_and_resume_with_reboot();
  success_writes_only_inactive_slot_and_refuses_late_cancel();
  std::cout<<GitUpdatePolicy::BOARD<<" production worker: "<<assertions<<" assertions passed\n";
}
