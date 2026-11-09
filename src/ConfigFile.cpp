#include "SelectedSettingsRestore.h"
#include "DeviceOps.h"
#include "ShadeGpioPolicy.h"
#include <driver/gpio.h>
#include <Arduino.h>
#include <LittleFS.h>
#include "esp_log.h"
#include "ConfigFile.h"
#include "GitUpdate.h"
#include <esp_timer.h>
#include "SettingsRevisions.h"
#include "Utils.h"
#include "ConfigSettings.h"
#include "ConfigValidation.h"
#include "BoardProfile.h"
#include "ModelPersistence.h"
#include "ModelRecords.h"
#include "TaskWdt.h"
#include "ModelIdleForAdministration.h"
#include "LoopQueue.h"
#include "SafeRestart.h"
#include "RestoreExecution.h"
#include "SettingsDurability.h"

static const char *TAG = "ConfigFile";

extern ConfigSettings settings;
extern LoopQueue loopQueue;

// The records' strings are read straight into these fields.
static_assert(CFG_NAME_LEN == sizeof(SomfyShade::name), "shade name size");
static_assert(CFG_NAME_LEN == sizeof(SomfyGroup::name), "group name size");
static_assert(CFG_NAME_LEN == sizeof(SomfyRoom::name), "room name size");
static_assert(CFG_HOSTNAME_LEN == sizeof(ConfigSettings::hostname), "hostname size");
static_assert(CFG_NTP_SERVER_LEN == sizeof(NTPSettings::ntpServer), "NTP server size");
static_assert(CFG_POSIX_ZONE_LEN == sizeof(NTPSettings::posixZone), "time zone size");
static_assert(CFG_MQTT_PROTOCOL_LEN == sizeof(MQTTSettings::protocol), "MQTT protocol size");
static_assert(CFG_MQTT_HOSTNAME_LEN == sizeof(MQTTSettings::hostname), "MQTT hostname size");
static_assert(CFG_MQTT_TOPIC_LEN == sizeof(MQTTSettings::rootTopic), "MQTT root topic size");
static_assert(CFG_MQTT_TOPIC_LEN == sizeof(MQTTSettings::discoTopic),
              "MQTT discovery topic size");

bool ConfigFile::begin(const char *filename, bool readOnly) {
  this->end();
  this->readOnly = readOnly;
  this->_writeFailed = false;
  if(readOnly) {
    char path[256];
    const int n=snprintf(path,sizeof(path),"%s%s",LittleFS.mountpoint(),filename);
    snapshotStatus=n>0&&size_t(n)<sizeof(path) ? snapshot.capture(path,modelFilesystemMounted()) : ModelLoadStatus::unavailable;
    readPosition=0;readFailed=false;beginRecord();
    return this->_opened=snapshotStatus==ModelLoadStatus::loaded;
  }
  this->file = LittleFS.open(filename, "w");
  this->_opened = bool(this->file) && !this->file.isDirectory();
  if (!this->_opened)
    this->file.close();
  return this->_opened;
}
bool ConfigFile::beginRAM(String *buf) {
  this->end();
  readOnly = false;
  _writeFailed = false;
  _ramBuf = buf;
  _opened = buf != nullptr;
  return _opened;
}
void ConfigFile::end() {
  if (this->isOpen()) {
    if (_ramBuf) {
      _ramBuf = nullptr;
    } else {
      if (!this->readOnly)
        this->file.flush();
      this->file.close();
    }
  }
  snapshot.clear();readPosition=0;
  this->_opened = false;
}
bool ConfigFile::isOpen() { return this->_opened; }
bool ConfigFile::readByte(uint8_t &val) {
  if(!snapshot.data() || readPosition>=snapshot.size()){readFailed=true;return false;}
  val=snapshot.data()[readPosition++];return true;
}
uint32_t ConfigFile::position() { return readOnly ? readPosition : this->file.position(); }
bool ConfigFile::seekRead(uint32_t offset) {
  if(offset>snapshot.size()){readFailed=true;return false;}
  readPosition=offset;beginRecord();return true;
}
bool ConfigFile::writeBytes(const uint8_t *buf, size_t len) {
  if (!this->isOpen() || this->readOnly || this->_writeFailed)
    return false;
  if (!len)
    return true;
  const bool ok =
      _ramBuf ? _ramBuf->concat(reinterpret_cast<const char *>(buf), len)
              : this->file.write(buf, len) == len;
  this->_writeFailed = !ok;
  return ok;
}
bool ConfigFile::writeHeader() {
  if (!this->isOpen())
    return false;
  return writeConfigHeader(*this, this->header);
}
bool ConfigFile::readHeader() {
  if(!this->isOpen()) return false;
  readConfigHeader(*this, this->header);
  ESP_LOGD(TAG, "version:%u len:%u roomSize:%u roomRecs:%u shadeSize:%u shadeRecs:%u groupSize:%u groupRecs: %u pos:%d", this->header.version, this->header.length, this->header.roomRecordSize, this->header.roomRecords, this->header.shadeRecordSize, this->header.shadeRecords, this->header.groupRecordSize, this->header.groupRecords, this->position());
  return true;
}

// Between the shade model and its records.
static room_record_t roomRecord(const SomfyRoom *room) {
  room_record_t rec;
  rec.roomId = room->roomId;
  strlcpy(rec.name, room->name, sizeof(rec.name));
  rec.sortOrder = room->sortOrder;
  return rec;
}
static shade_record_t shadeRecord(SomfyShade *shade) {
  shade_record_t rec;
  rec.shadeId = shade->getShadeId();
  rec.paired = shade->paired;
  rec.shadeType = shade->shadeType;
  rec.remoteAddress = shade->getRemoteAddress();
  strlcpy(rec.name, shade->name, sizeof(rec.name));
  rec.tiltType = shade->tiltType;
  rec.proto = shade->proto;
  rec.bitLength = shade->bitLength;
  rec.upTime = shade->upTime;
  rec.downTime = shade->downTime;
  rec.tiltTime = shade->tiltTime;
  rec.stepSize = shade->stepSize;
  for(uint8_t j = 0; j < SOMFY_MAX_LINKED_REMOTES; j++) rec.linkedRemotes[j] = shade->linkedRemotes[j].getRemoteAddress();
  rec.lastRollingCode = shade->lastRollingCode;
  rec.flags = shade->getFlags();
  rec.myPos = shade->state().myPos;
  rec.myTiltPos = shade->state().myTiltPos;
  rec.currentPos = shade->state().currentPos;
  rec.currentTiltPos = shade->state().currentTiltPos;
  rec.flipCommands = shade->flipCommands;
  rec.flipPosition = shade->flipPosition;
  rec.repeats = shade->repeats;
  rec.sortOrder = shade->sortOrder;
  rec.gpioUp = shade->gpioUp;
  rec.gpioDown = shade->gpioDown;
  rec.gpioMy = shade->gpioMy;
  rec.gpioFlags = shade->gpioFlags;
  rec.roomId = shade->roomId;
  return rec;
}
static group_record_t groupRecord(SomfyGroup *group) {
  group_record_t rec;
  rec.groupId = group->getGroupId();
  rec.groupType = group->groupType;
  rec.remoteAddress = group->getRemoteAddress();
  strlcpy(rec.name, group->name, sizeof(rec.name));
  rec.proto = group->proto;
  rec.bitLength = group->bitLength;
  memcpy(rec.linkedShades, group->linkedShades, sizeof(rec.linkedShades));
  rec.repeats = group->repeats;
  rec.sortOrder = group->sortOrder;
  rec.flipCommands = group->flipCommands;
  rec.roomId = group->roomId;
  rec.lastRollingCode = group->lastRollingCode;
  return rec;
}
static settings_record_t settingsRecord() {
  settings_record_t rec;
  strlcpy(rec.fwVersion, settings.fwVersion.name, sizeof(rec.fwVersion));
  strlcpy(rec.hostname, settings.hostname, sizeof(rec.hostname));
  strlcpy(rec.ntpServer, settings.NTP.ntpServer, sizeof(rec.ntpServer));
  strlcpy(rec.posixZone, settings.NTP.posixZone, sizeof(rec.posixZone));
  rec.ssdpBroadcast = settings.ssdpBroadcast;
  rec.checkForUpdate = settings.checkForUpdate;
  return rec;
}
static net_record_t netRecord() {
  net_record_t rec;
  rec.connType = static_cast<uint8_t>(settings.connType);
  rec.dhcp = settings.IP.dhcp;
  strlcpy(rec.ip, settings.IP.ip.toString().c_str(), sizeof(rec.ip));
  strlcpy(rec.gateway, settings.IP.gateway.toString().c_str(), sizeof(rec.gateway));
  strlcpy(rec.subnet, settings.IP.subnet.toString().c_str(), sizeof(rec.subnet));
  strlcpy(rec.dns1, settings.IP.dns1.toString().c_str(), sizeof(rec.dns1));
  strlcpy(rec.dns2, settings.IP.dns2.toString().c_str(), sizeof(rec.dns2));
  strlcpy(rec.mqttProtocol, settings.MQTT.protocol, sizeof(rec.mqttProtocol));
  strlcpy(rec.mqttHostname, settings.MQTT.hostname, sizeof(rec.mqttHostname));
  rec.mqttPort = settings.MQTT.port;
  rec.mqttPubDisco = settings.MQTT.pubDisco;
  strlcpy(rec.mqttRootTopic, settings.MQTT.rootTopic, sizeof(rec.mqttRootTopic));
  strlcpy(rec.mqttDiscoTopic, settings.MQTT.discoTopic, sizeof(rec.mqttDiscoTopic));
#if HAS_ETHERNET
  rec.ethBoardType = settings.Ethernet.boardType;
  rec.ethPhyType = settings.Ethernet.phyType;
  rec.ethClkMode = settings.Ethernet.CLKMode;
  rec.ethPhyAddress = settings.Ethernet.phyAddress;
  rec.ethPWRPin = settings.Ethernet.PWRPin;
  rec.ethMDCPin = settings.Ethernet.MDCPin;
  rec.ethMDIOPin = settings.Ethernet.MDIOPin;
#endif
  return rec;
}
static trans_record_t transRecord(const transceiver_config_t &cfg) {
  trans_record_t rec;
  rec.enabled = cfg.enabled;
  rec.proto = cfg.proto;
  rec.type = cfg.type;
  rec.SCKPin = cfg.SCKPin;
  rec.CSNPin = cfg.CSNPin;
  rec.MOSIPin = cfg.MOSIPin;
  rec.MISOPin = cfg.MISOPin;
  rec.TXPin = cfg.TXPin;
  rec.RXPin = cfg.RXPin;
  rec.frequency = cfg.frequency;
  rec.rxBandwidth = cfg.rxBandwidth;
  rec.deviation = cfg.deviation;
  rec.txPower = cfg.txPower;
  rec.noiseDetection = cfg.noiseDetection;
  return rec;
}
// Counter values are collected from the same immutable bytes later adopted.
// No model record, GPIO or NVS read is applied during adoption itself.
static_assert(CounterLoad::maximumEntries==SOMFY_MAX_SHADES*(SOMFY_MAX_LINKED_REMOTES+1)+SOMFY_MAX_GROUPS,"Complete loaded counter address bound");
static void cooperateCounterLoad(const void *){resetTaskWdt();delay(0);}
static bool restoreIdle(SomfyShadeController *model,const RadioInstallToken &radio){
  if(loopQueue.pending() || restartPending() || !modelIdleForAdministration(*model))return false;
  return model->transceiver.installPauseHeld(radio);
}
static bool continueRestoreCounterLoad(const void *context){
  return gitUpdate.transferRestoreClaimed(*static_cast<const GitUpdateOwnerToken *>(context),uint64_t(esp_timer_get_time())/1000);
}
bool ShadeConfigFile::prepareCounters(CounterLoadControl control) {
  counters.clear();const uint32_t start=position();
  if(!skipRecords(*this,header.roomRecords))return false;
  for(unsigned i=0;i<header.shadeRecords;++i){
    shade_record_t rec;
    if(!readShadeRecord(*this,header,rec))return false;
    if(rec.shadeId==255)continue;
    if(!counters.include(rec.remoteAddress,rec.lastRollingCode))return false;
    for(auto address:rec.linkedRemotes)if(!counters.include(address))return false;
  }
  for(unsigned i=0;i<header.groupRecords;++i){
    group_record_t rec;
    if(!readGroupRecord(*this,header,rec))return false;
    if(rec.groupId!=255 && !counters.include(rec.remoteAddress,rec.hasRollingCode?rec.lastRollingCode:0))return false;
  }
  if(readFailed || !seekRead(start))return false;
  const auto result=counters.resolve(control);
  if(!result.ready())ESP_LOGE(TAG,"Model counter load failed at stage %u",unsigned(result.stage));
  return result.ready();
}

// Restore is an external input boundary. Boot keeps the historical codec,
// while selected restore records must be complete before any narrowing cast.
bool ShadeConfigFile::validateRestoreRecords(const restore_options_t &opts){
  const uint32_t modelStart=position();
  config_header_t checkedHeader;
  if(!seekRead(0))return false;
  beginCheckedValues();readConfigHeader(*this,checkedHeader);
  const auto headerValues=endCheckedValues();const auto v=checkedHeader.version;
  const unsigned headerFields=4+(v>=19?2:0)+(v>10?2:0)+(v>=21?2:0)+(v>13?4:0);
  if(!headerValues.valid || headerValues.fields!=headerFields || readFailed || position()!=modelStart ||
      !seekRead(modelStart))return false;
  header=checkedHeader;
  auto checked=[&](unsigned fields,auto decode){
    beginCheckedValues();const bool parsed=decode();const auto values=endCheckedValues();
    return parsed&&values.valid&&values.fields==fields&&!readFailed;
  };
  const auto knownProtocol=[](radio_proto proto){
    return proto==radio_proto::RTS || proto==radio_proto::RTW || proto==radio_proto::RTV ||
        proto==radio_proto::GP_Relay || proto==radio_proto::GP_Remote;
  };
  uint32_t roomsSeen=0,shadesSeen=0,groupsSeen=0;
  const auto uniqueId=[](uint32_t &seen,uint8_t id,uint8_t maximum,uint8_t unused){
    if(id==unused)return true;
    if(!id || id>maximum)return false;
    const uint32_t mask=uint32_t(1)<<(id-1);
    if(seen&mask)return false;
    seen|=mask;return true;
  };
  static_assert(SOMFY_MAX_ROOMS<=32 && SOMFY_MAX_SHADES<=32 && SOMFY_MAX_GROUPS<=32,"Bounded stable ID sets");
  bool valid=true;
  if(opts.shades){
    const unsigned shadeFields=12+(v<5?5:SOMFY_MAX_LINKED_REMOTES)+(v<4?1:2)+
        (v>6?1:0)+(v>1?1:0)+(v>5?1:0)+(v>7?1:0)+(v>=9?1:0)+(v>=10?1:0)+
        (v>=12?1:0)+(v>=13?1:0)+(v>14?2:0)+(v>15?1:0)+(v>16?1:0)+(v>=19?1:0);
    const unsigned groupFields=6+SOMFY_MAX_GROUPED_SHADES+(v>=23?1:0)+
        (v>=12?1:0)+(v>=13?1:0)+(v>=18?1:0)+(v>=19?1:0);
    for(unsigned i=0;valid&&i<header.roomRecords;++i)valid=checked(3,[&]{
      room_record_t rec;return readRoomRecord(*this,rec)&&uniqueId(roomsSeen,rec.roomId,SOMFY_MAX_ROOMS,0);
    });
    for(unsigned i=0;valid&&i<header.shadeRecords;++i)valid=checked(shadeFields,[&]{
      shade_record_t rec;if(!readShadeRecord(*this,header,rec))return false;
      return rec.shadeId==255 || (uniqueId(shadesSeen,rec.shadeId,SOMFY_MAX_SHADES,255) &&
          uint8_t(rec.shadeType)<=uint8_t(shade_types::rgate1) &&
          uint8_t(rec.tiltType)<=uint8_t(tilt_types::euromode) && knownProtocol(rec.proto));
    });
    for(unsigned i=0;valid&&i<header.groupRecords;++i)valid=checked(groupFields,[&]{
      group_record_t rec;if(!readGroupRecord(*this,header,rec))return false;
      return rec.groupId==255 || (uniqueId(groupsSeen,rec.groupId,SOMFY_MAX_GROUPS,255) &&
          rec.groupType==group_types::channel &&
          (rec.proto==radio_proto::RTS || rec.proto==radio_proto::RTW || rec.proto==radio_proto::RTV));
    });
  }else valid=skipRecords(*this,uint32_t(header.roomRecords)+header.shadeRecords+header.groupRecords);
  if(opts.repeaters){
    for(unsigned i=0;valid&&i<header.repeaterRecords;++i)valid=checked(SOMFY_MAX_REPEATERS,[&]{
      uint32_t addresses[SOMFY_MAX_REPEATERS];if(!readRepeaterRecord(*this,addresses))return false;
      for(auto address:addresses)if(address>0xffffff)return false;
      return true;
    });
  }
  const bool rewound=seekRead(modelStart);
  return valid&&rewound&&!readFailed;
}

// Two bounded passes over the SAME immutable model: count physically touched
// pads, then validate each assignment against projected settings and its peers.
// No model draft, mutable-file reread, pin operation or counter I/O is needed.
bool ShadeConfigFile::validateRestoreGPIOs(SomfyShadeController *model,bool imported,
    const SelectedSettingsRestore &stage){
  static_assert(3*SOMFY_MAX_SHADES<255,"Per-pin owner counts cannot overflow");
  uint8_t owners[256]={};const uint32_t start=position();
  auto visit=[&](auto action){
    if(!imported){
      for(auto &shade:model->shades)if(shade.getShadeId()!=255 &&
          !action(shade.proto,shade.gpioUp,shade.gpioDown,shade.gpioMy))return false;
      return true;
    }
    bool valid=seekRead(start)&&skipRecords(*this,header.roomRecords);
    for(unsigned i=0;valid&&i<header.shadeRecords;++i){
      shade_record_t shade;
      valid=readShadeRecord(*this,header,shade);
      if(valid&&shade.shadeId!=255)valid=action(shade.proto,shade.gpioUp,shade.gpioDown,shade.gpioMy);
    }
    const bool rewound=seekRead(start);
    return valid&&rewound&&!readFailed;
  };
  if(!visit([&](radio_proto proto,uint8_t up,uint8_t down,uint8_t my){
    const uint8_t mask=ShadeGpioPolicy::physicalMask(proto),pins[]={up,down,my};
    for(unsigned i=0;i<3;++i)if((mask&(1u<<i))&&pins[i]!=255)++owners[pins[i]];
    return true;
  }))return false;
  pin_users_t invalid;
  invalid.uses=[](uint8_t pin,void *){return !GPIO_IS_VALID_OUTPUT_GPIO(pin);};
  return visit([&](radio_proto proto,uint8_t up,uint8_t down,uint8_t my){
    struct Context{const uint8_t *owners;uint8_t mask,pins[3];} context{owners,ShadeGpioPolicy::physicalMask(proto),{up,down,my}};
    pin_users_t others;others.ctx=&context;
    others.uses=[](uint8_t pin,void *opaque){
      const auto &current=*static_cast<const Context *>(opaque);unsigned own=0;
      for(unsigned i=0;i<3;++i)if((current.mask&(1u<<i))&&current.pins[i]==pin)++own;
      return current.owners[pin]>own;
    };
    return stage.validateGPIOs(proto,up,down,my,settings,model->transceiver.config,others,invalid,model->transceiver.retainedPinClaims(),retainedEthernetPinClaims())==0;
  });
}

void ShadeConfigFile::readRoom(SomfyRoom *room) {
  room_record_t rec = roomRecord(room);
  readRoomRecord(*this, rec);
  room->roomId = rec.roomId;
  strlcpy(room->name, rec.name, sizeof(room->name));
  room->sortOrder = rec.sortOrder;
}
void ShadeConfigFile::readShade(SomfyShade *shade) {
  shade_record_t rec = shadeRecord(shade);
  readShadeRecord(*this, this->header, rec);
  if(rec.shadeId == 255) {
    shade->clear();
    return;
  }
  uint16_t ownCode=rec.lastRollingCode,linkedCodes[SOMFY_MAX_LINKED_REMOTES]={};
  if(rec.remoteAddress && !counters.lookup(rec.remoteAddress,ownCode)){readFailed=true;return;}
  for(unsigned i=0;i<SOMFY_MAX_LINKED_REMOTES;++i)
    if(rec.linkedRemotes[i] && !counters.lookup(rec.linkedRemotes[i],linkedCodes[i])){readFailed=true;return;}
  shade->setShadeId(rec.shadeId);
  shade->paired = rec.paired;
  shade->shadeType = rec.shadeType;
  shade->setRemoteAddress(rec.remoteAddress);
  strlcpy(shade->name, rec.name, sizeof(shade->name));
  shade->tiltType = rec.tiltType;
  shade->proto = rec.proto;
  shade->bitLength = rec.bitLength;
  shade->upTime = rec.upTime;
  shade->downTime = rec.downTime;
  shade->tiltTime = rec.tiltTime;
  shade->stepSize = rec.stepSize;
  for(uint8_t j = 0; j < SOMFY_MAX_LINKED_REMOTES; j++) {
    SomfyLinkedRemote *rem = &shade->linkedRemotes[j];
    rem->setRemoteAddress(rec.linkedRemotes[j]);
    if(rem->getRemoteAddress() != 0) rem->lastRollingCode = linkedCodes[j];
  }
  shade->lastRollingCode = ownCode;
  shade->setFlags(rec.flags);
  // The targets are where the shade is.
  shade->restore(rec.currentPos, rec.currentTiltPos, rec.myPos, rec.myTiltPos);
  shade->flipCommands = rec.flipCommands;
  shade->flipPosition = rec.flipPosition;
  shade->repeats = rec.repeats;
  shade->sortOrder = rec.sortOrder;
  shade->gpioUp = rec.gpioUp;
  shade->gpioDown = rec.gpioDown;
  shade->gpioMy = rec.gpioMy;
  shade->gpioFlags = rec.gpioFlags;
  shade->roomId = rec.roomId;
  // GPIO setup waits until every model record and final hardware settings
  // are loaded; per-record setup could inspect stale other-shade ownership.

}
void ShadeConfigFile::readGroup(SomfyGroup *group) {
  group_record_t rec = groupRecord(group);
  readGroupRecord(*this, this->header, rec);
  if(rec.groupId == 255) {
    group->clear();
    return;
  }
  uint16_t ownCode=rec.lastRollingCode;
  if(rec.remoteAddress && !counters.lookup(rec.remoteAddress,ownCode)){readFailed=true;return;}
  group->setGroupId(rec.groupId);
  group->groupType = rec.groupType;
  group->setRemoteAddress(rec.remoteAddress);
  strlcpy(group->name, rec.name, sizeof(group->name));
  group->proto = rec.proto;
  group->bitLength = rec.bitLength;
  memcpy(group->linkedShades, rec.linkedShades, sizeof(group->linkedShades));
  group->compressLinkedShadeIds();
  group->repeats = rec.repeats;
  group->sortOrder = rec.sortOrder;
  group->flipCommands = rec.flipCommands;
  group->roomId = rec.roomId;
  group->lastRollingCode = ownCode;
}
void ShadeConfigFile::readRepeaters(SomfyShadeController *s) {
  uint32_t repeaters[SOMFY_MAX_REPEATERS];
  readRepeaterRecord(*this, repeaters);
  for(uint8_t i = 0; i < SOMFY_MAX_REPEATERS; i++) s->linkRepeater(repeaters[i]);
}

// The rooms, shades and groups, in place of the ones the controller has.
void ShadeConfigFile::readModel(SomfyShadeController *s) {
  for(uint8_t i = 0; i < this->header.roomRecords; i++) this->readRoom(&s->rooms[i]);
  for(uint8_t i = this->header.roomRecords; i < SOMFY_MAX_ROOMS; i++) s->rooms[i].clear();
  for(uint8_t i = 0; i < this->header.shadeRecords; i++) this->readShade(&s->shades[i]);
  for(uint8_t i = this->header.shadeRecords; i < SOMFY_MAX_SHADES; i++) s->shades[i].clear();
  for(uint8_t i = 0; i < this->header.groupRecords; i++) this->readGroup(&s->groups[i]);
  for(uint8_t i = this->header.groupRecords; i < SOMFY_MAX_GROUPS; i++) s->groups[i].clear();
}
bool ShadeConfigFile::begin(bool readOnly) { return this->begin("/shades.cfg", readOnly); }
bool ShadeConfigFile::begin(const char *filename, bool readOnly) {
  // All disk model writes use the checked stage/verify/replace path. Backup
  // serialization uses beginRAM and must never truncate the canonical file.
  if(!readOnly)return false;
  return ConfigFile::begin(filename,true);
}
void ShadeConfigFile::end() { counters.clear();ConfigFile::end(); }
bool ShadeConfigFile::writeModel(SomfyShadeController *s,
                                 ConfigWriter &writer) {
  this->header.version = SHADE_HDR_VER;
  this->header.length = SHADE_HDR_SIZE;
  this->header.roomRecordSize = ROOM_REC_SIZE;
  this->header.roomRecords = s->roomCount();
  this->header.shadeRecordSize = SHADE_REC_SIZE;
  this->header.shadeRecords = s->shadeCount();
  this->header.groupRecordSize = GROUP_REC_SIZE;
  this->header.groupRecords = s->groupCount();
  this->header.repeaterRecords = 1;
  this->header.repeaterRecordSize = REPEATER_REC_SIZE;
  strlcpy(this->header.serverId, settings.serverId,
          sizeof(this->header.serverId));
  class Records : public ModelRecordSource {
    SomfyShadeController *model;

  public:
    explicit Records(SomfyShadeController *s) : model(s) {}
    bool room(uint8_t slot, room_record_t &out) override {
      auto &item = model->rooms[slot];
      if (!item.roomId)
        return false;
      out = roomRecord(&item);
      return true;
    }
    bool shade(uint8_t slot, shade_record_t &out) override {
      auto &item = model->shades[slot];
      if (item.getShadeId() == 255)
        return false;
      out = shadeRecord(&item);
      return true;
    }
    bool group(uint8_t slot, group_record_t &out) override {
      auto &item = model->groups[slot];
      if (item.getGroupId() == 255)
        return false;
      out = groupRecord(&item);
      return true;
    }
    void repeaters(uint32_t (&out)[SOMFY_MAX_REPEATERS]) override {
      memcpy(out, model->repeaters, sizeof(out));
    }
  } records(s);
  return writeModelRecords(writer, this->header, records);
}
ModelCommitResult ShadeConfigFile::commitModel(SomfyShadeController *model) {
  if(!model->modelLoad.mayCommit())return {ModelCommitState::failed,ModelCommitStage::untrusted};
  class Serializer : public ShadeConfigFile, public ModelSerializer {
    SomfyShadeController *model;

  public:
    explicit Serializer(SomfyShadeController *s) : model(s) {}
    bool write(ConfigWriter &writer) override {
      header.settingsRecordSize = header.netRecordSize =
          header.transRecordSize = 0;
      return writeModel(model, writer);
    }
  } source(model);
  return commitModelFile(LittleFS.mountpoint(), source);
}

bool ShadeConfigFile::save(SomfyShadeController *s) {
  if(!s->modelLoad.mayCommit())return false;
  this->header.settingsRecordSize = 0;
  this->header.netRecordSize = 0;
  this->header.transRecordSize = 0;
  return this->writeModel(s, *this);
}
bool ShadeConfigFile::backup(SomfyShadeController *s) {
  const settings_record_t settingsRec = settingsRecord();
  const net_record_t netRec = netRecord();
  const trans_record_t transRec = transRecord(s->transceiver.config);
  // The header gives each record's size, which a counter measures.
  ConfigCounter settingsBytes, netBytes;
  writeSettingsRecord(settingsBytes, settingsRec);
  writeNetRecord(netBytes, netRec, HAS_ETHERNET);
  this->header.settingsRecordSize = settingsBytes.bytes;
  this->header.netRecordSize = netBytes.bytes;
  this->header.transRecordSize = TRANS_REC_SIZE;
  return this->writeModel(s, *this) &&
         writeSettingsRecord(*this, settingsRec) &&
         writeNetRecord(*this, netRec, HAS_ETHERNET) &&
         writeTransRecord(*this, transRec) && !this->_writeFailed;
}
bool ShadeConfigFile::validate() {
  if(!this->readHeader() || readFailed)return false;
  if(this->header.version < 1 || this->header.version > SHADE_HDR_VER) {
    ESP_LOGE(TAG, "Invalid Header Version:%u", this->header.version);
    return false;
  }
  if(!modelRecordCountsFit(this->header)) {
    ESP_LOGE(TAG, "Model record counts exceed capacity: rooms:%u shades:%u groups:%u",
      this->header.roomRecords, this->header.shadeRecords, this->header.groupRecords);
    return false;
  }
  if(this->header.shadeRecordSize < 100) {
    ESP_LOGE(TAG, "Invalid Shade Record Size:%u", this->header.shadeRecordSize);
    return false;
  }
  if(this->header.version > 10) {
    if(this->header.groupRecordSize < 100) {
      ESP_LOGE(TAG, "Invalid Group Record Size:%u", this->header.groupRecordSize);
      return false;
    }
  }
  if(this->position() != this->header.length) {
    ESP_LOGE(TAG, "File not positioned at %u end of header: %d", this->header.length, this->position());
    return false;
  }

  // Only a warning: backups written before 4.0 declare the transceiver record
  // 6 bytes short, and 4.0 backups from boards without Ethernet declare the
  // network record 28 bytes long.  Records are read to their newline.
  const uint32_t fsize = expectedFileBytes(this->header);
  if(this->readSize() != fsize) {
    ESP_LOGW(TAG, "File is %u bytes but its header adds up to %u", (unsigned)this->readSize(), (unsigned)fsize);
  }
  // Next check to see if the records match the header length.
  uint8_t recs = 0;
  uint32_t startPos = this->position();
  if(this->header.version >= 19) {
    while(recs < this->header.roomRecords) {
      uint32_t pos = this->position();
      if(!this->seekChar(CFG_REC_END)) {
        ESP_LOGE(TAG, "Failed to find the room record end %d", recs);
        return false;
      }
      if(this->position() - pos != this->header.roomRecordSize) {
        ESP_LOGE(TAG, "Room record length is %d and should be %d", this->position() - pos, this->header.roomRecordSize);
        return false;
      }
      recs++;
    }
    recs = 0;
  }
  while(recs < this->header.shadeRecords) {
    uint32_t pos = this->position();
    if(!this->seekChar(CFG_REC_END)) {
      ESP_LOGE(TAG, "Failed to find the shade record end %d", recs);
      return false;
    }
    if(this->position() - pos != this->header.shadeRecordSize) {
      ESP_LOGE(TAG, "Shade record length is %d and should be %d", this->position() - pos, this->header.shadeRecordSize);
      return false;
    }
    const uint32_t endPos = this->position();
    this->seekRead(pos);
    shade_record_t shade;
    if(!readShadeRecord(*this, this->header, shade)) {
      ESP_LOGE(TAG, "Unsupported bit length in shade record %u", recs);
      return false;
    }
    this->seekRead(endPos);
    recs++;
  }
  if(this->header.version > 10) {
    recs = 0;
    while(recs < this->header.groupRecords) {
      uint32_t pos = this->position();
      if(!this->seekChar(CFG_REC_END)) {
        ESP_LOGE(TAG, "Failed to find the group record end %d", recs);
        return false;
      }
      recs++;
      if(this->position() - pos != this->header.groupRecordSize) {
        ESP_LOGE(TAG, "Group record length is %d and should be %d", this->position() - pos, this->header.groupRecordSize);
        return false;
      }
      const uint32_t endPos = this->position();
      this->seekRead(pos);
      group_record_t group;
      if(!readGroupRecord(*this, this->header, group)) {
        ESP_LOGE(TAG, "Unsupported bit length in group record %u", recs - 1);
        return false;
      }
      this->seekRead(endPos);
    }
  }
  if(this->header.version >= 21) {
    recs = 0;
    while(recs < this->header.repeaterRecords) {
      uint32_t pos = this->position();
      if(!this->seekChar(CFG_REC_END)) {
        ESP_LOGE(TAG, "Failed to find the repeater record end %d", recs);
        return false;
      }
      else if(this->position() - pos != this->header.repeaterRecordSize) {
        ESP_LOGW(TAG, "Repeater record length is %d and should be %d", this->position() - pos, this->header.repeaterRecordSize);
        return false;
      }
      recs++;
    }
  }
  // Optional legacy record fields may default at newline, but physical EOF
  // anywhere in a declared record is an error. Check all backup tails before
  // adoption, including unselected settings, without writing settings/NVS.
  if(header.settingsRecordSize){settings_record_t rec{};readSettingsRecord(*this,header,rec);}
  if(header.netRecordSize){net_record_t rec{};readNetRecord(*this,header,rec,HAS_ETHERNET);}
  if(header.transRecordSize){trans_record_t rec{};readTransRecord(*this,rec);}
  const bool valid=!readFailed && position()==readSize();
  this->seekRead(startPos);
  return valid;
}
ModelLoadStatus ShadeConfigFile::loadBoot(SomfyShadeController *s) {
  ShadeConfigFile file;
  if(!file.begin("/shades.cfg",true))return file.snapshotStatus;
  const bool loaded=file.loadFile(s);
  file.end();return loaded?ModelLoadStatus::loaded:ModelLoadStatus::unavailable;
}
bool ShadeConfigFile::load(SomfyShadeController *s, const char *filename) {
  ShadeConfigFile file;
  if(file.begin(filename, true)) {
    bool success = file.loadFile(s, filename);
    file.end();
    return success;
  }
  return false;
}
bool ShadeConfigFile::validateFile(const char *filename) {
  if(!LittleFS.exists(filename)) return false;
  ShadeConfigFile file;
  file.begin(filename, true);
  const bool valid = file.validate();
  file.end();
  return valid;
}
RestoreApplyResult ShadeConfigFile::restore(SomfyShadeController *s,const char *filename,
    restore_options_t &opts,const GitUpdateOwnerToken &owner,const RadioInstallToken &radio){
  ShadeConfigFile file;
  return file.restoreFile(s,filename,opts,owner,radio);
}
RestoreApplyResult ShadeConfigFile::restoreFile(SomfyShadeController *s,const char *filename,
    restore_options_t &opts,const GitUpdateOwnerToken &owner,const RadioInstallToken &radio){
  RestoreApplyResult result;
  if(!gitUpdate.transferRestoreClaimed(owner,uint64_t(esp_timer_get_time())/1000) ||
      !restoreExecutionHolds(owner,radio)) {result.refusal=RestoreRefusal::ownership;return result;}
  if(s->transceiver.counterEditUncertain()){result.refusal=RestoreRefusal::busy;return result;}
  bool opened=false;
  if(!isOpen()){begin(filename,true);opened=true;}
  struct CloseOpened{ShadeConfigFile &file;bool active;~CloseOpened(){if(active)file.end();}}close{*this,opened};
  if(!validate())return result;
  if(!gitUpdate.transferRestoreClaimed(owner,uint64_t(esp_timer_get_time())/1000)){
    result.refusal=RestoreRefusal::ownership;return result;
  }
  if(!restoreIdle(s,radio)){result.refusal=RestoreRefusal::busy;return result;}
  const bool recovering=!s->modelLoad.usable();
  if(recovering && (!opts.shades || !opts.repeaters)){
    result.refusal=RestoreRefusal::incompleteModel;return result;
  }
  if(!opts.shades&&!opts.repeaters&&!opts.settings&&!opts.network&&!opts.mqtt&&!opts.transceiver)return result;
  if(!validateRestoreRecords(opts)){result.refusal=RestoreRefusal::invalid;return result;}
  SelectedSettingsRestore stage;
  const bool selected=opts.settings||opts.network||opts.mqtt||opts.transceiver;
  const uint32_t modelStart=position();
  // Even model-only restore prepares the no-write settings context so pin
  // projection may use proven current owners without allocating a draft.
  if(!selected && !stage.prepare(settings,s->transceiver.config,opts,header,*this,false,s->transceiver.retainedPinClaims(),retainedEthernetPinClaims(),retainedGpioPinClaims())){
    result.settings=stage.status();result.refusal=RestoreRefusal::settings;return result;
  }
  if(selected){
    const bool sameBoard=header.serverId[0]&&settings.serverId[0]&&!strcmp(header.serverId,settings.serverId);
    if(!skipRecords(*this,uint32_t(header.roomRecords)+header.shadeRecords+header.groupRecords+header.repeaterRecords) ||
       !stage.prepare(settings,s->transceiver.config,opts,header,*this,
          sameBoard,s->transceiver.retainedPinClaims(),retainedEthernetPinClaims(),retainedGpioPinClaims()) || readFailed){
      result.settings=stage.status();result.refusal=RestoreRefusal::settings;return result;
    }
    result.settings=stage.status();
    if(!seekRead(modelStart)){result.refusal=RestoreRefusal::invalid;return result;}
  }
  if((opts.shades||opts.network||opts.transceiver) && !validateRestoreGPIOs(s,opts.shades,stage)){
    result.refusal=RestoreRefusal::invalid;return result;
  }
  if(opts.shades){
    const bool prepared=prepareCounters({&owner,continueRestoreCounterLoad,cooperateCounterLoad});
    result.counterWriteAttempted=counters.result().writeAttempted;
    if(!prepared){
      if(result.counterWriteAttempted)s->modelLoad.fail();
      result.refusal=RestoreRefusal::counterStorage;return result;
    }
  }
  // Preflight itself consumes time for every selection, including repeater-only
  // and settings-only restore. No revision or live mutation uses an expired grant.
  if(!gitUpdate.transferRestoreClaimed(owner,uint64_t(esp_timer_get_time())/1000) || !restoreIdle(s,radio)){
    if(result.counterWriteAttempted)s->modelLoad.fail();
    result.refusal=RestoreRefusal::ownership;return result;
  }
  if(opts.shades)s->modelLoad.fail();
  invalidateDeviceSettingsRevisions();++s->configurationRevision;++s->restoreRevision;
  if(opts.shades){
    result.modelApplied=true;readModel(s);
    if(readFailed){result.refusal=RestoreRefusal::invalid;return result;}
  }else skipRecords(*this,header.roomRecords+header.shadeRecords+header.groupRecords);
  if(opts.repeaters){
    result.modelApplied=true;memset(s->repeaters,0,sizeof(s->repeaters));
    for(unsigned i=0;i<header.repeaterRecords;++i)readRepeaters(s);
  }else skipRecords(*this,header.repeaterRecords);
  if(readFailed){if(result.modelApplied)s->modelLoad.fail();return result;}
  if(opts.shades||opts.repeaters){
    if(opts.shades)s->modelLoad.beginRecoveryCommit();
    result.modelFile=s->commit();
    if(opts.shades)s->modelLoad.finishRecoveryCommit(result.modelFile.saved());
    if(!result.modelFile.saved()){result.refusal=RestoreRefusal::modelStorage;return result;}
  }
  if(selected)result.settings=applyPreparedSettingsRestore(stage,s->transceiver,owner,radio);
  // Final actual ownership, including a live radio adoption followed by a
  // marker failure, governs physical setup. Never project from stale pins.
  if(s->modelLoad.usable() && (opts.shades || ((opts.network||opts.transceiver) &&
      (result.settings.appliedNamespaces || result.settings.persistence.liveApplied))))s->configureGPIOs();
  result.complete=result.settings.saved();
  result.refusal=result.complete?RestoreRefusal::none:RestoreRefusal::settings;return result;
}
bool ShadeConfigFile::loadFile(SomfyShadeController *s, const char *filename) {
  bool opened = false;
  if(!this->isOpen()) {
    ESP_LOGI(TAG, "Opening shade config file");
    this->begin(filename, true);
    opened = true;
  }
  if(!this->validate()) {
    ESP_LOGE(TAG, "Shade config file invalid!");
    if(opened) this->end();
    return false;
  }
  s->modelLoad.fail();
  if(!prepareCounters({nullptr,nullptr,cooperateCounterLoad})){if(opened)this->end();return false;}
  this->readModel(s);
  if(readFailed){if(opened)this->end();return false;}
  if(this->header.repeaterRecords > 0) {
    memset(s->repeaters, 0x00, sizeof(s->repeaters));
    for(uint8_t i = 0; i < this->header.repeaterRecords; i++) this->readRepeaters(s);
  }
  if(opened) {
    ESP_LOGI(TAG, "Closing shade config file");
    this->end();
  }
  return !readFailed;
}
bool ShadeConfigFile::exists() { return LittleFS.exists("/shades.cfg"); }
