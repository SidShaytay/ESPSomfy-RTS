#include <assert.h>
#include <stdint.h>
#include <stdio.h>
static unsigned long uptime=0;
static unsigned validations=0;
static bool otaPending=true,routes=true;
static constexpr unsigned long OTA_PROVEN_MS=60000;
static constexpr int ESP_OK=0;
struct { bool online=true;bool connected()const{return online;} } net;
struct { struct { bool trusted=true;bool usable()const{return trusted;} } modelLoad; } somfy;
struct { struct { bool trusted=false;bool loadTrusted()const{return trusted;} } Security; } settings;
unsigned long millis(){return uptime;}
bool bundledWebRoutesReady(){return routes;}
int esp_ota_mark_app_valid_cancel_rollback(){++validations;return ESP_OK;}
#define ESP_LOGI(...) ((void)0)
#define ESP_LOGE(...) ((void)0)
#define ESPSOMFY_BUNDLED_WEB
#include "production-security-health.inc"
int main(){
 uptime=60000;health();assert(otaPending && !validations);
 uptime=3600000;health();assert(otaPending && !validations);
 settings.Security.trusted=true;somfy.modelLoad.trusted=false;health();assert(otaPending && !validations);
 somfy.modelLoad.trusted=true;net.online=false;health();assert(otaPending && !validations);
 net.online=true;routes=false;health();assert(otaPending && !validations);
 routes=true;uptime=59999;health();assert(otaPending && !validations);
 uptime=60000;health();assert(!otaPending && validations==1);
 puts("Actual OTA confirmation block requires trusted model and security loads, network, bundled routes and minimum uptime");
}
