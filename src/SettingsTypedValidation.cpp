#include "SettingsTypedValidation.h"
#include "ConfigSettings.h"
#include "Transceiver.h"
#include "ProtoSettings.h"
#include <math.h>
#include <string.h>
namespace {
bool number(settings_part_t part,proto_field_t field,int64_t value) {
  const auto *meta=settingsFieldFor(part,field,HAS_ETHERNET);
  return meta && settingsIntegerValid(*meta,value);
}
template<size_t N> bool text(settings_part_t part,proto_field_t field,const char (&value)[N]) {
  const auto *meta=settingsFieldFor(part,field,HAS_ETHERNET);
  const auto *end=static_cast<const char *>(memchr(value,0,N));
  return meta && end && size_t(end-value)<=size_t(meta->hi);
}
bool real(proto_field_t field,float value) {
  const auto *meta=settingsFieldFor(settings_part_t::radio,field,HAS_ETHERNET);
  // Compare in the stored float domain, including legitimate boundary rounding.
  return meta && isfinite(value) && value>=float(meta->lo)/100.0f && value<=float(meta->hi)/100.0f;
}
}
bool validSettings(const ConfigSettings &c) {
  return text(settings_part_t::general,proto_field_t::hostname,c.hostname) &&
    number(settings_part_t::conn,proto_field_t::connMode,uint8_t(c.connType));
}
bool validSettings(const NTPSettings &c) {
  return text(settings_part_t::ntp,proto_field_t::ntpServer,c.ntpServer) &&
    text(settings_part_t::ntp,proto_field_t::posixZone,c.posixZone);
}
bool validSettings(const WifiSettings &c) {
  return text(settings_part_t::wifi,proto_field_t::ssid,c.ssid) &&
    text(settings_part_t::wifi,proto_field_t::passphrase,c.passphrase);
}
bool validSettings(const IPSettings &c) {
  for(const auto &raw:c.invalidAddress)if(raw[0])return false;
  return true;
}
bool validSettings(const MQTTSettings &c) {
  return number(settings_part_t::mqtt,proto_field_t::port,c.port) &&
    text(settings_part_t::mqtt,proto_field_t::protocol,c.protocol) &&
    text(settings_part_t::mqtt,proto_field_t::hostname,c.hostname) &&
    text(settings_part_t::mqtt,proto_field_t::username,c.username) &&
    text(settings_part_t::mqtt,proto_field_t::password,c.password) &&
    text(settings_part_t::mqtt,proto_field_t::rootTopic,c.rootTopic) &&
    text(settings_part_t::mqtt,proto_field_t::discoTopic,c.discoTopic);
}
#if HAS_ETHERNET
bool validSettings(const EthernetSettings &c) {
  return number(settings_part_t::ethernet,proto_field_t::boardType,c.boardType) &&
    number(settings_part_t::ethernet,proto_field_t::phyType,c.phyType) &&
    number(settings_part_t::ethernet,proto_field_t::CLKMode,c.CLKMode) &&
    number(settings_part_t::ethernet,proto_field_t::phyAddress,c.phyAddress) &&
    number(settings_part_t::ethernet,proto_field_t::PWRPin,c.PWRPin) &&
    number(settings_part_t::ethernet,proto_field_t::MDCPin,c.MDCPin) &&
    number(settings_part_t::ethernet,proto_field_t::MDIOPin,c.MDIOPin);
}
#endif
bool validSettings(const transceiver_config_t &c) {
  return BoardProfile::radioFrequencyAllowed(c.frequency) &&
    number(settings_part_t::radio,proto_field_t::bitLength,c.type) &&
    number(settings_part_t::radio,proto_field_t::proto,uint8_t(c.proto)) &&
    number(settings_part_t::radio,proto_field_t::TXPin,c.TXPin) &&
    number(settings_part_t::radio,proto_field_t::RXPin,c.RXPin) &&
    number(settings_part_t::radio,proto_field_t::SCKPin,c.SCKPin) &&
    number(settings_part_t::radio,proto_field_t::MOSIPin,c.MOSIPin) &&
    number(settings_part_t::radio,proto_field_t::MISOPin,c.MISOPin) &&
    number(settings_part_t::radio,proto_field_t::CSNPin,c.CSNPin) &&
    number(settings_part_t::radio,proto_field_t::txPower,c.txPower) &&
    real(proto_field_t::frequency,c.frequency) &&
    real(proto_field_t::rxBandwidth,c.rxBandwidth) &&
    real(proto_field_t::deviation,c.deviation);
}
