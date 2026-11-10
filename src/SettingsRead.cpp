#include "SettingsRead.h"
#include "ConfigSettings.h"
#include "Transceiver.h"
#include "Secrets.h"
#include <stdio.h>
#include <string.h>
#include <type_traits>

namespace {
class SnapshotWriter {
  SettingsSnapshotSink &sink;
public:
  explicit SnapshotWriter(SettingsSnapshotSink &sink) : sink(sink) {}
  void add(proto_field_t field, bool input) {
    settings_value_t value; value.kind='b'; value.boolean=input; sink.field(field,value);
  }
  template<class T, typename std::enable_if<std::is_integral<T>::value &&
      !std::is_same<T,bool>::value,int>::type = 0>
  void add(proto_field_t field, T input) {
    settings_value_t value; value.kind='i'; value.integer=static_cast<int32_t>(input); sink.field(field,value);
  }
  void add(proto_field_t field, float input) {
    settings_value_t value; value.kind='n'; value.number=input; sink.field(field,value);
  }
  template<size_t N>
  void add(proto_field_t field, const char (&input)[N]) {
    settings_value_t value; value.kind='s';
    size_t length=0;
    while(length<N && input[length]) ++length;
    if(length>=sizeof(value.text)) length=sizeof(value.text)-1;
    memcpy(value.text,input,length);
    sink.field(field,value);
    wipeSecret(value.text);
  }
};
void addAddress(SnapshotWriter &writer, proto_field_t field, const IPAddress &ip) {
  if(!(ip[0] || ip[1] || ip[2] || ip[3])) { writer.add(field,""); return; }
  char text[16];
  snprintf(text,sizeof(text),"%u.%u.%u.%u",ip[0],ip[1],ip[2],ip[3]);
  writer.add(field,text);
}
}
bool readSettingsSnapshot(settings_section_t section, const ConfigSettings &settings,
                          const transceiver_config_t &cfg, SettingsSnapshotSink &sink) {
  bool readable=false;
  switch(section){
   case settings_section_t::general:readable=settings.loadStatus.readable() && settings.NTP.loadStatus.readable();break;
   case settings_section_t::network:readable=settings.loadStatus.readable() && settings.WIFI.loadStatus.readable() && settings.IP.loadStatus.readable()
#if HAS_ETHERNET
     && settings.Ethernet.loadStatus.readable()
#endif
     ;break;
   case settings_section_t::mqtt:readable=settings.MQTT.loadStatus.readable();break;
   case settings_section_t::security:readable=settings.Security.loadTrusted();break;
   case settings_section_t::radio:readable=cfg.loadStatus.readable();break;
  }
  if(!readable)return false;

  SnapshotWriter w(sink);
  switch(section) {
    case settings_section_t::general:
      w.add(proto_field_t::hostname, settings.hostname);
      w.add(proto_field_t::ssdpBroadcast, settings.ssdpBroadcast);
      w.add(proto_field_t::ntpServer, settings.NTP.ntpServer);
      w.add(proto_field_t::posixZone, settings.NTP.posixZone);
      break;
    case settings_section_t::network:
      w.add(proto_field_t::connMode, static_cast<uint8_t>(settings.connType));
      w.add(proto_field_t::ssid, settings.WIFI.ssid);
      w.add(proto_field_t::hasPassphrase, hasSecret(settings.WIFI.passphrase));
      w.add(proto_field_t::roaming, settings.WIFI.roaming);
      w.add(proto_field_t::hidden, settings.WIFI.hidden);
      w.add(proto_field_t::hasEthernet, bool(HAS_ETHERNET));
#if HAS_ETHERNET
      w.add(proto_field_t::boardType, settings.Ethernet.boardType);
      w.add(proto_field_t::phyType, settings.Ethernet.phyType);
      w.add(proto_field_t::CLKMode, settings.Ethernet.CLKMode);
      w.add(proto_field_t::phyAddress, settings.Ethernet.phyAddress);
      w.add(proto_field_t::PWRPin, settings.Ethernet.PWRPin);
      w.add(proto_field_t::MDCPin, settings.Ethernet.MDCPin);
      w.add(proto_field_t::MDIOPin, settings.Ethernet.MDIOPin);
#endif
      w.add(proto_field_t::dhcp, settings.IP.dhcp);
      if(settings.IP.invalidAddress[0][0])w.add(proto_field_t::ip,settings.IP.invalidAddress[0]);else addAddress(w, proto_field_t::ip, settings.IP.ip);
      if(settings.IP.invalidAddress[1][0])w.add(proto_field_t::gateway,settings.IP.invalidAddress[1]);else addAddress(w, proto_field_t::gateway, settings.IP.gateway);
      if(settings.IP.invalidAddress[2][0])w.add(proto_field_t::subnet,settings.IP.invalidAddress[2]);else addAddress(w, proto_field_t::subnet, settings.IP.subnet);
      if(settings.IP.invalidAddress[3][0])w.add(proto_field_t::dns1,settings.IP.invalidAddress[3]);else addAddress(w, proto_field_t::dns1, settings.IP.dns1);
      if(settings.IP.invalidAddress[4][0])w.add(proto_field_t::dns2,settings.IP.invalidAddress[4]);else addAddress(w, proto_field_t::dns2, settings.IP.dns2);
      break;
    case settings_section_t::mqtt:
      w.add(proto_field_t::enabled, settings.MQTT.enabled);
      w.add(proto_field_t::pubDisco, settings.MQTT.pubDisco);
      w.add(proto_field_t::protocol, settings.MQTT.protocol);
      w.add(proto_field_t::hostname, settings.MQTT.hostname);
      w.add(proto_field_t::port, settings.MQTT.port);
      w.add(proto_field_t::username, settings.MQTT.username);
      w.add(proto_field_t::hasPassword, hasSecret(settings.MQTT.password));
      w.add(proto_field_t::rootTopic, settings.MQTT.rootTopic);
      w.add(proto_field_t::discoTopic, settings.MQTT.discoTopic);
      break;
    case settings_section_t::security:
      w.add(proto_field_t::securityType, static_cast<uint8_t>(settings.Security.type));
      w.add(proto_field_t::username, settings.Security.username);
      w.add(proto_field_t::hasPassword, hasSecret(settings.Security.password));
      w.add(proto_field_t::hasPin, hasSecret(settings.Security.pin));
      break;
    case settings_section_t::radio: {
      w.add(proto_field_t::bitLength, cfg.type);
      w.add(proto_field_t::proto, static_cast<uint8_t>(cfg.proto));
      w.add(proto_field_t::enabled, cfg.enabled);
      w.add(proto_field_t::TXPin, cfg.TXPin);
      w.add(proto_field_t::RXPin, cfg.RXPin);
      w.add(proto_field_t::SCKPin, cfg.SCKPin);
      w.add(proto_field_t::MOSIPin, cfg.MOSIPin);
      w.add(proto_field_t::MISOPin, cfg.MISOPin);
      w.add(proto_field_t::CSNPin, cfg.CSNPin);
      w.add(proto_field_t::frequency, cfg.frequency);
      w.add(proto_field_t::rxBandwidth, cfg.rxBandwidth);
      w.add(proto_field_t::deviation, cfg.deviation);
      w.add(proto_field_t::txPower, cfg.txPower);
      w.add(proto_field_t::noiseDetection, cfg.noiseDetection);
      w.add(proto_field_t::radioInit, cfg.radioInit);
      break;
    }
    default: return false;
  }
  return true;
}
