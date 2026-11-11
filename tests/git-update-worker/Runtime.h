#pragma once
// A deterministic SDK boundary for the real production GitUpdate.cpp. No
// controller, socket, RF, DNS, or external HTTP access is possible in this test.
#include <ArduinoJson.h>
#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <functional>
#include <map>
#include <string>
#include <vector>
#include <stdexcept>
#include <strings.h>
#include "GitUpdateMemory.h"
#include "GitUpdateAttestation.h"
#include "GitUpdateCertificate.h"

#ifdef ESPSOMFY_STACK_AUDIT
namespace StackAudit { inline void observeCurrent() {} }
#endif
#define CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE 1
#ifndef ESPSOMFY_RELEASE_TAG
#define ESPSOMFY_RELEASE_TAG "v4.0.2"
#endif
#define ESPSOMFY_APP_COMPAT "sha256:cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
#define ESPSOMFY_SOURCE_EPOCH 1760000000
constexpr int pdPASS=1,pdTRUE=1;
constexpr int portMAX_DELAY=0x7fffffff;
#define pdMS_TO_TICKS(v) (v)
using esp_err_t=int;
using TaskHandle_t=void *;
using portMUX_TYPE=int;
#define portMUX_INITIALIZER_UNLOCKED 0
#define portENTER_CRITICAL(x) ((void)(x),++Fake::criticalDepth)
#define portEXIT_CRITICAL(x) ((void)(x),--Fake::criticalDepth)
constexpr int MALLOC_CAP_SPIRAM=1,MALLOC_CAP_INTERNAL=2,MALLOC_CAP_8BIT=4;
struct FakeTaskExit {};
namespace Fake {
inline unsigned criticalDepth=0;
inline uint32_t now=1000;
inline uint32_t randomWord=0x12345678;
inline unsigned randomCalls=0;
inline time_t wallClock=1760000000;
inline size_t activeAllocations=0,allocationCalls=0,activeClients=0;
inline int failAllocationAfter=-1;
inline unsigned attestationCalls=0;
inline uint32_t notified=0;
inline bool queued=false;
inline unsigned notificationTimeouts=0,observedNotificationTimeouts=0;
inline bool checkNoWorkAfterTimeout=false;inline size_t timeoutUrlCount=0;
inline bool memoryReady=true,fsCompatible=true,bootloaderMatches=true;
inline bool createFail=false,notifyFail=false;
inline unsigned createdTasks=0;
inline unsigned bundleCalls=0;
inline int bundleResult=0;
inline uint32_t bundleFlags=0;
inline bool expiredCertificate=false,futureCertificate=false,expiredIntermediate=false,missingVerify=false;
inline bool failBegin=false,failWrite=false,failEnd=false,failActivate=false,allocationFail=false;
inline bool cancelledDuringPrepare=false,cancelDuringWrite=false,cancelAcceptedDuringWrite=false;
inline bool autoLoop=true,inFlash=false,failAbort=false,wrongBootReadback=false;
inline std::function<void()> onWrite,onBegin,onEnd;
inline std::map<void*,int> allocationCaps;
inline bool extraAppPartition=false,extraOtaPartition=false;
inline unsigned prepareCalls=0,otaBegins=0,otaWrites=0,otaEnds=0,otaActivations=0,otaAborts=0;
inline uint32_t otaAddress=0;
inline std::function<void()> onYield;
inline std::function<void()> onRead;
inline std::vector<uint8_t> written;
inline size_t psramPhysical=8388608;
inline size_t flashPhysical=GitUpdateProfile::flashBytes;
inline size_t internalFree=100000,internalLargest=80000;
inline std::map<std::string,std::string> seenAccept;
struct Response {
  int status=200;
  int openError=0,socketError=0,tlsSdkError=0,tlsError=0,tlsFlags=0;
  std::string body;
  int64_t length=-2; // -2 => actual body length
  bool chunked=false,connection=true,clean=true;
  unsigned temporaryOpenFailures=0,temporaryStatusFailures=0,temporaryTruncations=0,temporaryHeaderFailures=0;
  int temporaryStatusCode=503;std::string retryAfter,rateRemaining,rateReset;
  std::string location,encoding;
  bool duplicateLocation=false;
  size_t readLimit=4096,truncateAt=SIZE_MAX;
  bool requireJsonAccept=false;
};
inline std::map<std::string,Response> responses;
inline std::vector<std::string> urls;
}
extern "C" time_t __wrap_time(time_t *out) { if(out) *out=Fake::wallClock;return Fake::wallClock; }
inline uint32_t esp_random() { assert(Fake::criticalDepth==0);++Fake::randomCalls;return Fake::randomWord; }
inline uint32_t millis() { return Fake::now; }
inline int64_t esp_timer_get_time(){return int64_t(Fake::now)*1000;}
inline void *heap_caps_malloc(size_t bytes,int caps) {
  assert(Fake::criticalDepth==0);
  assert(caps==(MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT) || (caps==(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT) && Fake::prepareCalls));
  if(Fake::allocationFail || (Fake::failAllocationAfter>=0 && int(Fake::allocationCalls++)>=Fake::failAllocationAfter)) return nullptr;
  void *p=std::malloc(bytes);if(p){++Fake::activeAllocations;Fake::allocationCaps[p]=caps;}return p;
}
inline void *heap_caps_aligned_alloc(size_t alignment,size_t bytes,int caps) {
  assert(Fake::criticalDepth==0);
  assert(caps==(MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));
  if(Fake::allocationFail || (Fake::failAllocationAfter>=0 && int(Fake::allocationCalls++)>=Fake::failAllocationAfter)) return nullptr;
  void *p=nullptr;if(posix_memalign(&p,alignment,bytes)!=0) return nullptr;
  ++Fake::activeAllocations;Fake::allocationCaps[p]=caps;return p;
}
inline void *heap_caps_realloc(void *p,size_t bytes,int) { return std::realloc(p,bytes); }
inline bool esp_ptr_external_ram(const void *p){const auto found=Fake::allocationCaps.find(const_cast<void*>(p));return found!=Fake::allocationCaps.end() && found->second==(MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);}
inline void heap_caps_free(void *p) { assert(Fake::criticalDepth==0);if(p) {assert(Fake::activeAllocations);--Fake::activeAllocations;Fake::allocationCaps.erase(p);}std::free(p); }
inline size_t heap_caps_get_free_size(int) { return Fake::internalFree; }
inline size_t heap_caps_get_minimum_free_size(int) { return 70000; }
inline size_t heap_caps_get_largest_free_block(int) { return Fake::internalLargest; }
inline size_t esp_psram_get_size() { return Fake::psramPhysical; }
struct FakeEsp {
  uint32_t getPsramSize() const { return 8300000; }
  uint32_t getFreePsram() const { return 7000000; }
  uint32_t getFlashChipSize() const { return Fake::flashPhysical; }
};
inline FakeEsp ESP;
struct String {
  std::string value;
  explicit String(std::string v="") : value(std::move(v)) {}
  void trim() {
    auto start=value.find_first_not_of(" \r\n\t");
    if(start==std::string::npos) { value.clear();return; }
    auto end=value.find_last_not_of(" \r\n\t");value=value.substr(start,end-start+1);
  }
  bool operator==(const char *s) const { return value==s; }
};
struct File {
  explicit operator bool() const { return true; }
  size_t size() const { return 6; }
  String readString() const { return String(Fake::fsCompatible ? "4.0.2\n" : "4.0.1\n"); }
  void close() {}
};
struct FakeLittleFS { File open(const char *,const char *) { return {}; } };
inline FakeLittleFS LittleFS;
inline bool gitUpdateMemoryReady() { return Fake::memoryReady; }
inline GitUpdateMemoryStats gitUpdateMemoryStats() { return {1,2,0,0,3,0}; }
inline const char *gitUpdateIdentity() { return "test-running-identity"; }
inline UpdateAttestation gitUpdateAssetsStatus(UpdateAttestationControl control={}) {
  if(control.stopped()) return UpdateAttestation::interrupted;
  ++Fake::attestationCalls;control.cooperate();
  if(control.stopped()) return UpdateAttestation::interrupted;
  return Fake::fsCompatible ? UpdateAttestation::verified : UpdateAttestation::missing;
}
inline const char *gitUpdateBootloaderDigest() { return "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"; }
inline const char *gitUpdateBootloaderIdentity() { return "ESPSOMFY_BOOT_ID:0:256:bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"; }
inline UpdateAttestation gitUpdateBootloaderStatus(UpdateAttestationControl control={}) {
  if(control.stopped()) return UpdateAttestation::interrupted;
  ++Fake::attestationCalls;control.cooperate();
  if(control.stopped()) return UpdateAttestation::interrupted;
  return Fake::bootloaderMatches ? UpdateAttestation::verified : UpdateAttestation::mismatch;
}
inline int bundleOpaque=123;
inline int fakeBundleVerify(void *opaque,mbedtls_x509_crt *,int,uint32_t *flags) {
  assert(opaque==&bundleOpaque);++Fake::bundleCalls;
  *flags&=~MBEDTLS_X509_BADCERT_NOT_TRUSTED;*flags|=Fake::bundleFlags;return Fake::bundleResult;
}
esp_err_t esp_crt_bundle_attach(void *configuration) {
  mbedtls_ssl_conf_verify(static_cast<mbedtls_ssl_config *>(configuration),fakeBundleVerify,&bundleOpaque);return ESP_OK;
}

constexpr int ESP_PARTITION_TYPE_ANY=255,ESP_PARTITION_TYPE_APP=0,ESP_PARTITION_TYPE_DATA=1,ESP_PARTITION_SUBTYPE_DATA_SPIFFS=0x82,ESP_PARTITION_SUBTYPE_DATA_NVS=2;
constexpr int ESP_PARTITION_SUBTYPE_DATA_COREDUMP=3;
constexpr int ESP_PARTITION_SUBTYPE_APP_OTA_0=16,ESP_PARTITION_SUBTYPE_APP_OTA_1=17,ESP_PARTITION_SUBTYPE_ANY=255,ESP_PARTITION_SUBTYPE_DATA_OTA=0;
struct esp_partition_t { uint32_t address;uint32_t size;int type=ESP_PARTITION_TYPE_APP;int subtype=ESP_PARTITION_SUBTYPE_APP_OTA_0;const char *label="app0"; };
inline esp_partition_t runningPartition{GitUpdateProfile::app0Address,GitUpdateProfile::appBytes},nextPartition{GitUpdateProfile::app1Address,GitUpdateProfile::appBytes,ESP_PARTITION_TYPE_APP,ESP_PARTITION_SUBTYPE_APP_OTA_1,"app1"},
  fsPartition{GitUpdateProfile::fsAddress,GitUpdateProfile::fsBytes,ESP_PARTITION_TYPE_DATA,ESP_PARTITION_SUBTYPE_DATA_SPIFFS,"spiffs"},nvsPartition{GitUpdateProfile::nvsAddress,GitUpdateProfile::nvsBytes,ESP_PARTITION_TYPE_DATA,ESP_PARTITION_SUBTYPE_DATA_NVS,"nvs"},
  otaPartition{0xe000,0x2000,ESP_PARTITION_TYPE_DATA,ESP_PARTITION_SUBTYPE_DATA_OTA,"otadata"},coredumpPartition{0x7f0000,0x10000,ESP_PARTITION_TYPE_DATA,ESP_PARTITION_SUBTYPE_DATA_COREDUMP,"coredump"},extraApp{0xf00000,0x10000,ESP_PARTITION_TYPE_APP,0};
struct FakePartitionIterator { std::vector<const esp_partition_t *> partitions;size_t index=0; };
using esp_partition_iterator_t=FakePartitionIterator *;
inline esp_partition_iterator_t esp_partition_find(int type,int subtype,const char *) {
  auto *iterator=new FakePartitionIterator;
  std::vector<const esp_partition_t *> all{&runningPartition,&nextPartition,&fsPartition,&nvsPartition,&otaPartition,&coredumpPartition};
  if(Fake::extraAppPartition) all.push_back(&extraApp);
  if(Fake::extraOtaPartition) all.push_back(&otaPartition);
  for(const auto *partition:all) if((type==ESP_PARTITION_TYPE_ANY || partition->type==type) && (subtype==ESP_PARTITION_SUBTYPE_ANY || partition->subtype==subtype)) iterator->partitions.push_back(partition);
  if(iterator->partitions.empty()) { delete iterator;return nullptr; }
  return iterator;
}
inline const esp_partition_t *esp_partition_get(esp_partition_iterator_t iterator) { return iterator->partitions[iterator->index]; }
inline void esp_partition_iterator_release(esp_partition_iterator_t iterator) { delete iterator; }
inline esp_partition_iterator_t esp_partition_next(esp_partition_iterator_t iterator) {
  if(++iterator->index<iterator->partitions.size()) return iterator;
  delete iterator;return nullptr;
}
enum esp_ota_img_states_t { ESP_OTA_IMG_VALID=0,ESP_OTA_IMG_PENDING_VERIFY=1,ESP_OTA_IMG_UNDEFINED=2 };
inline esp_ota_img_states_t fakeImageState=ESP_OTA_IMG_VALID;
inline const esp_partition_t *esp_ota_get_running_partition() { return &runningPartition; }
inline const esp_partition_t *esp_ota_get_next_update_partition(void *) { return &nextPartition; }
inline const esp_partition_t *fakeBootPartition=&runningPartition;
inline const esp_partition_t *esp_ota_get_boot_partition(){return Fake::wrongBootReadback?&runningPartition:fakeBootPartition;}
inline const esp_partition_t *esp_partition_find_first(int,int subtype,const char *) { return subtype==ESP_PARTITION_SUBTYPE_DATA_SPIFFS ? &fsPartition : &nvsPartition; }
inline esp_err_t esp_ota_get_state_partition(const esp_partition_t *,esp_ota_img_states_t *state) { *state=fakeImageState;return ESP_OK; }
using esp_ota_handle_t=uint32_t;
inline esp_err_t esp_ota_begin(const esp_partition_t *partition,size_t,esp_ota_handle_t *handle) {
  Fake::otaBegins++;Fake::otaAddress=partition->address;if(Fake::onBegin)Fake::onBegin();if(Fake::failBegin)return ESP_FAIL;*handle=1;return ESP_OK;
}
inline esp_err_t esp_ota_write(esp_ota_handle_t,const void *data,size_t bytes) {
  Fake::otaWrites++;assert(Fake::allocationCaps.at(const_cast<void*>(data))==(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT));
  struct Active{Active(){Fake::inFlash=true;}~Active(){Fake::inFlash=false;}}active;
  if(Fake::onWrite)Fake::onWrite();
  if(Fake::cancelDuringWrite) Fake::cancelAcceptedDuringWrite=gitUpdate.requestCancel();
  if(Fake::failWrite) return ESP_FAIL;
  const auto *p=static_cast<const uint8_t *>(data);Fake::written.insert(Fake::written.end(),p,p+bytes);return ESP_OK;
}
inline esp_err_t esp_ota_abort(esp_ota_handle_t) { Fake::otaAborts++;return Fake::failAbort?ESP_FAIL:ESP_OK; }
inline esp_err_t esp_ota_end(esp_ota_handle_t) { Fake::otaEnds++;if(Fake::onEnd)Fake::onEnd();return Fake::failEnd ? ESP_FAIL : ESP_OK; }
inline esp_err_t esp_ota_set_boot_partition(const esp_partition_t *partition) { Fake::otaActivations++;if(Fake::failActivate)return ESP_FAIL;fakeBootPartition=partition;return ESP_OK; }
struct esp_image_header_t { uint8_t magic;uint8_t rest[11];uint16_t chip_id;uint8_t tail[10]; } __attribute__((packed));
constexpr uint8_t ESP_IMAGE_HEADER_MAGIC=0xe9;
constexpr uint16_t ESP_CHIP_ID_ESP32S3=9;
// Tests use OpenSSL's actual SHA-256; this is a cryptographic boundary adapter,
// never a canned digest matching whichever image is under test.
extern "C" unsigned char *SHA256(const unsigned char *,size_t,unsigned char *);
inline int mbedtls_sha256(const unsigned char *data,size_t bytes,unsigned char *out,int) { return SHA256(data,bytes,out) ? 0 : -1; }

constexpr int HTTP_EVENT_ON_HEADER=1,HTTP_TRANSPORT_OVER_SSL=2,ESP_ERR_HTTP_CONNECT=0x7002;
struct esp_http_client_event_t { int event_id;void *user_data;char *header_key;char *header_value; };
struct esp_http_client_config_t {
  const char *url=nullptr;int timeout_ms=0;bool disable_auto_redirect=false;bool is_async=false;int max_authorization_retries=0;
  esp_err_t (*crt_bundle_attach)(void *)=nullptr;bool skip_cert_common_name_check=false;
  esp_err_t (*event_handler)(esp_http_client_event_t *)=nullptr;void *user_data=nullptr;
  int buffer_size=0,buffer_size_tx=0;const char *user_agent=nullptr;int transport_type=0;
};
struct FakeClient {
  esp_http_client_config_t config;
  std::string url;
  Fake::Response response;
  size_t offset=0;
  std::string accept;
  mbedtls_ssl_config ssl;
};
using esp_http_client_handle_t=FakeClient *;
inline esp_http_client_handle_t esp_http_client_init(const esp_http_client_config_t *config) {
  assert(Fake::criticalDepth==0);
  assert(config->crt_bundle_attach==GitUpdateCertificate::attach && !config->is_async);
  assert(!config->skip_cert_common_name_check);
  assert(config->disable_auto_redirect);
  assert(config->transport_type==HTTP_TRANSPORT_OVER_SSL);
  ++Fake::activeClients;auto *client=new FakeClient;client->config=*config;client->url=config->url;Fake::urls.push_back(client->url);
  auto found=Fake::responses.find(client->url);
  if(found==Fake::responses.end()) client->response.status=404;
  else {client->response=found->second;
    if(found->second.temporaryOpenFailures){--found->second.temporaryOpenFailures;client->response.openError=ESP_ERR_HTTP_CONNECT;client->response.tlsError=-0x7280;}
    if(found->second.temporaryStatusFailures){--found->second.temporaryStatusFailures;client->response.status=found->second.temporaryStatusCode;}
    if(found->second.temporaryHeaderFailures){--found->second.temporaryHeaderFailures;client->response.length=-1;client->response.socketError=104;}
    if(found->second.temporaryTruncations){--found->second.temporaryTruncations;client->response.truncateAt=10;}
  }
  return client;
}
inline esp_err_t esp_http_client_cleanup(esp_http_client_handle_t client) { delete client;--Fake::activeClients;return ESP_OK; }
inline esp_err_t esp_http_client_set_header(esp_http_client_handle_t c,const char *key,const char *value) {
  if(std::string(key)=="Accept") { c->accept=value;Fake::seenAccept[c->url]=value; } return ESP_OK;
}
inline esp_err_t esp_http_client_open(esp_http_client_handle_t c,int) {
  if(c->response.requireJsonAccept && c->accept!="application/vnd.github+json") c->response.status=415;
  Fake::now+=10;
  if(c->response.openError)return c->response.openError;
  if(!c->response.connection || c->config.crt_bundle_attach(&c->ssl)!=ESP_OK) return ESP_FAIL;
  if(Fake::missingVerify) return ESP_OK; // service must reject absent leaf verification
  uint32_t chainFlags=0;
  for(int depth:{1,0}) {
    mbedtls_x509_crt cert{{2020,1,1,0,0,0},{2035,1,1,0,0,0}};
    if(Fake::expiredCertificate || (depth==1 && Fake::expiredIntermediate)) cert.valid_to={2024,1,1,0,0,0};
    if(Fake::futureCertificate) cert.valid_from={2030,1,1,0,0,0};
    uint32_t flags=MBEDTLS_X509_BADCERT_NOT_TRUSTED;
    if(c->ssl.MBEDTLS_PRIVATE(f_vrfy)(c->ssl.MBEDTLS_PRIVATE(p_vrfy),&cert,depth,&flags)!=0) return ESP_FAIL;
    chainFlags|=flags;
  }
  return chainFlags ? ESP_FAIL : ESP_OK;
}
inline int esp_http_client_get_errno(esp_http_client_handle_t c) { assert(Fake::criticalDepth==0);return c->response.socketError; }
inline esp_err_t esp_http_client_get_and_clear_last_tls_error(esp_http_client_handle_t c,int *error,int *flags) {
  assert(Fake::criticalDepth==0);*error=c->response.tlsError;*flags=c->response.tlsFlags;
  const auto result=c->response.tlsSdkError;c->response.tlsError=c->response.tlsFlags=c->response.tlsSdkError=0;return result;
}
inline int64_t esp_http_client_fetch_headers(esp_http_client_handle_t c) {
  auto header=[&](const char *name,std::string &value) {
    if(value.empty()) return;
    esp_http_client_event_t event{HTTP_EVENT_ON_HEADER,c->config.user_data,const_cast<char *>(name),value.data()};c->config.event_handler(&event);
  };
  header("Retry-After",c->response.retryAfter);header("x-ratelimit-remaining",c->response.rateRemaining);header("x-ratelimit-reset",c->response.rateReset);
  header("Location",c->response.location);
  if(c->response.duplicateLocation) header("Location",c->response.location);
  header("Content-Encoding",c->response.encoding);
  return c->response.length==-2 ? int64_t(c->response.body.size()) : c->response.length;
}
inline int esp_http_client_get_status_code(esp_http_client_handle_t c) { return c->response.status; }
inline bool esp_http_client_is_chunked_response(esp_http_client_handle_t c) { return c->response.chunked; }
inline int esp_http_client_read(esp_http_client_handle_t c,char *out,size_t count) {
  if(Fake::onRead) Fake::onRead();
  Fake::now+=10;
  if(c->offset>=c->response.truncateAt) return 0;
  const size_t n=std::min({count,c->response.readLimit,c->response.body.size()-c->offset,c->response.truncateAt-c->offset});
  if(!n) return 0;
  std::memcpy(out,c->response.body.data()+c->offset,n);c->offset+=n;return int(n);
}
inline bool esp_http_client_is_complete_data_received(esp_http_client_handle_t c) { return c->response.clean && c->offset==c->response.body.size(); }
inline unsigned uxTaskGetStackHighWaterMark(TaskHandle_t) { return 8192; }
constexpr int eSetValueWithOverwrite=0;
inline int xTaskCreatePinnedToCore(void (*)(void *),const char *,uint32_t stack,void *,unsigned priority,TaskHandle_t *task,int core) { assert(Fake::criticalDepth==0);assert(stack==12288 && priority==1 && core==0);++Fake::createdTasks;if(Fake::createFail) return ESP_FAIL;*task=reinterpret_cast<void *>(1);return pdPASS; }
inline int xTaskNotify(TaskHandle_t,uint32_t command,int) { assert(Fake::criticalDepth==0);if(Fake::notifyFail) return ESP_FAIL;Fake::notified=command;Fake::queued=true;return pdPASS; }
inline int xTaskNotifyWait(uint32_t,uint32_t,uint32_t *command,int ticks) {
#ifdef ESPSOMFY_STACK_AUDIT
  assert(ticks==5000);
#else
  assert(ticks==portMAX_DELAY);
#endif
  if(Fake::checkNoWorkAfterTimeout){assert(Fake::urls.size()==Fake::timeoutUrlCount);Fake::checkNoWorkAfterTimeout=false;}
  if(Fake::notificationTimeouts){
    --Fake::notificationTimeouts;++Fake::observedNotificationTimeouts;
    *command=5;Fake::checkNoWorkAfterTimeout=true;Fake::timeoutUrlCount=Fake::urls.size();return 0;
  }
  if(!Fake::queued) throw FakeTaskExit{};
  *command=Fake::notified;Fake::queued=false;return pdPASS;
}
inline void vTaskDelay(unsigned ticks) {
  assert(Fake::criticalDepth==0);
  Fake::now+=ticks;
  if(Fake::onYield) Fake::onYield();
  if(Fake::autoLoop) gitUpdate.loop();
}
