#include "SelectedSettingsRestore.h"
#include "Transceiver.h"
#include "SettingsTypedValidation.h"
#include "SettingsDurability.h"
#include "ShadeGpioPolicy.h"
#include "GitUpdate.h"
#include <stdlib.h>
#include <esp_timer.h>
#include <new>
#include <string.h>
#if defined(ESPSOMFY_LILYGO_T_EMBED_CC1101)
#include <esp_heap_caps.h>
#endif
namespace {
void *allocateDraft(size_t bytes) {
#if defined(ESPSOMFY_LILYGO_T_EMBED_CC1101)
  return heap_caps_malloc(bytes,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
#else
  return malloc(bytes);
#endif
}
void releaseDraft(void *memory,size_t){free(memory);}
uint8_t namespaceBit(SettingsNamespace space){return settingsNamespaceBit(space);}
template<size_t D,size_t S> bool text(char (&target)[D],const char (&source)[S]) {
  const auto *end=static_cast<const char *>(memchr(source,0,S));
  if(!end || size_t(end-source)>=D)return false;
  memcpy(target,source,size_t(end-source)+1);return true;
}
#if HAS_ETHERNET
bool sameIdentity(const config_header_t &header,const ConfigSettings &settings) {
  return header.serverId[0] && memchr(header.serverId,0,sizeof(header.serverId)) &&
    memchr(settings.serverId,0,sizeof(settings.serverId)) && !strcmp(header.serverId,settings.serverId);
}
#endif
}
struct SelectedSettingsRestore::Draft {
  ConfigSettings settings;
  transceiver_config_t radio;
  restore_options_t options;
  Draft(const ConfigSettings &current,const transceiver_config_t &rf,const restore_options_t &opts):radio(rf),options(opts) {
    // Copy only fields the selected restore may persist or use as pin claims.
    // Boot metadata, WiFi and security credentials never enter this draft.
    settings.loadStatus=current.loadStatus;memcpy(settings.hostname,current.hostname,sizeof(settings.hostname));
    settings.ssdpBroadcast=current.ssdpBroadcast;settings.checkForUpdate=current.checkForUpdate;settings.connType=current.connType;
    if(opts.settings)settings.NTP=current.NTP;
    if(opts.network)settings.IP=current.IP;
    if(opts.mqtt)settings.MQTT=current.MQTT;
#if HAS_ETHERNET
    settings.Ethernet=current.Ethernet;
#endif
  }
};
SelectedSettingsRestore::SelectedSettingsRestore():SelectedSettingsRestore(allocateDraft,releaseDraft){}
SelectedSettingsRestore::SelectedSettingsRestore(void *(*allocate)(size_t),void (*release)(void *,size_t)):allocateBytes(allocate),releaseBytes(release) {
  outcome.state=RestoreSettingsState::failed;outcome.persistence={settings_save_state_t::failed,"restore preparation"};
}
SelectedSettingsRestore::~SelectedSettingsRestore(){clear();}
void SelectedSettingsRestore::clear(){
  if(draft){draft->~Draft();volatile uint8_t *bytes=reinterpret_cast<volatile uint8_t *>(draft);
    for(size_t i=0;i<sizeof(Draft);++i)bytes[i]=0;
    releaseBytes(draft,sizeof(Draft));draft=nullptr;}
  prepared=false;
}
bool SelectedSettingsRestore::prepare(const ConfigSettings &current,const transceiver_config_t &radio,
    const restore_options_t &opts,const config_header_t &header,ConfigReader &reader,bool sameBoard,uint64_t retainedRadio,uint64_t retainedEthernet) {
  static_assert(sizeof(Draft)<=maximumDraftBytes,"Keep one private restore settings draft bounded");
  clear();outcome={};
  if(opts.settings)outcome.requestedNamespaces|=namespaceBit(SettingsNamespace::cfg)|namespaceBit(SettingsNamespace::ntp);
  if(opts.network)outcome.requestedNamespaces|=namespaceBit(SettingsNamespace::cfg)|namespaceBit(SettingsNamespace::ip)
#if HAS_ETHERNET
    |namespaceBit(SettingsNamespace::ethernet)
#endif
    ;
  if(opts.mqtt)outcome.requestedNamespaces|=namespaceBit(SettingsNamespace::mqtt);
  if(opts.transceiver)outcome.requestedNamespaces|=namespaceBit(SettingsNamespace::radio);
  auto fail=[&](RestoreSettingsState state,const char *key){outcome.state=state;outcome.persistence={settings_save_state_t::failed,key};clear();return false;};
  if(!outcome.requestedNamespaces){prepared=true;return true;}
  if((opts.settings && !header.settingsRecordSize) || ((opts.network||opts.mqtt) && !header.netRecordSize) ||
      (opts.transceiver && !header.transRecordSize) || header.version<=13)
    return fail(RestoreSettingsState::unavailable,"selected record");
  if(opts.mqtt && (header.version<22 || !current.MQTT.loadStatus.readable()))
    return fail(RestoreSettingsState::unavailable,"MQTT carried values");
  if((opts.settings||opts.network) && !current.loadStatus.readable() && !(opts.settings&&opts.network&&header.version>=20))
    return fail(RestoreSettingsState::unavailable,"CFG carried values");
  if(!allocateBytes || !releaseBytes)return fail(RestoreSettingsState::failed,"restore memory");
  void *memory=allocateBytes(sizeof(Draft));if(!memory)return fail(RestoreSettingsState::failed,"restore memory");
  draft=new(memory)Draft(current,radio,opts);auto &next=draft->settings;
  if(header.settingsRecordSize) {
    settings_record_t rec;
    if(opts.settings)reader.beginCheckedValues();
    readSettingsRecord(reader,header,rec);
    if(opts.settings) {
      const auto parsed=reader.endCheckedValues();
      if(!parsed.valid || parsed.fields!=(header.version>=20?6:5))return fail(RestoreSettingsState::refused,"settings record");
      if(!text(next.hostname,rec.hostname) || !text(next.NTP.ntpServer,rec.ntpServer) || !text(next.NTP.posixZone,rec.posixZone))
        return fail(RestoreSettingsState::refused,"settings text");
      next.ssdpBroadcast=rec.ssdpBroadcast;if(header.version>=20)next.checkForUpdate=rec.checkForUpdate;
      next.NTP.loadStatus={SettingsLoadState::loaded};
    }
  }
  if(header.netRecordSize) {
    net_record_t rec;
    if(opts.network||opts.mqtt)reader.beginCheckedValues();
    readNetRecord(reader,header,rec,HAS_ETHERNET);
    if(opts.network||opts.mqtt) {
      const auto parsed=reader.endCheckedValues();const uint16_t base=header.version>=22?13:7;
      bool complete=parsed.fields==base;
#if HAS_ETHERNET
      complete=complete || parsed.fields==base+1 || parsed.fields==base+7;
#endif
      if(!parsed.valid || !complete)return fail(RestoreSettingsState::refused,"network record");
      if(opts.network) {
        next.connType=conn_types_t(rec.connType);next.IP.dhcp=rec.dhcp;
        const char *addresses[]={rec.ip,rec.gateway,rec.subnet,rec.dns1,rec.dns2};
        for(uint8_t i=0;i<5;++i)if(!next.IP.setAddress(i,addresses[i]))return fail(RestoreSettingsState::refused,"IP value");
        next.IP.loadStatus={SettingsLoadState::loaded};
#if HAS_ETHERNET
        if(sameBoard && sameIdentity(header,current) && parsed.fields==base+7) {
          next.Ethernet.boardType=rec.ethBoardType;next.Ethernet.phyType=rec.ethPhyType;next.Ethernet.CLKMode=rec.ethClkMode;
          next.Ethernet.phyAddress=rec.ethPhyAddress;next.Ethernet.PWRPin=rec.ethPWRPin;next.Ethernet.MDCPin=rec.ethMDCPin;next.Ethernet.MDIOPin=rec.ethMDIOPin;
          next.Ethernet.loadStatus={SettingsLoadState::loaded};
        } else if(!current.Ethernet.loadStatus.readable())return fail(RestoreSettingsState::unavailable,"ETH carried values");
#endif
      }
      if(opts.mqtt) {
        if(!text(next.MQTT.protocol,rec.mqttProtocol) || !text(next.MQTT.hostname,rec.mqttHostname) ||
          !text(next.MQTT.rootTopic,rec.mqttRootTopic) || !text(next.MQTT.discoTopic,rec.mqttDiscoTopic))return fail(RestoreSettingsState::refused,"MQTT text");
        next.MQTT.port=rec.mqttPort;next.MQTT.pubDisco=rec.mqttPubDisco;
      }
    }
  }
  if(header.transRecordSize) {
    trans_record_t rec;
    if(opts.transceiver)reader.beginCheckedValues();
    readTransRecord(reader,rec);
    if(opts.transceiver) {
      const auto parsed=reader.endCheckedValues();if(!parsed.valid || parsed.fields<13 || parsed.fields>14)return fail(RestoreSettingsState::refused,"radio record");
      auto &rf=draft->radio;rf.enabled=rec.enabled;rf.proto=rec.proto;rf.type=rec.type;
      rf.SCKPin=rec.SCKPin;rf.CSNPin=rec.CSNPin;rf.MOSIPin=rec.MOSIPin;rf.MISOPin=rec.MISOPin;rf.TXPin=rec.TXPin;rf.RXPin=rec.RXPin;
      rf.frequency=rec.frequency;rf.rxBandwidth=rec.rxBandwidth;rf.deviation=rec.deviation;rf.txPower=rec.txPower;rf.noiseDetection=rec.noiseDetection;
      rf.applyBoardPins();rf.loadStatus={SettingsLoadState::loaded};
    }
  }
  if(opts.settings||opts.network) {if(!validSettings(next))return fail(RestoreSettingsState::refused,"CFG value");next.loadStatus={SettingsLoadState::loaded};}
  if(opts.settings && !validSettings(next.NTP))return fail(RestoreSettingsState::refused,"NTP value");
  if(opts.network && !validSettings(next.IP))return fail(RestoreSettingsState::refused,"IP value");
#if HAS_ETHERNET
  if(opts.network && !validSettings(next.Ethernet))return fail(RestoreSettingsState::refused,"ETH value");
#else
  (void)sameBoard;
#endif
  if(opts.mqtt && !validSettings(next.MQTT))return fail(RestoreSettingsState::refused,"MQTT value");
  if(opts.transceiver && !validSettings(draft->radio))return fail(RestoreSettingsState::refused,"radio value");
  bool newEthernet=false;
#if HAS_ETHERNET
  newEthernet=opts.network && (next.connType==conn_types_t::ethernet || next.connType==conn_types_t::ethernetpref);
#endif
  if(opts.transceiver || newEthernet) {
    const auto context=radioPinContext(next,retainedEthernet);
    if(!context.qualified || !draft->radio.loadStatus.usable() || !validSettings(draft->radio))
      return fail(RestoreSettingsState::unavailable,"hardware pin owners");
    if((draft->radio.pinClaims()|retainedRadio)&context.ethernetClaims)
      return fail(RestoreSettingsState::refused,"hardware pin overlap");
  }
  outcome.state=RestoreSettingsState::prepared;prepared=true;return true;
}
RestoreSettingsResult SelectedSettingsRestore::apply(ConfigSettings &current,Transceiver &radio,
    const GitUpdateOwnerToken &owner,const RadioInstallToken &pause,SettingsEffects &effects) {
  if(!prepared){outcome.state=RestoreSettingsState::refused;outcome.persistence={settings_save_state_t::failed,"restore consumed"};return outcome;}
  prepared=false;
  if(!outcome.requestedNamespaces)return outcome;
  auto finish=[&](){clear();return outcome;};
  auto fresh=[&](){
    const auto micros=esp_timer_get_time();const uint64_t now=micros<0?UINT64_MAX:uint64_t(micros)/1000;
    return owner.kind==GitUpdateExternalOwner::restore && current.Security.loadTrusted() &&
      gitUpdate.ownsExternal(owner) && gitUpdate.transferRestoreClaimed(owner,now) && radio.installPauseHeld(pause);
  };
  if(!draft || !fresh()) {
    outcome.state=RestoreSettingsState::refused;outcome.persistence={settings_save_state_t::failed,"restore ownership"};return finish();
  }
  auto failure=[&](settings_save_result_t result){
    const uint8_t changed=outcome.verifiedNamespaces & uint8_t(~outcome.appliedNamespaces);
    if(changed)settingsDurability().markUncertain(changed);
    if(outcome.verifiedNamespaces || result.state==settings_save_state_t::partial)result.state=settings_save_state_t::partial;
    outcome.persistence=result;outcome.state=result.state==settings_save_state_t::partial?RestoreSettingsState::partial:RestoreSettingsState::failed;
  };
  auto persist=[&](SettingsNamespace space,auto &value){
    if(!(outcome.requestedNamespaces&namespaceBit(space)))return true;
    if(!fresh()){failure({settings_save_state_t::failed,"restore source"});return false;}
    const auto result=value.save();if(!result.saved()){failure(result);return false;}
    outcome.verifiedNamespaces|=namespaceBit(space);return true;
  };
  auto &next=draft->settings;
  if(!persist(SettingsNamespace::cfg,next) || !persist(SettingsNamespace::ntp,next.NTP) ||
      !persist(SettingsNamespace::ip,next.IP) || !persist(SettingsNamespace::mqtt,next.MQTT)
#if HAS_ETHERNET
      || !persist(SettingsNamespace::ethernet,next.Ethernet)
#endif
      )return finish();
  if(draft->options.transceiver) {
    if(!fresh()){failure({settings_save_state_t::failed,"restore source"});return finish();}
    const auto result=radio.setConfigWhilePaused(draft->radio,pause,owner,radioPinContext(next,retainedEthernetPinClaims()));
    if(result.radioNamespaceVerified || result.liveApplied)outcome.verifiedNamespaces|=namespaceBit(SettingsNamespace::radio);
    if(result.liveApplied)outcome.appliedNamespaces|=namespaceBit(SettingsNamespace::radio);
    if(!result.saved()){failure(result);return finish();}
    outcome.verifiedNamespaces|=namespaceBit(SettingsNamespace::radio);outcome.persistence=result;
  }
  if(!fresh()){failure({settings_save_state_t::failed,"restore source"});return finish();}
  // One admitted RAM/effects unit follows the verified namespace writes.
  if(outcome.requestedNamespaces&namespaceBit(SettingsNamespace::cfg)) {
    text(current.hostname,next.hostname);current.ssdpBroadcast=next.ssdpBroadcast;current.checkForUpdate=next.checkForUpdate;
    current.connType=next.connType;current.loadStatus=next.loadStatus;
  }
  const auto opts=draft->options;
  if(opts.settings)current.NTP=next.NTP;
  if(opts.network){current.IP=next.IP;
#if HAS_ETHERNET
    current.Ethernet=next.Ethernet;
#endif
  }
  if(opts.mqtt)current.MQTT=next.MQTT;
  if(opts.settings){effects.hostnameChanged();effects.timeChanged();}
  if(opts.mqtt)effects.mqttChanged();
  // A returned applied bit includes completion of the corresponding live hook;
  // model restore can avoid a duplicate MQTT disconnect without guessing.
  outcome.appliedNamespaces=outcome.requestedNamespaces;outcome.state=RestoreSettingsState::saved;
  if(opts.settings)effects.completed(settings_section_t::general,false);
  if(opts.network)effects.completed(settings_section_t::network,false);
  if(opts.mqtt)effects.completed(settings_section_t::mqtt,false);
  if(opts.transceiver)effects.completed(settings_section_t::radio,false);
  return finish();
}
int8_t SelectedSettingsRestore::validateGPIOs(radio_proto protocol,uint8_t up,uint8_t down,uint8_t my,
    const ConfigSettings &current,const transceiver_config_t &radio,
    const pin_users_t &otherPins,const pin_users_t &invalidOutputs,uint64_t retainedRadio,uint64_t retainedEthernet)const {
  const auto &cfg=draft?draft->settings:current;const auto &rf=draft?draft->radio:radio;
  const uint8_t mask=ShadeGpioPolicy::physicalMask(protocol);
  const bool physical=((mask&ShadeGpioPolicy::up)&&up!=255) || ((mask&ShadeGpioPolicy::down)&&down!=255) || ((mask&ShadeGpioPolicy::my)&&my!=255);
  if(physical && (!prepared || !rf.loadStatus.usable()
#if HAS_ETHERNET
      || !cfg.loadStatus.usable() || !cfg.Ethernet.loadStatus.usable()
#endif
      ))return PIN_STORAGE_UNAVAILABLE;
  struct RadioClaims {const transceiver_config_t *cfg;uint64_t retained;} radioClaims{&rf,retainedRadio};
  struct EthernetClaims {const ConfigSettings *cfg;uint64_t retained;} ethClaims{&cfg,retainedEthernet};
  pin_users_t radioPins,ethernetPins;
  radioPins.ctx=&radioClaims;radioPins.uses=[](uint8_t p,void *ctx){const auto &claims=*static_cast<const RadioClaims *>(ctx);return claims.cfg->usesPin(p)||LivePinClaims::contains(claims.retained,p);};
  ethernetPins.ctx=&ethClaims;ethernetPins.uses=[](uint8_t p,void *ctx){const auto &claims=*static_cast<const EthernetClaims *>(ctx);
    return LivePinClaims::contains(claims.retained,p)
#if HAS_ETHERNET
      || ((claims.cfg->connType==conn_types_t::ethernet || claims.cfg->connType==conn_types_t::ethernetpref) && claims.cfg->Ethernet.usesPin(p))
#endif
      ;
  };
  return gpioPinConflict(protocol,up,down,my,radioPins,ethernetPins,otherPins,invalidOutputs);
}
