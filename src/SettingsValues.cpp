#include "ConfigSettings.h"
#include "Transceiver.h"
#include "Secrets.h"

#if HAS_ETHERNET
EthernetSettings::EthernetSettings() {}
#endif

WifiSettings::WifiSettings() {}

IPSettings::IPSettings() {}

bool BaseSettings::parseValueString(JsonObject &obj, const char *prop, char *pdest, size_t size) {
  if(!obj[prop].isNull()) strlcpy(pdest, obj[prop], size);
  return true;
}

bool BaseSettings::parseIPAddress(JsonObject &obj, const char *prop, IPAddress *pdest) {
  if(!obj[prop].isNull()) {
    char buff[16];
    strlcpy(buff, obj[prop], sizeof(buff));
    pdest->fromString(buff);
  }
  return true;
}

int BaseSettings::parseValueInt(JsonObject &obj, const char *prop, int defVal) {
  if(!obj[prop].isNull()) return obj[prop];
  return defVal;
}

double BaseSettings::parseValueDouble(JsonObject &obj, const char *prop, double defVal) {
  if(!obj[prop].isNull()) return obj[prop];
  return defVal;
}

bool ConfigSettings::fromJSON(JsonObject &obj) {
    if(!obj["ssdpBroadcast"].isNull()) this->ssdpBroadcast = obj["ssdpBroadcast"];
    if(!obj["hostname"].isNull()) this->parseValueString(obj, "hostname", this->hostname, sizeof(this->hostname));
    if(!obj["connType"].isNull()) this->connType = static_cast<conn_types_t>(obj["connType"].as<uint8_t>());
    return true;
}

bool MQTTSettings::fromJSON(JsonObject &obj) {
  if(!obj["enabled"].isNull()) this->enabled = obj["enabled"];
  if(!obj["pubDisco"].isNull()) this->pubDisco = obj["pubDisco"];
  this->parseValueString(obj, "protocol", this->protocol, sizeof(this->protocol));
  this->parseValueString(obj, "hostname", this->hostname, sizeof(this->hostname));
  // An empty password clears the stored one only alongside a new username.
  const char *username = obj["username"].as<const char *>();
  bool newUsername = username && strcmp(username, this->username) != 0;
  this->parseValueString(obj, "username", this->username, sizeof(this->username));
  applySecret(obj["password"].as<const char *>(), this->password, sizeof(this->password), newUsername);
  this->parseValueString(obj, "rootTopic", this->rootTopic, sizeof(this->rootTopic));
  this->parseValueString(obj, "discoTopic", this->discoTopic, sizeof(this->discoTopic));
  if(!obj["port"].isNull()) this->port = obj["port"];
  return true;
}

bool NTPSettings::fromJSON(JsonObject &obj) {
  this->parseValueString(obj, "ntpServer", this->ntpServer, sizeof(this->ntpServer));
  this->parseValueString(obj, "posixZone", this->posixZone, sizeof(this->posixZone));
  return true;
}

bool IPSettings::fromJSON(JsonObject &obj) {
  if(!obj["dhcp"].isNull()) this->dhcp = obj["dhcp"];
  this->parseIPAddress(obj, "ip", &this->ip);
  this->parseIPAddress(obj, "gateway", &this->gateway);
  this->parseIPAddress(obj, "subnet", &this->subnet);
  this->parseIPAddress(obj, "dns1", &this->dns1);
  this->parseIPAddress(obj, "dns2", &this->dns2);
  return true;
}

bool SecuritySettings::fromJSON(JsonObject &obj) {
  if(!obj["type"].isNull()) this->type = static_cast<security_types>(obj["type"].as<uint8_t>());
  this->parseValueString(obj, "username", this->username, sizeof(this->username));
  applySecret(obj["password"].as<const char *>(), this->password, sizeof(this->password));
  applySecret(obj["pin"].as<const char *>(), this->pin, sizeof(this->pin));
  return true;
}

bool WifiSettings::fromJSON(JsonObject &obj) {
  // An empty passphrase clears the stored one only alongside a new SSID (an open network).
  const char *ssid = obj["ssid"].as<const char *>();
  bool newSsid = ssid && strcmp(ssid, this->ssid) != 0;
  this->parseValueString(obj, "ssid", this->ssid, sizeof(this->ssid));
  applySecret(obj["passphrase"].as<const char *>(), this->passphrase, sizeof(this->passphrase), newSsid);
  if(!obj["roaming"].isNull()) this->roaming = obj["roaming"];
  if(!obj["hidden"].isNull()) this->hidden = obj["hidden"];
  return true;
}

#if HAS_ETHERNET
bool EthernetSettings::fromJSON(JsonObject &obj) {
  if(!obj["boardType"].isNull()) this->boardType = obj["boardType"];
  if(!obj["phyAddress"].isNull()) this->phyAddress = obj["phyAddress"];
  if(!obj["CLKMode"].isNull()) this->CLKMode = obj["CLKMode"];
  if(!obj["phyType"].isNull()) this->phyType = obj["phyType"];
  if(!obj["PWRPin"].isNull()) this->PWRPin = obj["PWRPin"];
  if(!obj["MDCPin"].isNull()) this->MDCPin = obj["MDCPin"];
  if(!obj["MDIOPin"].isNull()) this->MDIOPin = obj["MDIOPin"];
  return true;
}
#endif

void transceiver_config_t::applyBoardPins() {
  if(!BoardProfile::integratedRadio) return;
  this->SCKPin = BoardProfile::sck;
  this->MOSIPin = BoardProfile::mosi;
  this->MISOPin = BoardProfile::miso;
  this->CSNPin = BoardProfile::cs;
  this->TXPin = BoardProfile::tx;
  this->RXPin = BoardProfile::rx;
}
