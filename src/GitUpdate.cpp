#include "GitUpdate.h"
#include <atomic>

GitUpdateClass gitUpdate;
namespace {
// A single owner for both manual uploads and GitHub operations, including the
// interval between checking compatibility and making an image bootable.
std::atomic<unsigned> owner{0}; // 0 free, 1 GitHub, 2 manual
}
bool GitUpdateClass::beginManualUpdate() {
  unsigned expected=0;
  return owner.compare_exchange_strong(expected,2);
}
void GitUpdateClass::endManualUpdate() {
  unsigned expected=2;
  owner.compare_exchange_strong(expected,0);
}
bool GitUpdateClass::busy() const { return owner.load() != 0; }

#if defined(ESPSOMFY_LILYGO_T_EMBED_CC1101_PLUS)
#include "GitUpdatePolicy.h"
#include "GitUpdateMemory.h"
#include "GitUpdateIdentity.h"
#include "ConfigSettings.h"
#include "Utils.h"
#include <Arduino.h>
#include <esp_http_client.h>
#include <esp_crt_bundle.h>
#include <esp_ota_ops.h>
#include <esp_app_format.h>
#include <esp_heap_caps.h>
#include <esp_psram.h>
#include <mbedtls/sha256.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <ctime>
#include <cstring>
#include <cstdio>
#include <memory>

#ifndef ESPSOMFY_RELEASE_TAG
#define ESPSOMFY_RELEASE_TAG FW_VERSION
#endif
#ifndef ESPSOMFY_APP_COMPAT
#define ESPSOMFY_APP_COMPAT "unidentified"
#endif

extern rebootDelay_t rebootDelay;
namespace {
using namespace GitUpdatePolicy;
constexpr uint32_t OPERATION_MS=180000;
constexpr size_t API_LIMIT=65536;
constexpr size_t MANIFEST_LIMIT=4096;
constexpr size_t CHUNK=4096;
struct Snapshot {
  char state[16]="idle";
  char available[48]="";
  char error[160]="";
  bool compatible=false;
  uint8_t progress=0;
  size_t minInternal=0, minLargestInternal=0, maxPsramUsed=0;
};
portMUX_TYPE lock=portMUX_INITIALIZER_UNLOCKED;
Snapshot snapshot;
TaskHandle_t task=nullptr;
std::atomic<bool> cancelled{false};
std::atomic<unsigned> preparation{0}; // 1 requested, 2 prepared, 3 refused, 4 loop preparing
std::atomic<bool> operationDone{false};
bool (*prepareCallback)()=nullptr;
std::atomic<bool> quiesced{false};
Manifest candidate;
std::string firmwareDigest;

void measure() {
  const size_t internal=heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT);
  const size_t largest=heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT);
  const size_t psram=ESP.getPsramSize()-ESP.getFreePsram();
  portENTER_CRITICAL(&lock);
  if(snapshot.minInternal == 0 || internal < snapshot.minInternal) snapshot.minInternal=internal;
  if(snapshot.minLargestInternal == 0 || largest < snapshot.minLargestInternal) snapshot.minLargestInternal=largest;
  if(psram > snapshot.maxPsramUsed) snapshot.maxPsramUsed=psram;
  portEXIT_CRITICAL(&lock);
}
void state(const char *value, const char *error="") {
  portENTER_CRITICAL(&lock);
  snprintf(snapshot.state,sizeof(snapshot.state),"%s",value);
  snprintf(snapshot.error,sizeof(snapshot.error),"%s",error);
  portEXIT_CRITICAL(&lock);
  measure();
}
void progress(size_t count,size_t total) {
  portENTER_CRITICAL(&lock);
  snapshot.progress=total ? static_cast<uint8_t>((count*100)/total) : 0;
  portEXIT_CRITICAL(&lock);
  measure();
}
bool stopped(uint32_t start) { return cancelled.load() || uint32_t(millis()-start)>OPERATION_MS; }
void fail(const char *error) { state(cancelled.load() ? "cancelled" : "error",error); }
class PsramJsonAllocator : public ArduinoJson::Allocator {
public:
  void *allocate(size_t size) override { return heap_caps_malloc(size,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT); }
  void deallocate(void *memory) override { heap_caps_free(memory); }
  void *reallocate(void *memory,size_t size) override { return heap_caps_realloc(memory,size,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT); }
};
PsramJsonAllocator jsonAllocator;
struct Buffer {
  uint8_t *data=nullptr;
  size_t size=0;
  explicit Buffer(size_t bytes) : data(static_cast<uint8_t *>(heap_caps_malloc(bytes,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT))),size(bytes) {}
  ~Buffer() { heap_caps_free(data); }
  Buffer(const Buffer &)=delete;
};
struct Headers {
  char location[2048]="";
  bool invalid=false;
};
esp_err_t headerEvent(esp_http_client_event_t *event) {
  if(event->event_id != HTTP_EVENT_ON_HEADER) return ESP_OK;
  Headers &headers=*static_cast<Headers *>(event->user_data);
  if(strcasecmp(event->header_key,"Location")==0) {
    const size_t length=strlen(event->header_value);
    if(length >= sizeof(headers.location)) headers.invalid=true;
    else memcpy(headers.location,event->header_value,length+1);
  }
  if(strcasecmp(event->header_key,"Content-Encoding")==0 && strcasecmp(event->header_value,"identity")!=0) headers.invalid=true;
  return ESP_OK;
}
// Each hop gets a new client: the same CA bundle, hostname checks, and HTTPS
// policy apply to GitHub and the signed release-assets redirect alike.
bool download(const std::string &initial, Buffer &out,size_t &received,uint32_t start,bool report=false,bool metadata=false) {
  std::string url=initial;
  received=0;
  for(unsigned hop=0;hop<4;hop++) {
    if(stopped(start) || !allowedUrl(url.c_str())) { fail("Cancelled, timed out, or refused download URL"); return false; }
    Headers headers;
    esp_http_client_config_t config={};
    config.url=url.c_str();
    config.timeout_ms=3000;
    config.disable_auto_redirect=true;
    config.max_authorization_retries=-1;
    config.crt_bundle_attach=esp_crt_bundle_attach;
    config.skip_cert_common_name_check=false;
    config.event_handler=headerEvent;
    config.user_data=&headers;
    config.buffer_size=2048;
    config.buffer_size_tx=1024;
    config.user_agent="ESPSomfy-RTS-verified-updater/1";
    config.transport_type=HTTP_TRANSPORT_OVER_SSL;
    esp_http_client_handle_t client=esp_http_client_init(&config);
    if(!client) { fail("Unable to allocate verified HTTPS client"); return false; }
    struct Cleanup { esp_http_client_handle_t client; ~Cleanup() { esp_http_client_cleanup(client); } } cleanup{client};
    esp_http_client_set_header(client,"Accept-Encoding","identity");
    esp_http_client_set_header(client,"Accept","application/octet-stream");
    if(esp_http_client_open(client,0)!=ESP_OK) { fail("Verified HTTPS connection failed (certificate, hostname, clock, or network)"); return false; }
    measure();
    const int64_t length=esp_http_client_fetch_headers(client);
    const int code=esp_http_client_get_status_code(client);
    if(headers.invalid || length<0) { fail("Invalid download headers"); return false; }
    if(code==301 || code==302 || code==303 || code==307 || code==308) {
      if(!headers.location[0] || !allowedUrl(headers.location)) { fail("Refused HTTPS redirect"); return false; }
      url=headers.location;
      continue;
    }
    if(code!=200) { fail(code==404 ? "No exact-board stable release is published" : "GitHub refused download; retry later"); return false; }
    const bool chunked=esp_http_client_is_chunked_response(client);
    if((chunked && !metadata) || (!chunked && length<=0) || uint64_t(length)>out.size) { fail("Download size missing or exceeds staging bound"); return false; }
    while(chunked ? !esp_http_client_is_complete_data_received(client) : received<size_t(length)) {
      if(stopped(start)) { fail("Download cancelled or exceeded time limit"); return false; }
      const size_t left=(chunked ? out.size : size_t(length))-received;
      if(left==0) { fail("Metadata exceeds download bound");return false; }
      const int bytes=esp_http_client_read(client,reinterpret_cast<char *>(out.data+received),left>CHUNK ? CHUNK : left);
      if(bytes==0 && chunked && esp_http_client_is_complete_data_received(client)) break;
      if(bytes<=0) { fail("Download truncated or connection stalled"); return false; }
      received+=size_t(bytes);
      if(report) progress(received,size_t(length));
      vTaskDelay(1);
    }
    if(!esp_http_client_is_complete_data_received(client)) { fail("Download did not finish cleanly"); return false; }
    return true;
  }
  fail("Too many HTTPS redirects");
  return false;
}
bool digestMatches(const uint8_t *data,size_t bytes,const std::string &expected) {
  if(expected.size()!=64) return false;
  uint8_t digest[32];
  if(mbedtls_sha256(data,bytes,digest,0)!=0) return false;
  char hex[65];
  for(size_t i=0;i<32;i++) snprintf(hex+i*2,3,"%02x",digest[i]);
  return expected==hex;
}
std::string assetUrl(const std::string &tag,const char *asset) {
  return std::string("https://github.com/")+REPOSITORY+"/releases/download/"+tag+"/"+asset;
}
// Source is authenticated GitHub repository metadata, never a user URL or a
// checksum from a second unauthenticated origin. Missing digests fail closed.
bool check(uint32_t start) {
  state("checking");
  candidate=Manifest();
  firmwareDigest.clear();
  portENTER_CRITICAL(&lock);
  snapshot.compatible=false;
  snapshot.available[0]=0;
  snapshot.progress=0;
  portEXIT_CRITICAL(&lock);
  Buffer api(API_LIMIT);
  if(!api.data) { fail("Insufficient PSRAM for release metadata"); return false; }
  size_t size=0;
  if(!download(std::string("https://api.github.com/repos/")+REPOSITORY+"/releases/latest",api,size,start,false,true)) return false;
  JsonDocument filter(&jsonAllocator);
  filter["tag_name"]=true;filter["draft"]=true;filter["prerelease"]=true;
  filter["assets"][0]["name"]=true;filter["assets"][0]["size"]=true;filter["assets"][0]["digest"]=true;
  JsonDocument release(&jsonAllocator);
  if(deserializeJson(release,api.data,size,DeserializationOption::Filter(filter),DeserializationOption::NestingLimit(6))) { fail("Invalid GitHub release metadata"); return false; }
  const char *tag=release["tag_name"] | "";
  if(!release["draft"].is<bool>() || !release["prerelease"].is<bool>() || release["draft"].as<bool>() || release["prerelease"].as<bool>()) { fail("Only published stable releases are supported"); return false; }
  // Validate tag before constructing any release URL.
  Manifest probe;probe=Manifest();probe.schema=1;probe.board=BOARD;probe.layout=LAYOUT;probe.flashBytes=FLASH_BYTES;probe.psramBytes=PSRAM_BYTES;
  probe.appCompat=ESPSOMFY_APP_COMPAT;probe.version=tag;probe.sourceSha=std::string(40,'0');probe.asset=FIRMWARE_ASSET;probe.size=1024;probe.sha256=std::string(64,'0');probe.channel="stable";
  if(validate(probe,ESPSOMFY_APP_COMPAT)) { fail("Release tag is not a supported stable semantic version"); return false; }
  std::string manifestDigest;
  size_t firmwareSize=0,manifestSize=0;
  unsigned firmwareAssets=0,manifestAssets=0;
  for(JsonObject asset:release["assets"].as<JsonArray>()) {
    const char *name=asset["name"] | "";
    const char *digest=asset["digest"] | "";
    const size_t assetSize=asset["size"].is<uint32_t>() ? asset["size"].as<uint32_t>() : 0;
    if(strcmp(name,FIRMWARE_ASSET)==0) { firmwareAssets++;firmwareSize=assetSize;firmwareDigest=digest; }
    if(strcmp(name,MANIFEST_ASSET)==0) { manifestAssets++;manifestSize=assetSize;manifestDigest=digest; }
  }
  if(firmwareAssets!=1 || manifestAssets!=1 || !firmwareSize || firmwareSize>APP_BYTES || !manifestSize || manifestSize>MANIFEST_LIMIT || firmwareDigest.size()!=71 || manifestDigest.size()!=71 || firmwareDigest.compare(0,7,"sha256:") || manifestDigest.compare(0,7,"sha256:")) { fail("Release lacks unique exact-board assets and authenticated SHA-256 digests"); return false; }
  Buffer manifest(MANIFEST_LIMIT);
  if(!manifest.data) { fail("Insufficient PSRAM for update manifest"); return false; }
  if(!download(assetUrl(tag,MANIFEST_ASSET),manifest,size,start)) return false;
  if(size!=manifestSize || !digestMatches(manifest.data,size,manifestDigest.substr(7))) { fail("Release manifest size or digest mismatch"); return false; }
  JsonDocument json(&jsonAllocator);
  if(deserializeJson(json,manifest.data,size,DeserializationOption::NestingLimit(2))) { fail("Invalid update manifest JSON"); return false; }
  if(!json["schema"].is<uint32_t>() || !json["size"].is<uint32_t>() || !json["flash_bytes"].is<uint32_t>() || !json["psram_bytes"].is<uint32_t>()) { fail("Manifest numeric fields missing or invalid"); return false; }
  Manifest m;
  m.schema=json["schema"];m.size=json["size"];m.flashBytes=json["flash_bytes"];m.psramBytes=json["psram_bytes"];
  m.board=json["board"] | "";m.layout=json["layout"] | "";m.appCompat=json["app_compat"] | "";
  m.version=json["version"] | "";m.sourceSha=json["source_sha"] | "";m.asset=json["asset"] | "";m.sha256=json["sha256"] | "";m.channel=json["channel"] | "";
  if(const char *error=validate(m,ESPSOMFY_APP_COMPAT)) { fail(error);return false; }
  if(m.version!=tag || m.size!=firmwareSize || firmwareDigest.substr(7)!=m.sha256) { fail("Manifest differs from authenticated GitHub release");return false; }
  candidate=m;
  const bool update=newer(m.version.c_str(),ESPSOMFY_RELEASE_TAG);
  portENTER_CRITICAL(&lock);
  snapshot.compatible=true;
  snprintf(snapshot.available,sizeof(snapshot.available),"%s",m.version.c_str());
  portEXIT_CRITICAL(&lock);
  state(update ? "available" : "idle");
  return true;
}
const char *environmentError() {
#if !defined(CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE) || !CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE
  return "Framework has no boot rollback support";
#else
  if(ESP.getFlashChipSize()!=FLASH_BYTES || esp_psram_get_size()<PSRAM_BYTES) return "Exact-board flash/PSRAM requirements are not met";
  const esp_partition_t *running=esp_ota_get_running_partition();
  const esp_partition_t *next=esp_ota_get_next_update_partition(nullptr);
  if(!running || !next || running==next || running->size!=APP_BYTES || next->size!=APP_BYTES ||
    !((running->address==0x10000 && next->address==0x340000) || (running->address==0x340000 && next->address==0x10000))) return "Running partition layout differs from default_8MB dual-slot layout";
  const esp_partition_t *fs=esp_partition_find_first(ESP_PARTITION_TYPE_DATA,ESP_PARTITION_SUBTYPE_DATA_SPIFFS,"spiffs");
  const esp_partition_t *nvs=esp_partition_find_first(ESP_PARTITION_TYPE_DATA,ESP_PARTITION_SUBTYPE_DATA_NVS,"nvs");
  if(!fs || fs->address!=0x670000 || fs->size!=0x180000 || !nvs || nvs->address!=0x9000 || nvs->size!=0x5000) return "Filesystem or NVS partition differs from supported layout";
  esp_ota_img_states_t imageState;
  if(esp_ota_get_state_partition(running,&imageState)==ESP_OK && imageState==ESP_OTA_IMG_PENDING_VERIFY) return "Current firmware is awaiting health confirmation";
  return nullptr;
#endif
}
bool install(uint32_t start) {
  // Refresh authenticated release metadata on every install attempt. Stale UI
  // state cannot select an asset replaced since the preceding check.
  if(!check(start)) return false;
  if(!newer(candidate.version.c_str(),ESPSOMFY_RELEASE_TAG)) { fail("Release is not newer than the installed stable identity");return false; }
  if(const char *error=environmentError()) { fail(error);return false; }
  Buffer firmware(candidate.size);
  if(!firmware.data) { fail("No contiguous PSRAM block for complete firmware staging");return false; }
  state("downloading");
  size_t bytes=0;
  if(!download(assetUrl(candidate.version,FIRMWARE_ASSET),firmware,bytes,start,true)) return false;
  state("verifying");
  if(bytes!=candidate.size || !digestMatches(firmware.data,bytes,candidate.sha256)) { fail("Firmware is truncated or has an invalid SHA-256 digest");return false; }
  if(stopped(start)) { fail("Update cancelled or timed out before image validation");return false; }
  esp_image_header_t header;
  memcpy(&header,firmware.data,sizeof(header));
  if(header.magic!=ESP_IMAGE_HEADER_MAGIC || header.chip_id!=ESP_CHIP_ID_ESP32S3 || !hasIdentity(firmware.data,bytes,candidate)) { fail("Firmware chip or embedded exact-board build identity differs");return false; }
  if(const char *error=environmentError()) { fail(error);return false; }
  // loop() persists controller state and quiesces only after verification.
  preparation.store(1);
  const uint32_t waitStart=millis();
  while((preparation.load()==1 || preparation.load()==4) && !stopped(start) && uint32_t(millis()-waitStart)<10000) vTaskDelay(pdMS_TO_TICKS(10));
  // A timed-out request must not let loop start quiescing afterwards. If loop
  // already owns preparation, retain the operation gate until it returns.
  unsigned pending=1;
  preparation.compare_exchange_strong(pending,3);
  if(preparation.load()!=2 || stopped(start)) { fail("Controller is busy, cancelled, or could not prepare installation");return false; }
  state("installing");
  const esp_partition_t *partition=esp_ota_get_next_update_partition(nullptr);
  esp_ota_handle_t ota=0;
  if(!partition || esp_ota_begin(partition,bytes,&ota)!=ESP_OK) { fail("Unable to prepare inactive firmware slot");return false; }
  // Keep the flash writer's source and stack internal when cache is disabled.
  std::unique_ptr<uint8_t,decltype(&heap_caps_free)> chunk(static_cast<uint8_t *>(heap_caps_malloc(CHUNK,MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT)),heap_caps_free);
  if(!chunk) { esp_ota_abort(ota);fail("Insufficient internal memory for flash writer");return false; }
  for(size_t offset=0;offset<bytes;) {
    if(stopped(start)) { esp_ota_abort(ota);fail("Installation cancelled or timed out; previous slot retained");return false; }
    const size_t n=(bytes-offset)>CHUNK ? CHUNK : bytes-offset;
    memcpy(chunk.get(),firmware.data+offset,n);
    if(esp_ota_write(ota,chunk.get(),n)!=ESP_OK) { esp_ota_abort(ota);fail("Inactive firmware write failed; previous slot retained");return false; }
    offset+=n;
    progress(offset,bytes);
    vTaskDelay(1);
  }
  if(esp_ota_end(ota)!=ESP_OK) { fail("ESP image verification failed; previous slot retained");return false; }
  if(stopped(start)) { fail("Update cancelled before activation; previous slot retained");return false; }
  if(esp_ota_set_boot_partition(partition)!=ESP_OK) { fail("Could not activate verified inactive slot");return false; }
  state("rebooting");
  return true;
}
void worker(void *) {
  for(;;) {
    uint32_t command=0;
    xTaskNotifyWait(0,UINT32_MAX,&command,portMAX_DELAY);
    const uint32_t start=millis();
    // NTP/bootstrap is not weakened to make HTTPS connect. The user configures
    // NTP normally; its wall clock remains subject to the existing network's
    // trust. TLS itself enforces certificate validity and hostname afterwards.
    if(time(nullptr)<1735689600) fail("Clock is unset; wait for configured NTP before checking GitHub");
    else if(!gitUpdateMemoryReady()) fail("PSRAM TLS allocator is unavailable");
    else if(const char *error=environmentError()) fail(error);
    else if(command==1) check(start);
    else if(command==2) install(start);
    // If loop quiesced RF/state, every terminal outcome requires reboot to
    // resume it. Keep the ownership gate held until that reboot.
    if(preparation.load()==2 || preparation.load()==4) operationDone.store(true);
    else {
      preparation.store(0);
      owner.store(0);
    }
  }
}
}
void GitUpdateClass::begin(bool (*callback)()) {
  if(task) return;
  prepareCallback=callback;
  // ESP-IDF xTaskCreate allocates an internal task stack even when the large
  // TLS buffers and complete firmware image live in external PSRAM.
  if(xTaskCreatePinnedToCore(worker,"github-update",12288,nullptr,1,&task,0)!=pdPASS) state("error","Unable to start bounded GitHub update worker");
}
void GitUpdateClass::loop() {
  if(owner.load()==1) measure();
  unsigned pending=1;
  if(preparation.compare_exchange_strong(pending,4)) {
    if(cancelled.load()) preparation.store(3);
    else {
      const bool ready=prepareCallback && prepareCallback();
      if(ready) quiesced=true;
      preparation.store(ready ? 2 : 3);
    }
  }
  if(operationDone.exchange(false)) {
    if(quiesced) {
      rebootDelay.rebootTime=millis()+1500;
      rebootDelay.reboot=true;
    }
    else { preparation.store(0);owner.store(0); }
  }
}
bool GitUpdateClass::requestCheck() {
  if(!task || quiesced) return false;
  unsigned expected=0;
  if(!owner.compare_exchange_strong(expected,1)) return false;
  cancelled.store(false);preparation.store(0);
  state("checking");
  if(xTaskNotify(task,1,eSetValueWithOverwrite)==pdPASS) return true;
  owner.store(0);state("error","Unable to notify update worker");return false;
}
bool GitUpdateClass::requestInstall() {
  if(!task || quiesced) return false;
  unsigned expected=0;
  if(!owner.compare_exchange_strong(expected,1)) return false;
  Snapshot copy;
  portENTER_CRITICAL(&lock);copy=snapshot;portEXIT_CRITICAL(&lock);
  if(!copy.compatible || !newer(copy.available,ESPSOMFY_RELEASE_TAG)) { owner.store(0);return false; }
  cancelled.store(false);preparation.store(0);
  state("checking");
  if(xTaskNotify(task,2,eSetValueWithOverwrite)==pdPASS) return true;
  owner.store(0);state("error","Unable to notify update worker");return false;
}
bool GitUpdateClass::requestCancel() {
  if(owner.load()!=1) return false;
  Snapshot copy;
  portENTER_CRITICAL(&lock);copy=snapshot;portEXIT_CRITICAL(&lock);
  if(strcmp(copy.state,"rebooting")==0) return false;
  cancelled.store(true);
  return true;
}
void GitUpdateClass::status(JsonObject out) const {
  Snapshot copy;
  portENTER_CRITICAL(&lock);copy=snapshot;portEXIT_CRITICAL(&lock);
  out["supported"]=true;
  out["state"]=copy.state;
  out["installed"]=ESPSOMFY_RELEASE_TAG;
  out["available"]=copy.available;
  out["source"]=REPOSITORY;
  out["identity"]=gitUpdateIdentity();
  out["compatible"]=copy.compatible;
  out["busy"]=busy();
  out["progress"]=copy.progress;
  out["error"]=copy.error;
  out["canInstall"]=!busy() && task && !quiesced && copy.compatible && newer(copy.available,ESPSOMFY_RELEASE_TAG) && !environmentError();
  out["minInternalFree"]=copy.minInternal;
  out["minLargestInternalBlock"]=copy.minLargestInternal;
  out["maxPsramUsed"]=copy.maxPsramUsed;
  const auto memory=gitUpdateMemoryStats();
  out["tlsPsramCurrent"]=memory.psramCurrent;
  out["tlsPsramPeak"]=memory.psramPeak;
  out["tlsPsramAllocations"]=memory.psramAllocations;
  out["tlsAllocationFailures"]=memory.failures;
}
#else
void GitUpdateClass::begin(bool (*)()) {}
void GitUpdateClass::loop() {}
bool GitUpdateClass::requestCheck() { return false; }
bool GitUpdateClass::requestInstall() { return false; }
bool GitUpdateClass::requestCancel() { return false; }
void GitUpdateClass::status(JsonObject out) const {
  out["supported"]=false;out["state"]="unsupported";out["canInstall"]=false;
  out["compatible"]=false;out["busy"]=busy();out["progress"]=0;
  out["error"]="GitHub updates require the exact LILYGO Plus PSRAM/dual-slot profile";
}
#endif
