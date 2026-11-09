#include "ConfigSettings.h"
#include "SettingsLoad.h"
#include "SettingsTypedValidation.h"
#include "Secrets.h"
#include <string.h>
namespace {
template<class T> bool adopt(T &target,T &draft,SettingsLoadReader &reader) {
  auto status=reader.status();
  if(status.readable()) {
    if(!validSettings(draft))status.state=SettingsLoadState::invalid;
    target=draft;
  }
  target.loadStatus=status;
  return status.usable();
}
}
bool ConfigSettings::load() {
  SettingsLoadReader reader("CFG");
  char name[sizeof(hostname)];memcpy(name,hostname,sizeof(name));
  bool ssdp=true,updates=true;int8_t connection=0;
  reader.text("hostname",name);
  reader.flag("ssdpBroadcast",ssdp);
  reader.flag("checkForUpdate",updates);
  reader.i8("connType",connection);
  auto status=reader.status();bool migration=false;
  if(status.readable() && connection==0) {
    SettingsLoadReader legacy("WIFI");
    const auto oldName=legacy.text("hostname",name);
    const auto oldSsdp=legacy.flag("ssdpBroadcast",ssdp);
    if(!legacy.status().readable())status.state=SettingsLoadState::failed;
    else {
      connection=1;
      migration=oldName==SettingsReadField::present || oldSsdp==SettingsReadField::present;
    }
  }
  legacyMigrationPending=false;
  if(status.readable()) {
    memcpy(hostname,name,sizeof(name));
    ssdpBroadcast=ssdp;checkForUpdate=updates;connType=conn_types_t(uint8_t(connection));
    if(!validSettings(*this))status.state=SettingsLoadState::invalid;
    legacyMigrationPending=migration;
  }
  loadStatus=status;
  return status.usable();
}
bool NTPSettings::begin(){return load();}
bool NTPSettings::load() {
  NTPSettings draft=*this;SettingsLoadReader reader("NTP");
  reader.text("ntpServer",draft.ntpServer);
  reader.text("posixZone",draft.posixZone);
  return adopt(*this,draft,reader);
}
bool WifiSettings::begin(){return load();}
bool WifiSettings::load() {
  WifiSettings draft=*this;
  struct Wipe {WifiSettings &value;~Wipe(){wipeSecret(value.passphrase);}} wipe{draft};
  SettingsLoadReader reader("WIFI");
  reader.text("ssid",draft.ssid);
  reader.text("passphrase",draft.passphrase);
  reader.flag("roaming",draft.roaming);
  reader.flag("hidden",draft.hidden);
  return adopt(*this,draft,reader);
}
bool MQTTSettings::begin(){return load();}
bool MQTTSettings::load() {
  MQTTSettings draft=*this;
  struct Wipe {MQTTSettings &value;~Wipe(){wipeSecret(value.username);wipeSecret(value.password);}} wipe{draft};
  SettingsLoadReader reader("MQTT");
  int16_t port=1883;
  reader.text("protocol",draft.protocol);
  reader.text("hostname",draft.hostname);
  reader.i16("port",port);
  draft.port=uint16_t(port); // Historical signed NVS bits represent unsigned ports.
  reader.text("username",draft.username);
  reader.text("password",draft.password);
  reader.text("rootTopic",draft.rootTopic);
  reader.flag("enabled",draft.enabled);
  reader.flag("pubDisco",draft.pubDisco);
  reader.text("discoTopic",draft.discoTopic);
  return adopt(*this,draft,reader);
}
bool IPSettings::begin(){return load();}
bool IPSettings::load() {
  IPSettings draft=*this;SettingsLoadReader reader("IP");
  reader.flag("dhcp",draft.dhcp);
  static const char *const keys[]={"ip","gateway","subnet","dns1","dns2"};
  for(uint8_t i=0;i<5;++i) {
    char raw[16]={};draft.addressText(i,raw,sizeof(raw));
    reader.text(keys[i],raw);
    if(!draft.setAddress(i,raw))strlcpy(draft.invalidAddress[i],raw,sizeof(draft.invalidAddress[i]));
  }
  return adopt(*this,draft,reader);
}
#if HAS_ETHERNET
bool EthernetSettings::begin(){return load();}
bool EthernetSettings::load() {
  EthernetSettings draft=*this;SettingsLoadReader reader("ETH");
  int8_t board=int8_t(draft.boardType),phy=int8_t(draft.phyType),clock=int8_t(draft.CLKMode);
  reader.i8("boardType",board);
  reader.i8("phyType",phy);
  reader.i8("CLKMode",clock);
  draft.boardType=uint8_t(board);draft.phyType=uint8_t(phy);draft.CLKMode=uint8_t(clock);
  reader.i8("phyAddress",draft.phyAddress);
  reader.i8("PWRPin",draft.PWRPin);
  reader.i8("MDCPin",draft.MDCPin);
  reader.i8("MDIOPin",draft.MDIOPin);
  return adopt(*this,draft,reader);
}
#endif
bool ConfigSettings::networkUsable() const {
  return loadStatus.usable() && WIFI.loadStatus.usable() && IP.loadStatus.usable()
#if HAS_ETHERNET
    && Ethernet.loadStatus.usable()
#endif
    ;
}
bool ConfigSettings::settingsUsable() const {
  return networkUsable() && NTP.loadStatus.usable() && MQTT.loadStatus.usable() && Security.loadTrusted();
}
