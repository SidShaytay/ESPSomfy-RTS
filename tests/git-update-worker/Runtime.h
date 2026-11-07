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

#define CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE 1
#define ESPSOMFY_RELEASE_TAG "v4.0.2"
#define ESPSOMFY_APP_COMPAT "4.0.2"
#define ESPSOMFY_SOURCE_EPOCH 1
constexpr int ESP_OK=0,ESP_FAIL=-1;
constexpr int pdPASS=1;
constexpr int portMAX_DELAY=0x7fffffff;
#define pdMS_TO_TICKS(v) (v)
using esp_err_t=int;
using TaskHandle_t=void *;
using portMUX_TYPE=int;
#define portMUX_INITIALIZER_UNLOCKED 0
#define portENTER_CRITICAL(x) ((void)(x))
#define portEXIT_CRITICAL(x) ((void)(x))
constexpr int MALLOC_CAP_SPIRAM=1,MALLOC_CAP_INTERNAL=2,MALLOC_CAP_8BIT=4;
struct FakeTaskExit {};
namespace Fake {
inline uint32_t now=1000;
inline time_t wallClock=1760000000;
inline uint32_t notified=0;
inline bool queued=false;
inline bool memoryReady=true,fsCompatible=true,bootloaderMatches=true;
inline bool failBegin=false,failWrite=false,failEnd=false,failActivate=false,allocationFail=false;
inline bool cancelledDuringPrepare=false,cancelDuringWrite=false,cancelAcceptedDuringWrite=false;
inline bool autoLoop=true;
inline unsigned prepareCalls=0,otaBegins=0,otaWrites=0,otaEnds=0,otaActivations=0,otaAborts=0;
inline uint32_t otaAddress=0;
inline std::function<void()> onYield;
inline std::function<void()> onRead;
inline std::vector<uint8_t> written;
inline size_t psramPhysical=8388608;
inline std::map<std::string,std::string> seenAccept;
struct Response {
  int status=200;
  std::string body;
  int64_t length=-2; // -2 => actual body length
  bool chunked=false,connection=true,clean=true;
  std::string location,encoding;
  size_t readLimit=4096,truncateAt=SIZE_MAX;
  bool requireJsonAccept=false;
};
inline std::map<std::string,Response> responses;
inline std::vector<std::string> urls;
}
extern "C" time_t __wrap_time(time_t *out) { if(out) *out=Fake::wallClock;return Fake::wallClock; }
inline uint32_t millis() { return Fake::now; }
inline void *heap_caps_malloc(size_t bytes,int) { return Fake::allocationFail ? nullptr : std::malloc(bytes); }
inline void *heap_caps_realloc(void *p,size_t bytes,int) { return std::realloc(p,bytes); }
inline void heap_caps_free(void *p) { std::free(p); }
inline size_t heap_caps_get_free_size(int) { return 100000; }
inline size_t heap_caps_get_largest_free_block(int) { return 80000; }
inline size_t esp_psram_get_size() { return Fake::psramPhysical; }
struct FakeEsp {
  uint32_t getPsramSize() const { return 8300000; }
  uint32_t getFreePsram() const { return 7000000; }
  uint32_t getFlashChipSize() const { return 16777216; }
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
struct rebootDelay_t { bool reboot=false;uint32_t rebootTime=0; };
struct GitUpdateMemoryStats { size_t psramCurrent,psramPeak,psramAllocations,failures; };
inline bool gitUpdateMemoryReady() { return Fake::memoryReady; }
inline GitUpdateMemoryStats gitUpdateMemoryStats() { return {1,2,3,0}; }
inline const char *gitUpdateIdentity() { return "test-running-identity"; }
inline bool gitUpdateAssetsMatch() { return Fake::fsCompatible; }
inline const char *gitUpdateBootloaderDigest() { return "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"; }
inline bool gitUpdateBootloaderMatches() { return Fake::bootloaderMatches; }
inline esp_err_t esp_crt_bundle_attach(void *) { return ESP_OK; }

constexpr int ESP_PARTITION_TYPE_DATA=1,ESP_PARTITION_SUBTYPE_DATA_SPIFFS=2,ESP_PARTITION_SUBTYPE_DATA_NVS=3;
struct esp_partition_t { uint32_t address;uint32_t size; };
inline esp_partition_t runningPartition{0x10000,0x330000},nextPartition{0x340000,0x330000},fsPartition{0x670000,0x180000},nvsPartition{0x9000,0x5000};
enum esp_ota_img_states_t { ESP_OTA_IMG_VALID=0,ESP_OTA_IMG_PENDING_VERIFY=1,ESP_OTA_IMG_UNDEFINED=2 };
inline esp_ota_img_states_t fakeImageState=ESP_OTA_IMG_VALID;
inline const esp_partition_t *esp_ota_get_running_partition() { return &runningPartition; }
inline const esp_partition_t *esp_ota_get_next_update_partition(void *) { return &nextPartition; }
inline const esp_partition_t *esp_partition_find_first(int,int subtype,const char *) { return subtype==ESP_PARTITION_SUBTYPE_DATA_SPIFFS ? &fsPartition : &nvsPartition; }
inline esp_err_t esp_ota_get_state_partition(const esp_partition_t *,esp_ota_img_states_t *state) { *state=fakeImageState;return ESP_OK; }
using esp_ota_handle_t=uint32_t;
inline esp_err_t esp_ota_begin(const esp_partition_t *partition,size_t,esp_ota_handle_t *handle) {
  Fake::otaBegins++;Fake::otaAddress=partition->address;*handle=1;return Fake::failBegin ? ESP_FAIL : ESP_OK;
}
inline esp_err_t esp_ota_write(esp_ota_handle_t,const void *data,size_t bytes) {
  Fake::otaWrites++;
  if(Fake::cancelDuringWrite) Fake::cancelAcceptedDuringWrite=gitUpdate.requestCancel();
  if(Fake::failWrite) return ESP_FAIL;
  const auto *p=static_cast<const uint8_t *>(data);Fake::written.insert(Fake::written.end(),p,p+bytes);return ESP_OK;
}
inline esp_err_t esp_ota_abort(esp_ota_handle_t) { Fake::otaAborts++;return ESP_OK; }
inline esp_err_t esp_ota_end(esp_ota_handle_t) { Fake::otaEnds++;return Fake::failEnd ? ESP_FAIL : ESP_OK; }
inline esp_err_t esp_ota_set_boot_partition(const esp_partition_t *) { Fake::otaActivations++;return Fake::failActivate ? ESP_FAIL : ESP_OK; }
struct esp_image_header_t { uint8_t magic;uint8_t rest[11];uint16_t chip_id;uint8_t tail[10]; } __attribute__((packed));
constexpr uint8_t ESP_IMAGE_HEADER_MAGIC=0xe9;
constexpr uint16_t ESP_CHIP_ID_ESP32S3=9;
// Tests use OpenSSL's actual SHA-256; this is a cryptographic boundary adapter,
// never a canned digest matching whichever image is under test.
extern "C" unsigned char *SHA256(const unsigned char *,size_t,unsigned char *);
inline int mbedtls_sha256(const unsigned char *data,size_t bytes,unsigned char *out,int) { return SHA256(data,bytes,out) ? 0 : -1; }

constexpr int HTTP_EVENT_ON_HEADER=1,HTTP_TRANSPORT_OVER_SSL=2;
struct esp_http_client_event_t { int event_id;void *user_data;char *header_key;char *header_value; };
struct esp_http_client_config_t {
  const char *url=nullptr;int timeout_ms=0;bool disable_auto_redirect=false;int max_authorization_retries=0;
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
};
using esp_http_client_handle_t=FakeClient *;
inline esp_http_client_handle_t esp_http_client_init(const esp_http_client_config_t *config) {
  assert(config->crt_bundle_attach==esp_crt_bundle_attach);
  assert(!config->skip_cert_common_name_check);
  assert(config->disable_auto_redirect);
  assert(config->transport_type==HTTP_TRANSPORT_OVER_SSL);
  auto *client=new FakeClient;client->config=*config;client->url=config->url;Fake::urls.push_back(client->url);
  auto found=Fake::responses.find(client->url);
  if(found==Fake::responses.end()) client->response.status=404;
  else client->response=found->second;
  return client;
}
inline esp_err_t esp_http_client_cleanup(esp_http_client_handle_t client) { delete client;return ESP_OK; }
inline esp_err_t esp_http_client_set_header(esp_http_client_handle_t c,const char *key,const char *value) {
  if(std::string(key)=="Accept") { c->accept=value;Fake::seenAccept[c->url]=value; } return ESP_OK;
}
inline esp_err_t esp_http_client_open(esp_http_client_handle_t c,int) {
  if(c->response.requireJsonAccept && c->accept!="application/vnd.github+json") c->response.status=415;
  Fake::now+=10;return c->response.connection ? ESP_OK : ESP_FAIL;
}
inline int64_t esp_http_client_fetch_headers(esp_http_client_handle_t c) {
  auto header=[&](const char *name,std::string &value) {
    if(value.empty()) return;
    esp_http_client_event_t event{HTTP_EVENT_ON_HEADER,c->config.user_data,const_cast<char *>(name),value.data()};c->config.event_handler(&event);
  };
  header("Location",c->response.location);header("Content-Encoding",c->response.encoding);
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
inline int xTaskCreatePinnedToCore(void (*)(void *),const char *,uint32_t,void *,unsigned,TaskHandle_t *task,int) { *task=reinterpret_cast<void *>(1);return pdPASS; }
inline int xTaskNotify(TaskHandle_t,uint32_t command,int) { Fake::notified=command;Fake::queued=true;return pdPASS; }
inline int xTaskNotifyWait(uint32_t,uint32_t,uint32_t *command,int) {
  if(!Fake::queued) throw FakeTaskExit{};
  *command=Fake::notified;Fake::queued=false;return pdPASS;
}
inline void vTaskDelay(unsigned ticks) {
  Fake::now+=ticks;
  if(Fake::onYield) Fake::onYield();
  if(Fake::autoLoop) gitUpdate.loop();
}
