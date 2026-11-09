#include "Arduino.h"
#include "WifiClientScan.h"
#include "GitUpdate.h"
#include <assert.h>
#include <string.h>
#include <limits.h>
struct ESPNetwork {bool softAPOpened=false,joining=false;bool connecting()const{return joining;}void scanForClients();};
static bool _apScanning=false;
static constexpr int WIFI_SCAN_RUNNING=-1,WIFI_SCAN_FAILED=-2;
static int64_t esp_timer_get_time(){return 100000;}
struct {
  int completion=-2,startResult=-1;unsigned starts=0,physicalStarts=0,deletes=0,disconnects=0,probes=0;bool foreignOnSecondProbe=false;String ssid="Office";uint8_t mac[6]={1,2,3,4,5,6};
  void disconnect(bool off){assert(!off);++disconnects;}
  int scanNetworks(bool asynchronous,bool hidden){assert(asynchronous&&hidden);++starts;
    // Match pinned SDK: RUNNING may belong to somebody else; it is not a
    // receipt for a new physical scan. Starting with cold results deletes them.
    if(completion==WIFI_SCAN_RUNNING)return WIFI_SCAN_RUNNING;
    scanDelete();if(startResult==WIFI_SCAN_RUNNING){++physicalStarts;completion=WIFI_SCAN_RUNNING;}return startResult;}
  int scanComplete(){if(foreignOnSecondProbe && ++probes==2)completion=WIFI_SCAN_RUNNING;return completion;}
  void scanDelete(){++deletes;completion=WIFI_SCAN_FAILED;}
  bool getNetworkInfo(uint8_t index,String &name,uint8_t &security,int32_t &rssi,uint8_t *&bssid,int32_t &channel) {
    assert(index<2);name=ssid;security=3;rssi=-52;bssid=mac;channel=6;return true;
  }
} WiFi;
struct {unsigned sends=0;int count=0;void wifiNetworks(int n){++sends;count=n;}}protoSocket;
#define ESP_LOGW(...) ((void)0)
#define ESP_LOGI(...) ((void)0)
#include "production-wifi-scan.inc"
struct Sink {
  unsigned calls=0;bool ready=false;uint64_t token=0;
  static void complete(void *context,uint64_t token,bool success,const WifiScanReader &reader) {
    auto &sink=*static_cast<Sink*>(context);++sink.calls;sink.ready=success;sink.token=token;
    if(success){assert(reader.count()==2);WifiScanNetwork row;assert(reader.read(0,row));assert(!strcmp(row.ssid,"Office") && row.rssi==-52 && row.channel==6);assert(!reader.read(2,row));}
  }
};
int main() {
  ESPNetwork net;net.softAPOpened=true;
  Sink foreign;auto foreignToken=wifiScanFeed().request(Sink::complete,&foreign);
  WiFi.completion=WIFI_SCAN_RUNNING;clientScanLoop(net);
  assert(!WiFi.starts && !WiFi.physicalStarts && !WiFi.disconnects && !WiFi.deletes && !gitUpdate.busy() && !foreign.calls);
  WiFi.completion=2;clientScanLoop(net);assert(!WiFi.starts && !WiFi.deletes && !foreign.calls);
  assert(wifiScanFeed().cancel(foreignToken));WiFi.scanDelete(); // Foreign owner cleans its own results.
  const auto initialDeletes=WiFi.deletes;net.scanForClients();
  _apScanning=true;clientScanLoop(net);assert(!WiFi.starts);_apScanning=false;
  GitUpdateOwnerToken manual;assert(gitUpdate.acquireExternal(GitUpdateExternalOwner::manual,manual));clientScanLoop(net);assert(!WiFi.starts);assert(gitUpdate.releaseExternal(manual));
  clientScanLoop(net);assert(WiFi.starts==1 && WiFi.disconnects==1 && WiFi.deletes==initialDeletes+1 && gitUpdate.busy());
  Sink sink;auto token=wifiScanFeed().request(Sink::complete,&sink);
  WiFi.completion=2;clientScanLoop(net);assert(!sink.calls && wifiScanFeed().pending() && !gitUpdate.busy());
  clientScanLoop(net);assert(WiFi.starts==2 && gitUpdate.busy());WiFi.completion=2;clientScanLoop(net);
  assert(sink.calls==1 && sink.ready && sink.token==token && !gitUpdate.busy() && WiFi.deletes==initialDeletes+4);
  // Real SDK adapter bounds SSID and indices before a local copy can be used.
  WiFi.completion=2;ClientScanDriver driver(net);assert(driver.result()==2);WifiScanNetwork row;
  WiFi.ssid=String("123456789012345678901234567890123");assert(!driver.read(0,row));WiFi.ssid=String("Office");assert(!driver.read(256,row));
  WiFi.scanDelete();WiFi.startResult=-2;token=wifiScanFeed().request(Sink::complete,&sink);const auto cleared=WiFi.deletes;
  clientScanLoop(net);assert(sink.calls==2 && !sink.ready && sink.token==token && WiFi.deletes==cleared+1 && !gitUpdate.busy());
  const auto apiBefore=WiFi.starts,physicalBefore=WiFi.physicalStarts,disconnectBefore=WiFi.disconnects,deleteBefore=WiFi.deletes;
  WiFi.startResult=-1;WiFi.completion=-2;WiFi.foreignOnSecondProbe=true;WiFi.probes=0;
  token=wifiScanFeed().request(Sink::complete,&sink);clientScanLoop(net);
  assert(sink.calls==3 && !sink.ready && sink.token==token && !gitUpdate.busy());
  assert(WiFi.starts==apiBefore && WiFi.physicalStarts==physicalBefore && WiFi.disconnects==disconnectBefore && WiFi.deletes==deleteBefore);
  puts("Actual network scan adapter: ownership, exact local source, bounded rows and failed-start cleanup passed");
}
