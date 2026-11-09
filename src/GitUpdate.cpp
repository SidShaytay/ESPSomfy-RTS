#if defined(ARDUINO) && defined(GIT_UPDATE_HOST_TEST)
#error "Updater host test hooks are forbidden in firmware"
#endif
#include "GitUpdate.h"
#include "GitUpdatePolicy.h"
#include "GitUpdateImage.h"
#include "ManualUpdateLifecycle.h"
#include <atomic>
#include <cstdio>
#include <cstring>
#if defined(GIT_UPDATE_HOST_TEST)
#include "../tests/git-update-worker/Runtime.h"
#elif defined(ARDUINO)
#include <freertos/FreeRTOS.h>
#else
#include <mutex>
#endif

GitUpdateClass gitUpdate;
namespace {
#if defined(ARDUINO) || defined(GIT_UPDATE_HOST_TEST)
portMUX_TYPE serviceLock=portMUX_INITIALIZER_UNLOCKED;
struct Guard { Guard() { portENTER_CRITICAL(&serviceLock); } ~Guard() { portEXIT_CRITICAL(&serviceLock); } };
#else
std::mutex serviceLock;
struct Guard { std::lock_guard<std::mutex> lock; Guard():lock(serviceLock) {} };
#endif
enum class Owner : uint8_t { none,check,prepare,manual,restore,counter,restart,configuration,install };
Owner owner=Owner::none;
uint64_t ownerGeneration=0;
GitUpdateSnapshot view;
ManualUpdateLifecycle manualTransfer;
GitUpdateInstallHooks installHooks;
uint64_t installLease=0;
#if defined(ESPSOMFY_LILYGO_T_EMBED_CC1101)
bool matches(const GitUpdateCandidate &a,const GitUpdateCandidate &b) {
  return a.generation==b.generation && a.size==b.size &&
    memcmp(a.sourceSha,b.sourceSha,sizeof(a.sourceSha))==0 &&
    memcmp(a.version,b.version,sizeof(a.version))==0 &&
    memcmp(a.imageSha,b.imageSha,sizeof(a.imageSha))==0 &&
    memcmp(a.manifestSha,b.manifestSha,sizeof(a.manifestSha))==0;
}
#endif
bool acquire(Owner kind) {
  if(owner!=Owner::none || ownerGeneration==UINT64_MAX) return false;
  owner=kind;++ownerGeneration;return true;
}
}
const char *gitUpdatePhase(GitUpdatePhase phase) {
  static const char *const names[]={"unavailable","idle","queued","checking","available","current","preparing","prepared","changed","cancelled","error","downloading","verifying","writing","written","cleanupPending"};
  const size_t index=static_cast<size_t>(phase);
  return index<sizeof(names)/sizeof(*names) ? names[index] : "unavailable";
}
const char *gitUpdateReason(GitUpdateReason reason) {
  static const char *const text[]={"","Unsupported updater hardware","Updater is not started","Development builds cannot prepare stable updates",
    "Insufficient updater memory","Updater admission refused","Clock is unset","Unsupported physical hardware","Partition or OTA state differs",
    "Installed bootloader is not attested","Installed web assets are not attested","Verified HTTPS connection failed","Invalid download headers",
    "Refused HTTPS redirect","No compatible published stable release","Download incomplete or exceeds bound","Invalid release metadata",
    "Release repository differs","Invalid update manifest","Authenticated digest differs","Release compatibility differs",
    "Release is not newer","Request cancelled","Operation exceeded time limit","Release changed; confirm the new candidate","Candidate is no longer current","Persisted settings are uncertain; complete recovery before restart","TLS certificate validity window refused","Updater freshness is unavailable","Staged firmware verification failed","Inactive firmware write failed"};
  const size_t index=static_cast<size_t>(reason);
  return index<sizeof(text)/sizeof(*text) ? text[index] : "Updater unavailable";
}
namespace {
Owner externalOwner(GitUpdateExternalOwner kind) {
  switch(kind) {
    case GitUpdateExternalOwner::manual: return Owner::manual;
    case GitUpdateExternalOwner::restore: return Owner::restore;
    case GitUpdateExternalOwner::counter: return Owner::counter;
    case GitUpdateExternalOwner::restart: return Owner::restart;
    case GitUpdateExternalOwner::configuration: return Owner::configuration;
  }
  return Owner::none;
}
bool owns(const GitUpdateOwnerToken &token) {
  const auto expected=externalOwner(token.kind);
  return token.generation && token.generation==ownerGeneration && expected!=Owner::none && owner==expected;
}
}
bool GitUpdateClass::acquireExternal(GitUpdateExternalOwner kind,GitUpdateOwnerToken &token) {
  const auto requested=externalOwner(kind);
  if(requested==Owner::none || requested==Owner::restart) return false;
  Guard guard;
  if(!acquire(requested)) return false;
  token.generation=ownerGeneration;token.kind=kind;
  view.compatible=false;view.canPrepare=false;view.candidate={};
  if(view.phase!=GitUpdatePhase::unavailable) view.phase=GitUpdatePhase::idle;
  return true;
}
bool GitUpdateClass::ownsExternal(const GitUpdateOwnerToken &token) const {
  Guard guard;return owns(token);
}
bool GitUpdateClass::releaseExternal(const GitUpdateOwnerToken &token) {
  Guard guard;
  if(!owns(token) || manualTransfer.active(token)) return false;
  owner=Owner::none;return true;
}
bool GitUpdateClass::beginTransfer(const GitUpdateOwnerToken &token,ManualUpdateKind kind,uint64_t now) {
  Guard guard;return owns(token) && manualTransfer.begin(token,kind,now);
}
bool GitUpdateClass::claimTransfer(const GitUpdateOwnerToken &token,uint64_t now,bool queuedRestore) {
  Guard guard;return owns(token) && manualTransfer.claim(token,now,queuedRestore);
}
bool GitUpdateClass::transferContinuing(const GitUpdateOwnerToken &token) const {
  Guard guard;return owns(token) && manualTransfer.continuing(token);
}
bool GitUpdateClass::transferContinuing(const GitUpdateOwnerToken &token,uint64_t now) {
  Guard guard;return owns(token) && manualTransfer.continuing(token,now);
}
bool GitUpdateClass::transferResource(const GitUpdateOwnerToken &token,bool opened) {
  Guard guard;return owns(token) && manualTransfer.resource(token,opened);
}
bool GitUpdateClass::queueTransfer(const GitUpdateOwnerToken &token) {
  Guard guard;return owns(token) && manualTransfer.queue(token);
}
bool GitUpdateClass::holdTransferRestart(const GitUpdateOwnerToken &token) {
  Guard guard;return owns(token) && manualTransfer.holdRestart(token);
}
bool GitUpdateClass::holdTransferRecovery(const GitUpdateOwnerToken &token,uint64_t now) {
  Guard guard;return owns(token) && manualTransfer.holdRecovery(token,now);
}
bool GitUpdateClass::claimTransferRecovery(const GitUpdateOwnerToken &token,uint64_t now) {
  Guard guard;return owns(token) && manualTransfer.claimRecovery(token,now);
}
bool GitUpdateClass::transferRestoreClaimed(const GitUpdateOwnerToken &token,uint64_t now) {
  Guard guard;return owns(token) && manualTransfer.restoreClaimed(token,now);
}
bool GitUpdateClass::transferRestartHeld(const GitUpdateOwnerToken &token,uint64_t now) {
  Guard guard;return owns(token) && manualTransfer.restartHeld(token,now);
}
bool GitUpdateClass::acquireRestart(GitUpdateOwnerToken &result,uint64_t now,const GitUpdateOwnerToken &expected) {
  Guard guard;
  if(ownerGeneration==UINT64_MAX) return false;
  if(expected.generation) {
    if(!owns(expected) || !manualTransfer.handoffRestart(expected,now)) return false;
    owner=Owner::restart;++ownerGeneration;
  } else if(!acquire(Owner::restart)) return false;
  result={ownerGeneration,GitUpdateExternalOwner::restart};return true;
}
bool GitUpdateClass::releaseRestartSource(const GitUpdateOwnerToken &token,uint64_t now) {
  Guard guard;
  if(!owns(token) || !manualTransfer.cancelRestart(token,now)) return false;
  owner=Owner::none;return true;
}
bool GitUpdateClass::finishTransfer(const GitUpdateOwnerToken &token,uint64_t now,bool progress) {
  Guard guard;return owns(token) && manualTransfer.finish(token,now,progress);
}
bool GitUpdateClass::retireTransfer(const GitUpdateOwnerToken &token) {
  Guard guard;return owns(token) && manualTransfer.retire(token);
}
ManualUpdateCleanup GitUpdateClass::claimTransferCleanup(uint64_t now) {
  Guard guard;if(installLease)return {};const auto work=manualTransfer.cleanup(now);
  return owns(work.token) ? work : ManualUpdateCleanup{};
}
bool GitUpdateClass::closeTransfer(const GitUpdateOwnerToken &token) {
  Guard guard;return owns(token) && manualTransfer.close(token);
}
bool GitUpdateClass::busy() const { Guard guard;return owner!=Owner::none; }
GitUpdateSnapshot GitUpdateClass::snapshot() const {
  Guard guard;
  auto copy=view;copy.busy=owner!=Owner::none;
#ifdef ESPSOMFY_LILYGO_T_EMBED_CC1101
  copy.supported=true;
  copy.canInstall=!copy.busy && copy.compatible && copy.canPrepare && installHooks.current && installHooks.prepare && installHooks.cleanup && installHooks.written;
#else
  copy.reason=GitUpdateReason::unsupported;
#endif
#ifdef ESPSOMFY_RELEASE_TAG
  snprintf(copy.installed,sizeof(copy.installed),"%s",ESPSOMFY_RELEASE_TAG);
#endif
  copy.canPrepare=copy.canPrepare && !copy.busy;
  return copy;
}
void GitUpdateClass::status(JsonObject out) const {
  const auto s=snapshot();
  out["supported"]=s.supported;out["state"]=gitUpdatePhase(s.phase);
  out["error"]=gitUpdateReason(s.reason);out["busy"]=s.busy;
  out["installed"]=s.installed;out["available"]=s.candidate.version;
  out["source"]=GitUpdatePolicy::REPOSITORY;
  out["compatible"]=s.compatible;out["canInstall"]=s.canInstall;out["canPrepare"]=s.canPrepare;
  out["progress"]=s.progress;out["cancellable"]=s.cancellable;
  char generation[21];snprintf(generation,sizeof(generation),"%llu",static_cast<unsigned long long>(s.candidate.generation));
  out["candidateGeneration"]=generation;
  out["sourceSha"]=s.candidate.sourceSha;out["imageSha256"]=s.candidate.imageSha;
  out["manifestSha256"]=s.candidate.manifestSha;
  out["minInternalFree"]=s.minInternalFree;out["minLargestInternalBlock"]=s.minLargestInternal;
  out["maxPsramUsed"]=s.maxPsramUsed;out["workerStackMinimumBytes"]=s.workerStackMinimum;
  out["tlsRequested"]=s.tlsRequested;out["tlsPeakRequested"]=s.tlsPeak;
  out["tlsInternalRequested"]=s.tlsInternalRequested;out["tlsAllocationFailures"]=s.tlsFailures;
}

#if defined(ESPSOMFY_LILYGO_T_EMBED_CC1101)
#ifndef GIT_UPDATE_HOST_TEST
#include "GitUpdateMemory.h"
#include "GitUpdateAssets.h"
#include "GitUpdateBootloader.h"
#include <Arduino.h>
#include <esp_http_client.h>
#include <esp_crt_bundle.h>
#include <esp_ota_ops.h>
#include <esp_heap_caps.h>
#include <esp_psram.h>
#include <esp_random.h>
#include <esp_timer.h>
#include <freertos/task.h>
#include <mbedtls/sha256.h>
#endif
#include "GitUpdateCertificate.h"
#include "GitUpdateTransfer.h"
#include "LocalJsonAllocator.h"
#include <ctime>
#include <memory>
#include <string>
namespace {
constexpr uint32_t OPERATION_MS=180000;
constexpr size_t API_LIMIT=65536,MANIFEST_LIMIT=4096,JSON_BUDGET=192u*1024;
constexpr size_t INTERNAL_FREE=48u*1024,INTERNAL_LARGEST=16u*1024;
std::atomic<TaskHandle_t> workerTask{nullptr};
std::atomic<bool> cancelled{false};
std::atomic<uint32_t> operationStart{0};
bool startingWorker=false;
uint8_t admissionPending=0;
uint64_t runningLease=0,candidateGeneration=0;
GitUpdateCandidate operationExpected;
GitUpdateReadAdmission (*admitCallback)()=nullptr;
void *jsonAcquire(size_t size) { return heap_caps_aligned_alloc(alignof(max_align_t),size,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT); }
void jsonRelease(void *p) { heap_caps_free(p); }
GitUpdateReason stoppedReason() {
  if(cancelled.load()) return GitUpdateReason::cancelled;
  return uint32_t(millis()-operationStart)>OPERATION_MS ? GitUpdateReason::timeout : GitUpdateReason::none;
}
bool stopped() { return stoppedReason()!=GitUpdateReason::none; }
void cooperate() { vTaskDelay(1); }
void terminalLocked(GitUpdateReason reason) {
  view.reason=reason;
  view.phase=reason==GitUpdateReason::cancelled ? GitUpdatePhase::cancelled : GitUpdatePhase::error;
  view.compatible=false;view.candidate={};view.cancellable=false;view.canPrepare=false;
}
void fail(GitUpdateReason reason) {
  const auto stop=stoppedReason();
  Guard guard;terminalLocked(stop==GitUpdateReason::none ? reason : stop);
}
void measure() {
  const size_t internal=heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT);
  const size_t largest=heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT);
  const size_t used=ESP.getPsramSize()-ESP.getFreePsram();
  const auto tls=gitUpdateMemoryStats();
  const auto task=workerTask.load();
  const size_t stack=task ? uxTaskGetStackHighWaterMark(task) : 0;
  Guard guard;
  if(!view.minInternalFree || internal<view.minInternalFree) view.minInternalFree=internal;
  if(!view.minLargestInternal || largest<view.minLargestInternal) view.minLargestInternal=largest;
  if(used>view.maxPsramUsed) view.maxPsramUsed=used;
  view.workerStackMinimum=stack;
  view.tlsRequested=tls.requested;view.tlsPeak=tls.peak;
  view.tlsInternalRequested=tls.internalRequested;view.tlsFailures=tls.failures;
}
struct Buffer {
  uint8_t *data;
  size_t capacity;
  explicit Buffer(size_t size):data(static_cast<uint8_t *>(heap_caps_malloc(size,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT))),capacity(size) {}
  ~Buffer() { heap_caps_free(data); }
  Buffer(const Buffer &)=delete;
};
struct Headers { char location[2048]={};bool invalid=false,sawLocation=false; };
esp_err_t headerEvent(esp_http_client_event_t *event) {
  if(event->event_id!=HTTP_EVENT_ON_HEADER) return ESP_OK;
  auto &headers=*static_cast<Headers *>(event->user_data);
  if(!event->header_key || !event->header_value) { headers.invalid=true;return ESP_OK; }
  if(strcasecmp(event->header_key,"Location")==0) {
    const size_t length=strnlen(event->header_value,sizeof(headers.location));
    if(headers.sawLocation || length>=sizeof(headers.location)) headers.invalid=true;
    else memcpy(headers.location,event->header_value,length+1);
    headers.sawLocation=true;
  }
  if(strcasecmp(event->header_key,"Content-Encoding")==0 && strcasecmp(event->header_value,"identity")!=0) headers.invalid=true;
  return ESP_OK;
}
bool download(const std::string &initial,Buffer &out,size_t &received,bool api=false) {
  std::string url=initial;received=0;
  for(unsigned hop=0;hop<4;hop++) {
    if(stopped()) { fail(GitUpdateReason::cancelled);return false; }
    if(!GitUpdatePolicy::allowedUrl(url.c_str())) { fail(GitUpdateReason::redirect);return false; }
    Headers headers;
    GitUpdateCertificate certificate(ESPSOMFY_SOURCE_EPOCH);
    GitUpdateCertificate::Scope certificateScope(certificate);
    esp_http_client_config_t config={};
    config.url=url.c_str();config.timeout_ms=3000;config.disable_auto_redirect=true;
    config.max_authorization_retries=-1;config.crt_bundle_attach=GitUpdateCertificate::attach;
    config.is_async=false;
    config.skip_cert_common_name_check=false;config.event_handler=headerEvent;config.user_data=&headers;
    config.buffer_size=2048;config.buffer_size_tx=1024;
    config.user_agent="ESPSomfy-RTS-verified-updater/2";config.transport_type=HTTP_TRANSPORT_OVER_SSL;
    const auto client=esp_http_client_init(&config);
    if(!client) { fail(GitUpdateReason::memory);return false; }
    struct Cleanup { esp_http_client_handle_t client;~Cleanup() { esp_http_client_cleanup(client); } } cleanup{client};
    if(esp_http_client_set_header(client,"Accept-Encoding","identity")!=ESP_OK ||
        esp_http_client_set_header(client,"Accept",api ? "application/vnd.github+json" : "application/octet-stream")!=ESP_OK) { fail(GitUpdateReason::transport);return false; }
    const bool opened=esp_http_client_open(client,0)==ESP_OK;
    if(!opened || !certificate.complete()) {
      const auto timeFailure=certificate.failure();
      fail(certificate.rejectedByBundle() ? GitUpdateReason::transport :
        (timeFailure==GitUpdateCertificate::Failure::clock ? GitUpdateReason::clock :
        (timeFailure!=GitUpdateCertificate::Failure::none ? GitUpdateReason::certificateTime : GitUpdateReason::transport)));
      return false;
    }
    measure();
    const int64_t length=esp_http_client_fetch_headers(client);
    const int code=esp_http_client_get_status_code(client);
    if(headers.invalid || length<0) { fail(GitUpdateReason::headers);return false; }
    if(code==301 || code==302 || code==303 || code==307 || code==308) {
      if(!headers.location[0] || !GitUpdatePolicy::allowedUrl(headers.location)) { fail(GitUpdateReason::redirect);return false; }
      url=headers.location;continue;
    }
    if(code!=200) { fail(code==404 ? GitUpdateReason::notPublished : GitUpdateReason::transport);return false; }
    const GitUpdateTransfer::Bounds bounds{out.capacity,length,esp_http_client_is_chunked_response(client),api};
    const auto result=GitUpdateTransfer::readBody(bounds,out.data,received,
      [&](uint8_t *destination,size_t count) { return esp_http_client_read(client,reinterpret_cast<char *>(destination),count); },
      [&] { return esp_http_client_is_complete_data_received(client); },stopped,
      [&](size_t count,size_t total) {
        { Guard guard;view.progress=total ? static_cast<uint8_t>((count*100)/total) : 0; }
        cooperate();
      });
    if(result!=GitUpdateTransfer::Result::Complete || stopped()) { fail(GitUpdateReason::body);return false; }
    return true;
  }
  fail(GitUpdateReason::redirect);return false;
}
bool digestMatches(const uint8_t *data,size_t size,const std::string &expected) {
  if(expected.size()!=64) return false;
  uint8_t digest[32];if(mbedtls_sha256(data,size,digest,0)!=0) return false;
  char actual[65];for(size_t i=0;i<32;i++) snprintf(actual+2*i,3,"%02x",digest[i]);
  return expected==actual;
}
bool text(JsonVariantConst value,size_t maximum,std::string &out) {
  if(!value.is<const char *>()) return false;
  const JsonString input=value.as<JsonString>();
  if(!input.c_str() || input.size()>maximum || memchr(input.c_str(),0,input.size())) return false;
  out.assign(input.c_str(),input.size());return true;
}
bool manifestFrom(JsonDocument &json,GitUpdatePolicy::Manifest &m) {
  for(const char *field:{"schema","size","flash_bytes","psram_bytes","features"}) if(!json[field].is<uint32_t>()) return false;
  m.schema=json["schema"];m.size=json["size"];m.flashBytes=json["flash_bytes"];m.psramBytes=json["psram_bytes"];m.features=json["features"];
  if(!json["compatible_boards"].is<JsonArray>() || json["compatible_boards"].size()!=2) return false;
  for(JsonVariantConst board:json["compatible_boards"].as<JsonArrayConst>()) {
    std::string name;if(!text(board,63,name)) return false;m.compatibleBoards.push_back(name);
  }
  return text(json["board"],63,m.board) && text(json["layout"],63,m.layout) &&
    text(json["assets_mode"],7,m.assetsMode) && text(json["app_compat"],71,m.appCompat) && text(json["version"],47,m.version) &&
    text(json["source_sha"],40,m.sourceSha) && text(json["asset"],95,m.asset) &&
    text(json["sha256"],64,m.sha256) && text(json["channel"],16,m.channel) && text(json["bootloader_sha256"],64,m.bootloaderSha);
}
bool environment() {
  if(stopped()) { fail(GitUpdateReason::cancelled);return false; }
  if(time(nullptr)<ESPSOMFY_SOURCE_EPOCH) { fail(GitUpdateReason::clock);return false; }
  if(!gitUpdateMemoryReady() || heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT)<INTERNAL_FREE ||
      heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT)<INTERNAL_LARGEST) { fail(GitUpdateReason::memory);return false; }
  if(ESP.getFlashChipSize()!=GitUpdateProfile::flashBytes || esp_psram_get_size()<GitUpdateProfile::psramBytes) { fail(GitUpdateReason::hardware);return false; }
  struct Region { const char *label;int type,subtype;uint32_t address,size; };
  const Region expected[]={
    {"nvs",ESP_PARTITION_TYPE_DATA,ESP_PARTITION_SUBTYPE_DATA_NVS,GitUpdateProfile::nvsAddress,GitUpdateProfile::nvsBytes},
    {"otadata",ESP_PARTITION_TYPE_DATA,ESP_PARTITION_SUBTYPE_DATA_OTA,0xe000,0x2000},
    {"app0",ESP_PARTITION_TYPE_APP,ESP_PARTITION_SUBTYPE_APP_OTA_0,GitUpdateProfile::app0Address,GitUpdateProfile::appBytes},
    {"app1",ESP_PARTITION_TYPE_APP,ESP_PARTITION_SUBTYPE_APP_OTA_1,GitUpdateProfile::app1Address,GitUpdateProfile::appBytes},
    {"spiffs",ESP_PARTITION_TYPE_DATA,ESP_PARTITION_SUBTYPE_DATA_SPIFFS,GitUpdateProfile::fsAddress,GitUpdateProfile::fsBytes},
    {"coredump",ESP_PARTITION_TYPE_DATA,ESP_PARTITION_SUBTYPE_DATA_COREDUMP,0x7f0000,0x10000}};
  unsigned seen=0;
  for(auto it=esp_partition_find(ESP_PARTITION_TYPE_ANY,ESP_PARTITION_SUBTYPE_ANY,nullptr);it;it=esp_partition_next(it)) {
    const auto *partition=esp_partition_get(it);bool found=false;
    for(size_t i=0;partition && i<sizeof(expected)/sizeof(*expected);i++) {
      const auto &r=expected[i];
      if(partition->address==r.address && partition->size==r.size && partition->type==r.type && partition->subtype==r.subtype && strcmp(partition->label,r.label)==0 && !(seen&(1u<<i))) {
        seen|=1u<<i;found=true;break;
      }
    }
    if(!found) { esp_partition_iterator_release(it);fail(GitUpdateReason::layout);return false; }
  }
  const auto *running=esp_ota_get_running_partition();const auto *next=esp_ota_get_next_update_partition(nullptr);
  esp_ota_img_states_t imageState;
  if(seen!=63 || !running || !next || running->address==next->address ||
      !((running->address==GitUpdateProfile::app0Address && next->address==GitUpdateProfile::app1Address) ||
        (running->address==GitUpdateProfile::app1Address && next->address==GitUpdateProfile::app0Address)) ||
      esp_ota_get_state_partition(running,&imageState)!=ESP_OK ||
      (imageState!=ESP_OTA_IMG_VALID && imageState!=ESP_OTA_IMG_UNDEFINED)) { fail(GitUpdateReason::layout);return false; }
  if(gitUpdateBootloaderStatus({stopped,cooperate})!=UpdateAttestation::verified) { fail(GitUpdateReason::bootloader);return false; }
  if(gitUpdateAssetsStatus({stopped,cooperate})!=UpdateAttestation::verified) { fail(GitUpdateReason::assets);return false; }
  if(stopped()) { fail(GitUpdateReason::cancelled);return false; }
  return true;
}
bool metadata(bool prepare,const GitUpdateCandidate &expected,GitUpdatePolicy::Manifest *verified=nullptr) {
  using namespace GitUpdatePolicy;
  LocalUi::JsonAllocator allocator(JSON_BUDGET,jsonAcquire,jsonRelease);
  Buffer api(API_LIMIT);if(!api.data) { fail(GitUpdateReason::memory);return false; }
  size_t received=0;
  if(!download(std::string("https://api.github.com/repos/")+REPOSITORY+"/releases/latest",api,received,true)) return false;
  JsonDocument filter(&allocator);
  for(const char *field:{"id","url","html_url","published_at","tag_name","draft","prerelease"}) filter[field]=true;
  filter["assets"][0]["name"]=true;filter["assets"][0]["size"]=true;filter["assets"][0]["digest"]=true;
  if(filter.overflowed()) { fail(GitUpdateReason::memory);return false; }
  JsonDocument release(&allocator);
  const auto releaseError=deserializeJson(release,api.data,received,DeserializationOption::Filter(filter),DeserializationOption::NestingLimit(6));
  if(releaseError) { fail(releaseError==DeserializationError::NoMemory ? GitUpdateReason::memory : GitUpdateReason::metadata);return false; }
  std::string tag,url,html,published;
  if(!text(release["tag_name"],47,tag) || !stableVersion(tag.c_str()) ||
      !release["draft"].is<bool>() || release["draft"].as<bool>() ||
      !release["prerelease"].is<bool>() || release["prerelease"].as<bool>() ||
      !text(release["published_at"],32,published) || published.empty()) { fail(GitUpdateReason::metadata);return false; }
  if(!release["id"].is<uint64_t>() || release["id"].as<uint64_t>()==0 ||
      !text(release["url"],192,url) || !text(release["html_url"],192,html) ||
      url!=std::string("https://api.github.com/repos/")+REPOSITORY+"/releases/"+std::to_string(release["id"].as<uint64_t>()) ||
      html!=std::string("https://github.com/")+REPOSITORY+"/releases/tag/"+tag) { fail(GitUpdateReason::repository);return false; }
  if(!release["assets"].is<JsonArray>() || release["assets"].size()>128) { fail(GitUpdateReason::metadata);return false; }
  size_t fwSize=0,manifestSize=0;unsigned fwCount=0,manifestCount=0;
  std::string fwDigest,manifestDigest;
  for(JsonVariantConst asset:release["assets"].as<JsonArrayConst>()) {
    std::string name,digest;
    if(!asset.is<JsonObjectConst>() || !text(asset["name"],127,name)) { fail(GitUpdateReason::metadata);return false; }
    if(name!=FIRMWARE_ASSET && name!=MANIFEST_ASSET) continue;
    if(!asset["size"].is<uint32_t>() || !text(asset["digest"],71,digest) || digest.size()!=71 || digest.compare(0,7,"sha256:")!=0) { fail(GitUpdateReason::metadata);return false; }
    if(name==FIRMWARE_ASSET) { ++fwCount;fwSize=asset["size"].as<uint32_t>();fwDigest=digest.substr(7); }
    else { ++manifestCount;manifestSize=asset["size"].as<uint32_t>();manifestDigest=digest.substr(7); }
  }
  if(fwCount!=1 || manifestCount!=1 || fwSize<1024 || fwSize>APP_BYTES || !manifestSize || manifestSize>MANIFEST_LIMIT) { fail(GitUpdateReason::metadata);return false; }
  Buffer body(MANIFEST_LIMIT);if(!body.data) { fail(GitUpdateReason::memory);return false; }
  const std::string assetUrl=std::string("https://github.com/")+REPOSITORY+"/releases/download/"+tag+"/"+MANIFEST_ASSET;
  if(!download(assetUrl,body,received)) return false;
  if(received!=manifestSize || !digestMatches(body.data,received,manifestDigest)) { fail(GitUpdateReason::digest);return false; }
  JsonDocument json(&allocator);Manifest m;
  const auto manifestError=deserializeJson(json,body.data,received,DeserializationOption::NestingLimit(3));
  if(manifestError) { fail(manifestError==DeserializationError::NoMemory ? GitUpdateReason::memory : GitUpdateReason::manifest);return false; }
  if(!manifestFrom(json,m)) { fail(GitUpdateReason::manifest);return false; }
  if(validate(m) || m.bootloaderSha!=gitUpdateBootloaderDigest()) { fail(GitUpdateReason::compatibility);return false; }
  if(m.version!=tag || m.size!=fwSize || m.sha256!=fwDigest) { fail(GitUpdateReason::digest);return false; }
  if(verified)*verified=m; // Copy bounded metadata outside the service lock.
  GitUpdateCandidate found;
  found.size=m.size;
  snprintf(found.version,sizeof(found.version),"%s",m.version.c_str());
  snprintf(found.sourceSha,sizeof(found.sourceSha),"%s",m.sourceSha.c_str());
  snprintf(found.imageSha,sizeof(found.imageSha),"%s",m.sha256.c_str());
  snprintf(found.manifestSha,sizeof(found.manifestSha),"%s",manifestDigest.c_str());
  found.generation=expected.generation;
  const bool same=prepare && matches(found,expected);
  const auto stop=stoppedReason();
  Guard guard;
  if(stop!=GitUpdateReason::none || cancelled.load()) { terminalLocked(cancelled.load() ? GitUpdateReason::cancelled : stop);return false; }
  if(!same) {
    if(candidateGeneration==UINT64_MAX) { terminalLocked(GitUpdateReason::invalidCandidate);return false; }
    found.generation=++candidateGeneration;
  }
  view.candidate=found;view.compatible=true;view.canPrepare=newer(found.version,ESPSOMFY_RELEASE_TAG);
  view.reason=prepare && !same ? GitUpdateReason::candidateChanged : (view.canPrepare ? GitUpdateReason::none : GitUpdateReason::noNewerRelease);
  view.phase=prepare ? (same ? GitUpdatePhase::prepared : GitUpdatePhase::changed) : (view.canPrepare ? GitUpdatePhase::available : GitUpdatePhase::current);
  view.cancellable=false;view.progress=100;
  return true;
}
std::unique_ptr<Buffer> installImage;
GitUpdatePolicy::Manifest installManifest;
GitUpdateOwnerToken installSource;
RadioInstallToken installRadio;
bool installCleanup=false,installWritten=false,installWorkerActive=false,installUncertain=false;
uint64_t transferNow(){const auto t=esp_timer_get_time();return t<0?UINT64_MAX:uint64_t(t)/1000;}
void discardInstallation() {
  GitUpdateOwnerToken source;RadioInstallToken radio;
  {
    Guard guard;
    if(!installLease || installWorkerActive || installUncertain)return;
    source=installSource;radio=installRadio;
    if(manualTransfer.active(source)) {
      const auto now=transferNow();
      if(!manualTransfer.cancelRestart(source,now)) {
        manualTransfer.retire(source);manualTransfer.finish(source,now,false);
        const auto cleanup=manualTransfer.cleanup(now);
        if(cleanup.token.generation!=source.generation || !manualTransfer.resource(source,false) || !manualTransfer.close(source))return;
      }
    }
    view.cancellable=false;
  }
  // The worker has finished all SDK I/O and the lifecycle has closed before
  // loop-owned resources can resume. A busy cleanup keeps the same owner.
  if(source.generation && !installHooks.cleanup(source,radio)){Guard guard;view.phase=GitUpdatePhase::cleanupPending;return;}
  installImage.reset();installManifest={}; // No allocation/free under the lock.
  Guard guard;
  if(ownerGeneration==runningLease && (owner==Owner::install || owner==Owner::manual))owner=Owner::none;
  installSource={};installRadio={};installLease=0;installCleanup=false;installWritten=false;
}
void stageInstallation(const GitUpdateCandidate &expected) {
  if(!environment() || !metadata(true,expected,&installManifest))return;
  {Guard guard;if(!matches(view.candidate,expected)){return;}view.phase=GitUpdatePhase::downloading;view.cancellable=true;view.progress=0;}
  installImage.reset(new(std::nothrow)Buffer(expected.size));
  if(!installImage || !installImage->data){fail(GitUpdateReason::memory);return;}
  size_t received=0;
  const std::string url=std::string("https://github.com/")+GitUpdatePolicy::REPOSITORY+"/releases/download/"+expected.version+"/"+GitUpdatePolicy::FIRMWARE_ASSET;
  if(!download(url,*installImage,received))return;
  if(received!=expected.size){fail(GitUpdateReason::body);return;}
  {Guard guard;view.phase=GitUpdatePhase::verifying;}
  GitUpdateImage::Identity identity;
  if(GitUpdateImage::verify(installImage->data,received,gitUpdateBootloaderIdentity(),&installManifest,identity,{stopped,cooperate})!=GitUpdateImage::Result::verified){fail(GitUpdateReason::invalidImage);return;}
  {Guard guard;admissionPending=3;}
}
void writeInstallation() {
  const auto *target=esp_ota_get_next_update_partition(nullptr);
  esp_ota_handle_t handle=0;
  if(!target || !installImage || stopped() || esp_ota_begin(target,installImage->capacity,&handle)!=ESP_OK){fail(GitUpdateReason::writeFailed);Guard guard;installCleanup=true;return;}
  {Guard guard;manualTransfer.resource(installSource,true);}
  std::unique_ptr<uint8_t,decltype(&heap_caps_free)> chunk(static_cast<uint8_t*>(heap_caps_malloc(4096,MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT)),heap_caps_free);
  bool okay=bool(chunk);
  for(size_t offset=0;okay && offset<installImage->capacity;){
    if(stopped()){okay=false;break;}
    const auto count=std::min(size_t(4096),installImage->capacity-offset);
    memcpy(chunk.get(),installImage->data+offset,count);
    if(esp_ota_write(handle,chunk.get(),count)!=ESP_OK){okay=false;break;}
    offset+=count;cooperate();const auto now=transferNow();
    {Guard guard;okay=manualTransfer.finish(installSource,now,true)&&manualTransfer.claim(installSource,now);view.progress=uint8_t(offset*100/installImage->capacity);}
  }
  if(stopped())okay=false;
  bool closed=true;
  if(!okay){closed=esp_ota_abort(handle)==ESP_OK;fail(GitUpdateReason::writeFailed);}
  else okay=esp_ota_end(handle)==ESP_OK; // The SDK consumes the handle even on error.
  const auto now=transferNow();
  {Guard guard;
    if(!closed){installUncertain=true;view.phase=GitUpdatePhase::error;view.reason=GitUpdateReason::writeFailed;view.cancellable=false;return;}
    // An expired progress claim is reclaimed by discardInstallation. I/O is
    // already closed; no controller cleanup may run before that reclamation.
    manualTransfer.resource(installSource,false);
    if(okay && !stopped() && manualTransfer.continuing(installSource,now) && manualTransfer.holdRestart(installSource) && manualTransfer.finish(installSource,now,true) && manualTransfer.restartHeld(installSource,now)){installWritten=true;view.phase=GitUpdatePhase::written;view.cancellable=false;admissionPending=4;}
    else {installCleanup=true;terminalLocked(stopped() ? stoppedReason() : GitUpdateReason::writeFailed);}
  }
  // No boot selection or restart occurs on the worker.
}
void worker(void *) {
  for(;;) {
    uint32_t command=0;xTaskNotifyWait(0,UINT32_MAX,&command,portMAX_DELAY);
    uint64_t lease;GitUpdateCandidate expected;
    {
      Guard guard;lease=runningLease;expected=operationExpected;
      if(!lease || ownerGeneration!=lease || (owner!=Owner::check && owner!=Owner::prepare && owner!=Owner::install && !(owner==Owner::manual && installLease==lease))) continue;
    }
    if(command==3){stageInstallation(expected);
      {Guard guard;installWorkerActive=false;if(admissionPending!=3)installCleanup=true;}
      continue;
    }
    if(command==4){writeInstallation();Guard guard;installWorkerActive=false;continue;}
    operationStart=millis();
    if(environment()) metadata(command==2,expected);
    measure();
    // All transport/doc buffers have been destroyed before another owner can
    // begin. Cancellation never releases the gate while a worker still uses it.
    { Guard guard;if(ownerGeneration==lease && (owner==Owner::check || owner==Owner::prepare)) owner=Owner::none; }
  }
}
GitUpdateRequest queue(bool prepare,const GitUpdateCandidate &expected) {
  Guard guard;
  if(owner!=Owner::none) return GitUpdateRequest::busy;
  if(!workerTask.load()) return GitUpdateRequest::unavailable;
  if(prepare && (!view.compatible || !view.canPrepare || !expected.generation || !matches(view.candidate,expected))) return GitUpdateRequest::invalidCandidate;
  if(!acquire(prepare ? Owner::prepare : Owner::check)) return GitUpdateRequest::unavailable;
  operationExpected=expected;runningLease=ownerGeneration;
  cancelled.store(false);admissionPending=prepare ? 2 : 1;
  view.phase=GitUpdatePhase::queued;view.reason=GitUpdateReason::none;view.cancellable=true;view.progress=0;
  if(!prepare) { view.compatible=false;view.canPrepare=false;view.candidate={}; }
  return GitUpdateRequest::accepted;
}
}
void GitUpdateClass::begin(GitUpdateReadAdmission (*admit)(),GitUpdateInstallHooks hooks) {
  if(workerTask.load()) return;
  {
    Guard guard;view.supported=true;
    if(startingWorker || workerTask.load()) return;
    if(!GitUpdatePolicy::stableVersion(ESPSOMFY_RELEASE_TAG)) { view.reason=GitUpdateReason::development;return; }
    if(!gitUpdateMemoryReady()) { view.reason=GitUpdateReason::memory;return; }
    if(!admit) { view.reason=GitUpdateReason::admission;return; }
    admitCallback=admit;installHooks=hooks;startingWorker=true;
  }
  // Boot-local opaque freshness, not an authentication secret. Never fall
  // back to a fixed epoch when the bounded seed attempts produce zero.
  uint64_t seed=0;
  for(unsigned attempt=0;attempt<4 && !seed;++attempt)
    seed=((uint64_t(esp_random())<<32)|esp_random()) & (UINT64_MAX>>1);
  if(!seed) { Guard guard;startingWorker=false;view.reason=GitUpdateReason::freshness;return; }
  TaskHandle_t created=nullptr;
  const bool started=xTaskCreatePinnedToCore(worker,"github-update",12288,nullptr,1,&created,0)==pdPASS;
  Guard guard;startingWorker=false;
  if(!started) { view.reason=GitUpdateReason::memory;return; }
  candidateGeneration=seed;workerTask.store(created);view.phase=GitUpdatePhase::idle;view.reason=GitUpdateReason::none;
}
void GitUpdateClass::loop() {
  uint8_t command;uint64_t lease,install;bool active,uncertain;
  { Guard guard;command=admissionPending;lease=runningLease;install=installLease;active=installWorkerActive;uncertain=installUncertain;if(!active)admissionPending=0; }
  if(install){
    if(!installHooks.current(operationExpected.generation))cancelled=true;
    bool cleanup,written;GitUpdateOwnerToken source;RadioInstallToken radio;
    {Guard guard;cleanup=installCleanup || stopped();written=installWritten;source=installSource;radio=installRadio;}
    if(active || uncertain)return; // Cancellation signals the worker; it cannot resume controls.
    if(cleanup){
      if(stopped()){Guard guard;terminalLocked(stoppedReason());}
      discardInstallation();
      return;
    }
    if(command==3){
      if(!installHooks.current(operationExpected.generation)){cancelled=true;return;}
      const auto now=transferNow();GitUpdateOwnerToken prepared;
      {Guard guard;prepared={installLease,GitUpdateExternalOwner::manual};owner=Owner::manual;installSource=prepared;
        if(!manualTransfer.begin(prepared,ManualUpdateKind::firmware,now)||!manualTransfer.claim(prepared,now)){installCleanup=true;return;}}
      const auto result=installHooks.prepare(prepared);
      const bool current=installHooks.current(operationExpected.generation);
      {Guard guard;installRadio=result.radio;if(!result.ready || !current || stopped()){installCleanup=true;terminalLocked(stopped() ? stoppedReason() : GitUpdateReason::admission);return;}view.phase=GitUpdatePhase::writing;installWorkerActive=true;}
      if(xTaskNotify(workerTask.load(),4,eSetValueWithOverwrite)!=pdPASS){Guard guard;installWorkerActive=false;installCleanup=true;terminalLocked(GitUpdateReason::admission);}
      return;
    }
    if(command==4 && written){installHooks.written(source,radio);return;}
    if(command==0)return;
  }
  if(!command) return;
  const bool admitted=!cancelled.load() && admitCallback && admitCallback().allowed;
  {
    Guard guard;
    if(ownerGeneration!=lease || (owner!=Owner::check && owner!=Owner::prepare && owner!=Owner::install)) return;
    if(!admitted || cancelled.load()) {
      terminalLocked(cancelled.load() ? GitUpdateReason::cancelled : GitUpdateReason::admission);
      if(owner==Owner::install)installCleanup=true;else owner=Owner::none;
      return;
    }
    view.phase=owner==Owner::install ? GitUpdatePhase::downloading : command==2 ? GitUpdatePhase::preparing : GitUpdatePhase::checking;
    if(owner==Owner::install){command=3;installWorkerActive=true;}
  }
  // FreeRTOS notification may wake another core; never hold our status mux
  // across scheduler/SDK operations.
  if(xTaskNotify(workerTask.load(),command,eSetValueWithOverwrite)!=pdPASS) {
    Guard guard;
    if(ownerGeneration==lease && (owner==Owner::check || owner==Owner::prepare)) {
      terminalLocked(GitUpdateReason::admission);owner=Owner::none;
    }
    if(ownerGeneration==lease && owner==Owner::install){installWorkerActive=false;installCleanup=true;terminalLocked(GitUpdateReason::admission);}
  }
}
GitUpdateRequest GitUpdateClass::requestInstall(const GitUpdateCandidate &expected,uint64_t &operation) {
  Guard guard;
  if(owner!=Owner::none)return GitUpdateRequest::busy;
  if(!workerTask.load() || !installHooks.current || !installHooks.prepare || !installHooks.cleanup || !installHooks.written)return GitUpdateRequest::unavailable;
  if(!view.compatible || !view.canPrepare || !matches(expected,view.candidate))return GitUpdateRequest::invalidCandidate;
  if(!acquire(Owner::install))return GitUpdateRequest::unavailable;
  installLease=runningLease=ownerGeneration;operationExpected=expected;cancelled=false;admissionPending=1;operationStart=millis();
  operation=expected.generation;view.phase=GitUpdatePhase::queued;view.reason=GitUpdateReason::none;view.progress=0;view.cancellable=true;
  return GitUpdateRequest::accepted;
}
GitUpdateRequest GitUpdateClass::requestCheck() { return queue(false,{}); }
GitUpdateRequest GitUpdateClass::requestPrepare(const GitUpdateCandidate &expected) { return queue(true,expected); }
bool GitUpdateClass::requestCancel() {
  Guard guard;
  if((owner!=Owner::check && owner!=Owner::prepare && !installLease) || !view.cancellable) return false;
  cancelled.store(true);return true;
}

#else
void GitUpdateClass::begin(GitUpdateReadAdmission (*)(),GitUpdateInstallHooks) {}
void GitUpdateClass::loop() {}
GitUpdateRequest GitUpdateClass::requestCheck() { Guard guard;return owner==Owner::none ? GitUpdateRequest::unavailable : GitUpdateRequest::busy; }
GitUpdateRequest GitUpdateClass::requestPrepare(const GitUpdateCandidate &) { return requestCheck(); }
GitUpdateRequest GitUpdateClass::requestInstall(const GitUpdateCandidate &,uint64_t &) { return requestCheck(); }
bool GitUpdateClass::requestCancel() { return false; }
#endif
