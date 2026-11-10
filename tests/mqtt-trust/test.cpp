#include "Arduino.h"
#include "ModelLoadTrust.h"
#include "SettingsLoadState.h"
#include "MqttConnection.h"
#include "ShadeChanges.h"
#include "ShadeTypes.h"
#include <cassert>
#include <string>
#include <vector>
#include <cstdio>
#define ESP_LOGD(...) ((void)0)
#define ESP_LOGI(...) ((void)0)
#define ESP_LOGE(...) ((void)0)
#define ESP_LOGW(...) ((void)0)
static void resetTaskWdt(){}
static unsigned long testMillis(){return 20000;}
#define millis testMillis
static bool restartPending(){return false;}
static const char *MQTT_COMMAND_TOPICS[]={"command"};
static char g_content[2048];
struct {uint64_t getEfuseMac(){return 1;}}ESP;
struct Message{std::string topic,payload;bool retain;};
struct Broker {
 bool active=false;unsigned connects=0,loops=0,subscriptions=0,streams=0;std::vector<Message>messages;
 bool connected(){return active;}void disconnect(){active=false;}
 void setServer(const char *,unsigned){}
 bool connect(const char *,const char *,const char *,const char *,unsigned,bool,const char *){++connects;active=true;return true;}
 void publish(const char *topic,const char *payload,bool retain){messages.push_back({topic,payload,retain});}
 void publish(const char *topic,const uint8_t *payload,size_t len,bool retain){messages.push_back({topic,std::string(reinterpret_cast<const char *>(payload),len),retain});}
 bool subscribe(const char *){++subscriptions;return true;}bool unsubscribe(const char *){return true;}
 void setCallback(void(*)(const char *,byte *,uint32_t)){}int state(){return 0;}void loop(){++loops;}
 void beginPublish(const char *,size_t,bool){++streams;}size_t write(const uint8_t *,size_t n){return n;}void endPublish(){}
}mqttClient;
struct{bool connected(){return true;}String mac="mac";}net;
struct{bool networkReady=true;bool networkUsable()const{return networkReady;}struct{SettingsLoadStatus loadStatus{SettingsLoadState::loaded};bool enabled=true;const char *protocol="mqtt",*hostname="broker",*username="",*password="",*rootTopic="test";unsigned port=1883;}MQTT;
 struct{struct{String toString(){return String("ip");}}ip;}IP;const char *hostname="controller",*serverId="id";struct{const char *name="version";}fwVersion;
}settings;
struct loop_item_t{int request=0;};
struct{unsigned submitted=0;void submit(const loop_item_t &){++submitted;}}loopQueue;
static unsigned parsed=0;static bool parseMqttCommand(const char *,byte *,uint32_t,int &){++parsed;return true;}
extern MqttConnection mqtt;
struct SomfyShade{
 uint8_t id=255;uint16_t changed=0;uint8_t getShadeId(){return id;}
 void publish(){}void publishTopics(uint16_t){mqtt.publish("shades/1/name","unpersisted",true);}static void unpublish(uint8_t){}
};
struct SomfyGroup{uint8_t id=255,changed=0;int direction=0;uint8_t getGroupId(){return id;}void publish(){}void publish(const char *,int){}static void unpublish(uint8_t){};};
struct SomfyShadeController {
 ModelLoadTrust modelLoad;SomfyShade shades[SOMFY_MAX_SHADES];SomfyGroup groups[SOMFY_MAX_GROUPS];unsigned flagUpdates=0;
 void updateGroupFlags(){++flagUpdates;}SomfyShade *getShadeById(uint8_t id){for(auto &s:shades)if(s.id==id)return &s;return nullptr;}
 SomfyGroup *getGroupById(uint8_t id){for(auto &g:groups)if(g.id==id)return &g;return nullptr;}
 void publish();void sendChanges();
}somfy;
struct{unsigned events=0;uint8_t lastShadeId=255;void shade(SomfyShade &s){++events;lastShadeId=s.id;}void shadeState(SomfyShade &s){++events;lastShadeId=s.id;}void group(SomfyGroup &){++events;}void groupState(SomfyGroup &){++events;}}protoSocket;
static char *itoa(int value,char *out,int){sprintf(out,"%d",value);return out;}
MqttConnection mqtt;
#include "production-mqtt.inc"
int main(){
 // A healthy missing model permits creation, but cannot erase a broker's old
 // model or announce online until a checked model save exists.
 somfy.modelLoad.bootResult(ModelLoadStatus::absent);assert(somfy.modelLoad.usable());
 assert(!mqtt.connect()&&!mqttClient.connects&&mqttClient.messages.empty());
 somfy.publish();assert(!somfy.flagUpdates&&mqttClient.messages.empty());
 somfy.shades[0].id=1;somfy.shades[0].changed=SHADE_CHANGED_CONFIG;
 somfy.sendChanges();assert(protoSocket.events==1&&protoSocket.lastShadeId==1&&!somfy.shades[0].changed&&mqttClient.messages.empty());
 protoSocket.events=0;
 somfy.modelLoad.bootResult(ModelLoadStatus::unknown);
 // Unknown/failed models neither announce online nor erase retained discovery.
 assert(!mqtt.connect()&&!mqttClient.connects&&!mqttClient.subscriptions&&mqttClient.messages.empty());
 somfy.publish();assert(mqttClient.messages.empty()&&!somfy.flagUpdates);
 somfy.shades[0].id=1;somfy.shades[0].changed=SHADE_CHANGED_CONFIG;
 somfy.sendChanges();assert(!protoSocket.events&&somfy.shades[0].changed==SHADE_CHANGED_CONFIG);
 byte payload='1';MqttConnection::receive("test/command",&payload,1);assert(!parsed&&!loopQueue.submitted);
 // Last safety boundary still refuses stale callers with a connected transport.
 mqttClient.active=true;uint8_t byte=1;assert(!mqtt.publish("shades/1/name","bad",true));assert(!mqtt.unpublish("discovery"));assert(!mqtt.publishBuffer("discovery",&byte,1,true));
 JsonDocument doc;auto obj=doc.to<JsonObject>();assert(!mqtt.publishDisco("discovery",obj,true));assert(!mqtt.subscribe("command"));
 assert(mqttClient.messages.empty()&&!mqttClient.streams&&!mqtt.connected());
 assert(!mqtt.loop()&&!mqttClient.active&&mqttClient.messages.size()==1);
 assert(mqttClient.messages[0].topic=="test/status"&&mqttClient.messages[0].payload=="offline"&&mqttClient.messages[0].retain);
 mqtt.loop();assert(mqttClient.messages.size()==1&&!mqttClient.loops);
 // Qualified model never authorizes unreadable network/MQTT credentials.
 somfy.modelLoad.bootResult(ModelLoadStatus::loaded);
 const auto priorConnects=mqttClient.connects;const auto priorMessages=mqttClient.messages.size();
 for(bool badNetwork:{false,true}) {
  settings.networkReady=!badNetwork;settings.MQTT.loadStatus.state=badNetwork?SettingsLoadState::loaded:SettingsLoadState::failed;
  assert(!mqtt.connect()&&mqttClient.connects==priorConnects);
  MqttConnection::receive("test/command",&payload,1);assert(!parsed&&!loopQueue.submitted);
  assert(!mqtt.publish("shades/1/name","bad",true));assert(mqttClient.messages.size()==priorMessages);
 }
 settings.networkReady=true;settings.MQTT.loadStatus.state=SettingsLoadState::loaded;
 // Explicit trusted recovery uses the original online/full-publish path.
 somfy.modelLoad.bootResult(ModelLoadStatus::absent);somfy.modelLoad.committed(false);assert(!mqtt.connect());somfy.modelLoad.committed(true);assert(mqtt.connect()&&mqttClient.connects==1&&mqttClient.subscriptions==1);
 bool online=false;for(const auto &message:mqttClient.messages)online|=message.topic=="test/status"&&message.payload=="online";assert(online);
 MqttConnection::receive("test/command",&payload,1);assert(parsed==1&&loopQueue.submitted==1);
 somfy.sendChanges();assert(protoSocket.events==1&&!somfy.shades[0].changed);
 somfy.modelLoad.fail();const auto beforeReset=mqttClient.messages.size();mqtt.reset();
 assert(!mqttClient.active&&mqttClient.messages.size()==beforeReset+1&&mqttClient.messages.back().payload=="offline");
 puts("Actual MQTT trust: no online, discovery deletion, model publication or command under untrusted state; offline once and trusted recovery passed");
}
