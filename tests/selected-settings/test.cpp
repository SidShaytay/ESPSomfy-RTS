#include <unity.h>
#include <nvs.h>
#include <string>
#include "SelectedSettingsRestore.h"
#include "QualifiedSettings.h"
#include "Transceiver.h"
#include "GitUpdate.h"
#include "SettingsDurability.h"
#include <esp_timer.h>
struct Bytes:ConfigReader,ConfigWriter {std::string text;size_t at=0;bool readByte(uint8_t &v)override{if(at>=text.size())return false;v=uint8_t(text[at++]);return true;}uint32_t position()override{return at;}bool writeBytes(const uint8_t *p,size_t n)override{text.append(reinterpret_cast<const char*>(p),n);return true;}};
static bool ownerHeld=true,pauseHeld=true,claimHeld=true;static unsigned radioWrites=0;static bool markerFailed=false;
GitUpdateClass gitUpdate;
bool GitUpdateClass::ownsExternal(const GitUpdateOwnerToken &owner)const{return ownerHeld && owner.generation==7 && owner.kind==GitUpdateExternalOwner::restore;}
bool GitUpdateClass::transferRestoreClaimed(const GitUpdateOwnerToken &owner,uint64_t){return ownsExternal(owner) && claimHeld;}
bool Transceiver::installPauseHeld(const RadioInstallToken &pause)const{return pauseHeld && pause.generation==9;}
settings_save_result_t Transceiver::setConfigWhilePaused(const transceiver_config_t &next,const RadioInstallToken &,const GitUpdateOwnerToken &,RadioPinContext){++radioWrites;auto copy=next;auto saved=copy.save();if(!saved.saved())return saved;config=copy;if(markerFailed){settingsDurability().markUncertain(settingsNamespaceBit(SettingsNamespace::radio));return {settings_save_state_t::partial,"radioInit",true};}return {settings_save_state_t::saved,nullptr,true};}
uint64_t retainedEthernetPinClaims(){return 0;}
namespace BoardProfile {bool usesPin(uint8_t){return false;}}
bool NTPSettings::apply(){return true;}
#if HAS_ETHERNET
#include "ethernet-claims.inc"
#endif
struct Effects:SettingsEffects {unsigned host=0,time=0,mqtt=0,completedCalls=0;bool networkPinsAvailable(const ConfigSettings &)const override{return true;}bool wifiClientsPresent()const override{return true;}void hostnameChanged()override{++host;}void timeChanged()override{++time;}void mqttChanged()override{++mqtt;}settings_save_result_t saveRadio(JsonObject)override{TEST_FAIL_MESSAGE("Restore must not reacquire live RadioSave admission");return {};}RestartResult completed(settings_section_t,bool reboot)override{TEST_ASSERT_FALSE(reboot);++completedCalls;return {};}};
void setUp(){FakeNvs::reset();settingsDurability()=SettingsDurability{};ownerHeld=pauseHeld=claimHeld=true;radioWrites=0;markerFailed=false;}void tearDown(){TEST_ASSERT_TRUE(FakeNvs::handles.empty());}
static ConfigSettings current(){ConfigSettings c;qualifySettings(c);c.Security.loadState=SecurityLoadState::knownEmpty;strcpy(c.serverId,"ABCDEF");strcpy(c.WIFI.ssid,"keep-wifi");strcpy(c.WIFI.passphrase,"keep-wifi-secret");strcpy(c.MQTT.username,"keep-user");strcpy(c.MQTT.password,"keep-password");c.MQTT.enabled=true;return c;}
static config_header_t header(){config_header_t h;h.version=24;h.settingsRecordSize=1;h.netRecordSize=1;h.transRecordSize=80;strcpy(h.serverId,"ABCDEF");return h;}
static Bytes records(){Bytes b;settings_record_t s;strcpy(s.hostname,"restored");strcpy(s.ntpServer,"restore.clock");strcpy(s.posixZone,"UTC0");s.ssdpBroadcast=true;s.checkForUpdate=false;writeSettingsRecord(b,s);net_record_t n;n.connType=1;strcpy(n.ip,"10.2.3.4");strcpy(n.gateway,"10.2.3.1");strcpy(n.subnet,"255.255.255.0");strcpy(n.mqttProtocol,"mqtt");strcpy(n.mqttHostname,"restored-broker");strcpy(n.mqttRootTopic,"restored-root");strcpy(n.mqttDiscoTopic,"discovery");writeNetRecord(b,n,HAS_ETHERNET);trans_record_t r;r.enabled=true;writeTransRecord(b,r);return b;}
static void fully_covered_restore_preserves_omitted_credentials_and_notifies_only_after_save(){
 auto cfg=current();transceiver_config_t radio;qualifySettings(radio);restore_options_t opts;opts.settings=opts.network=opts.mqtt=opts.transceiver=true;auto bytes=records();auto h=header();
 SelectedSettingsRestore stage;TEST_ASSERT_TRUE(stage.prepare(cfg,radio,opts,h,bytes,true,0,0,0));TEST_ASSERT_EQUAL(0,FakeNvs::writeOpens);TEST_ASSERT_EQUAL_STRING("ESPSomfyRTS",cfg.hostname);
 Transceiver live;live.config=radio;Effects effects;auto out=stage.apply(cfg,live,{7,GitUpdateExternalOwner::restore},{9},effects);
 TEST_ASSERT_TRUE(out.saved());TEST_ASSERT_EQUAL(out.requestedNamespaces,out.verifiedNamespaces);TEST_ASSERT_EQUAL(out.requestedNamespaces,out.appliedNamespaces);TEST_ASSERT_EQUAL(4,effects.completedCalls);
 TEST_ASSERT_EQUAL_STRING("restored",cfg.hostname);TEST_ASSERT_EQUAL_STRING("keep-wifi",cfg.WIFI.ssid);TEST_ASSERT_EQUAL_STRING("keep-wifi-secret",cfg.WIFI.passphrase);TEST_ASSERT_EQUAL_STRING("keep-user",cfg.MQTT.username);TEST_ASSERT_EQUAL_STRING("keep-password",cfg.MQTT.password);TEST_ASSERT_TRUE(cfg.MQTT.enabled);TEST_ASSERT_EQUAL(1,radioWrites);
 TEST_ASSERT_EQUAL(1,effects.host);TEST_ASSERT_EQUAL(1,effects.time);TEST_ASSERT_EQUAL(1,effects.mqtt);
 const auto sets=FakeNvs::sets;TEST_ASSERT_FALSE(stage.apply(cfg,live,{7,GitUpdateExternalOwner::restore},{9},effects).saved());TEST_ASSERT_EQUAL(sets,FakeNvs::sets);
}
static void unknown_omitted_fields_or_incomplete_records_never_write_or_adopt(){
 for(unsigned failure=0;failure<6;++failure){auto cfg=current();transceiver_config_t radio;qualifySettings(radio);restore_options_t opts;opts.settings=opts.network=opts.mqtt=true;auto bytes=records();auto h=header();
  if(failure==0)cfg.MQTT.loadStatus.state=SettingsLoadState::failed;
  if(failure==1){cfg.loadStatus.state=SettingsLoadState::failed;opts.settings=false;}
  if(failure==2)h.settingsRecordSize=0;
  if(failure==3)bytes.text.replace(bytes.text.find("restored-broker"),15,100,'x');
  if(failure==4)bytes.text=bytes.text.substr(0,bytes.text.find('\n'))+"\n1\n";
  if(failure==5){h.version=19;cfg.loadStatus.state=SettingsLoadState::failed;}
  SelectedSettingsRestore stage;TEST_ASSERT_FALSE(stage.prepare(cfg,radio,opts,h,bytes,true,0,0,0));TEST_ASSERT_FALSE(stage.status().saved());TEST_ASSERT_EQUAL(0,FakeNvs::writeOpens);TEST_ASSERT_EQUAL_STRING("ESPSomfyRTS",cfg.hostname);
 }
}
static void all_selected_values_are_preflighted_before_any_persistence(){
 auto cfg=current();transceiver_config_t radio;qualifySettings(radio);auto bytes=records();auto h=header();restore_options_t opts;opts.settings=opts.network=opts.transceiver=true;
 auto start=bytes.text.rfind('\n',bytes.text.size()-2)+1;auto comma=bytes.text.find(',',start);auto nextComma=bytes.text.find(',',comma+1);bytes.text.replace(comma+1,nextComma-comma-1,"257");
 SelectedSettingsRestore stage;TEST_ASSERT_FALSE(stage.prepare(cfg,radio,opts,h,bytes,true,0,0,0));TEST_ASSERT_EQUAL(0,FakeNvs::sets);TEST_ASSERT_EQUAL(0,radioWrites);
}
static void partial_namespace_or_radio_marker_failure_never_adopts_other_drafts(){
 for(bool marker:{false,true}){FakeNvs::reset();settingsDurability()=SettingsDurability{};auto cfg=current();transceiver_config_t radio;qualifySettings(radio);restore_options_t opts;opts.settings=opts.transceiver=true;auto bytes=records();SelectedSettingsRestore stage;TEST_ASSERT_TRUE(stage.prepare(cfg,radio,opts,header(),bytes,true,0,0,0));
  if(marker)markerFailed=true;else FakeNvs::failOpen=2;
  Transceiver live;live.config=radio;Effects effects;auto out=stage.apply(cfg,live,{7,GitUpdateExternalOwner::restore},{9},effects);
  TEST_ASSERT_FALSE(out.saved());TEST_ASSERT_EQUAL(RestoreSettingsState::partial,out.state);TEST_ASSERT_EQUAL_STRING("ESPSomfyRTS",cfg.hostname);TEST_ASSERT_EQUAL(0,effects.completedCalls);TEST_ASSERT_FALSE(settingsDurability().status().restartAllowed());
  TEST_ASSERT_EQUAL(marker?settingsNamespaceBit(SettingsNamespace::radio):0,out.appliedNamespaces);TEST_ASSERT_EQUAL(marker,out.persistence.liveApplied);
 }
}
static void full_coverage_can_repair_cfg_ntp_ip_but_not_uncovered_wifi_or_security(){
 auto cfg=current();cfg.loadStatus.state=cfg.NTP.loadStatus.state=cfg.IP.loadStatus.state=SettingsLoadState::failed;cfg.WIFI.loadStatus.state=SettingsLoadState::failed;transceiver_config_t radio;qualifySettings(radio);restore_options_t opts;opts.settings=opts.network=true;auto bytes=records();SelectedSettingsRestore stage;
 TEST_ASSERT_TRUE(stage.prepare(cfg,radio,opts,header(),bytes,true,0,0,0));Transceiver live;live.config=radio;Effects effects;TEST_ASSERT_TRUE(stage.apply(cfg,live,{7,GitUpdateExternalOwner::restore},{9},effects).saved());
 TEST_ASSERT_TRUE(cfg.loadStatus.usable());TEST_ASSERT_TRUE(cfg.NTP.loadStatus.usable());TEST_ASSERT_TRUE(cfg.IP.loadStatus.usable());TEST_ASSERT_FALSE(cfg.WIFI.loadStatus.readable());TEST_ASSERT_TRUE(cfg.Security.loadTrusted());TEST_ASSERT_FALSE(cfg.settingsUsable());
}
static void fresh_owner_and_pause_are_required_before_first_write(){
 for(unsigned failure=0;failure<4;++failure){auto cfg=current();transceiver_config_t rf;qualifySettings(rf);restore_options_t opts;opts.settings=true;auto bytes=records();SelectedSettingsRestore stage;TEST_ASSERT_TRUE(stage.prepare(cfg,rf,opts,header(),bytes,true,0,0,0));
  if(failure==0)ownerHeld=false;if(failure==1)pauseHeld=false;if(failure==2)cfg.Security.loadState=SecurityLoadState::failed;if(failure==3)claimHeld=false;
  Transceiver live;live.config=rf;Effects effects;auto result=stage.apply(cfg,live,{7,GitUpdateExternalOwner::restore},{9},effects);
  TEST_ASSERT_FALSE(result.saved());TEST_ASSERT_EQUAL(0,FakeNvs::writeOpens);TEST_ASSERT_EQUAL(0,effects.completedCalls);ownerHeld=pauseHeld=claimHeld=true;
 }
}
static unsigned wipedReleases=0;
static void *privateAllocate(size_t size){return malloc(size);}
static void privateRelease(void *p,size_t size){for(size_t i=0;i<size;++i)TEST_ASSERT_EQUAL(0,static_cast<uint8_t*>(p)[i]);++wipedReleases;free(p);}
static void *denyAllocation(size_t){return nullptr;}
static void failed_and_successful_staging_wipe_private_memory_and_allocation_failure_has_no_effects(){
 auto cfg=current();transceiver_config_t rf;qualifySettings(rf);restore_options_t opts;opts.settings=opts.network=true;
 auto bytes=records();SelectedSettingsRestore denied(denyAllocation,privateRelease);TEST_ASSERT_FALSE(denied.prepare(cfg,rf,opts,header(),bytes,true,0,0,0));TEST_ASSERT_EQUAL(0,FakeNvs::sets);
 wipedReleases=0;{
  SelectedSettingsRestore stage(privateAllocate,privateRelease);bytes=records();TEST_ASSERT_TRUE(stage.prepare(cfg,rf,opts,header(),bytes,true,0,0,0));
 }TEST_ASSERT_EQUAL(1,wipedReleases);
 bytes=records();bytes.text.replace(bytes.text.find("restore.clock"),13,100,'x');SelectedSettingsRestore invalid(privateAllocate,privateRelease);TEST_ASSERT_FALSE(invalid.prepare(cfg,rf,opts,header(),bytes,true,0,0,0));TEST_ASSERT_EQUAL(2,wipedReleases);TEST_ASSERT_EQUAL(0,FakeNvs::sets);
}
static void projected_gpio_uses_candidate_pins_and_none_never_requires_an_owner(){
 auto cfg=current();transceiver_config_t rf;qualifySettings(rf);rf.enabled=true;rf.TXPin=17;restore_options_t opts;opts.transceiver=true;auto bytes=records();SelectedSettingsRestore stage;TEST_ASSERT_TRUE(stage.prepare(cfg,rf,opts,header(),bytes,true,0,0,0));
 pin_users_t empty;
 TEST_ASSERT_EQUAL(PIN_CONFLICT_RADIO,stage.validateGPIOs(radio_proto::GP_Relay,17,255,35,cfg,rf,empty,empty,LivePinClaims::pinMask(17),0));
 TEST_ASSERT_EQUAL(PIN_CONFLICT_ETHERNET,stage.validateGPIOs(radio_proto::GP_Relay,17,255,35,cfg,rf,empty,empty,0,LivePinClaims::pinMask(17)));
 TEST_ASSERT_EQUAL(0,stage.validateGPIOs(radio_proto::GP_Relay,17,255,35,cfg,rf,empty,empty,0,0));TEST_ASSERT_EQUAL(PIN_CONFLICT_RADIO,stage.validateGPIOs(radio_proto::GP_Relay,13,255,35,cfg,rf,empty,empty,0,0));
 SelectedSettingsRestore modelOnly;restore_options_t none;none.shades=true;Bytes absent;rf.loadStatus.state=SettingsLoadState::failed;TEST_ASSERT_TRUE(modelOnly.prepare(cfg,rf,none,header(),absent,true,0,0,0));
 TEST_ASSERT_EQUAL(0,modelOnly.validateGPIOs(radio_proto::GP_Remote,255,255,255,cfg,rf,empty,empty,0,0));TEST_ASSERT_EQUAL(0,modelOnly.validateGPIOs(radio_proto::RTS,17,40,42,cfg,rf,empty,empty,0,0));TEST_ASSERT_EQUAL(PIN_STORAGE_UNAVAILABLE,modelOnly.validateGPIOs(radio_proto::GP_Relay,17,255,42,cfg,rf,empty,empty,0,0));
#if HAS_ETHERNET
 rf.loadStatus.state=SettingsLoadState::loaded;cfg.loadStatus.state=SettingsLoadState::failed;TEST_ASSERT_EQUAL(PIN_STORAGE_UNAVAILABLE,modelOnly.validateGPIOs(radio_proto::GP_Relay,17,255,42,cfg,rf,empty,empty,0,0));
#endif
}
static void retained_gpio_blocks_new_bus_adoption_before_any_namespace_write(){
 auto cfg=current();transceiver_config_t rf;qualifySettings(rf);restore_options_t opts;opts.transceiver=true;auto bytes=records();SelectedSettingsRestore stage;
 TEST_ASSERT_FALSE(stage.prepare(cfg,rf,opts,header(),bytes,true,0,0,LivePinClaims::pinMask(13)));
 TEST_ASSERT_EQUAL(RestoreSettingsState::refused,stage.status().state);TEST_ASSERT_EQUAL(0,FakeNvs::writeOpens);
#if HAS_ETHERNET
 rf.enabled=false;opts={};opts.network=true;bytes=records();const auto netStart=bytes.text.find('\n')+1;bytes.text.replace(netStart,3,"  2");SelectedSettingsRestore ethernet;
 TEST_ASSERT_FALSE(ethernet.prepare(cfg,rf,opts,header(),bytes,true,0,0,LivePinClaims::pinMask(21)));
 TEST_ASSERT_EQUAL(RestoreSettingsState::refused,ethernet.status().state);TEST_ASSERT_EQUAL(0,FakeNvs::writeOpens);
#endif
}
static void legacy_absent_fields_are_preserved_only_when_qualified(){
 auto cfg=current();cfg.checkForUpdate=false;transceiver_config_t rf;qualifySettings(rf);auto h=header();h.version=19;restore_options_t opts;opts.settings=true;auto bytes=records();SelectedSettingsRestore stage;TEST_ASSERT_TRUE(stage.prepare(cfg,rf,opts,h,bytes,true,0,0,0));Transceiver live;live.config=rf;Effects effects;TEST_ASSERT_TRUE(stage.apply(cfg,live,{7,GitUpdateExternalOwner::restore},{9},effects).saved());TEST_ASSERT_FALSE(cfg.checkForUpdate);
 opts={};opts.transceiver=true;bytes=records();const auto end=bytes.text.rfind(',');bytes.text=bytes.text.substr(0,end)+"\n";SelectedSettingsRestore legacyRadio;TEST_ASSERT_TRUE(legacyRadio.prepare(cfg,rf,opts,header(),bytes,true,0,0,0));
 opts={};opts.mqtt=true;bytes=records();h.version=21;SelectedSettingsRestore oldMqtt;TEST_ASSERT_FALSE(oldMqtt.prepare(cfg,rf,opts,h,bytes,true,0,0,0));
}
static void ethernet_adoption_requires_source_identity_and_complete_record_fields(){
#if HAS_ETHERNET
 for(unsigned source=0;source<3;++source){auto cfg=current();cfg.Ethernet.PWRPin=14;transceiver_config_t rf;qualifySettings(rf);auto h=header();restore_options_t opts;opts.network=true;auto bytes=records();SelectedSettingsRestore stage;
  if(source!=0)strcpy(h.serverId,"FEDCBA");if(source==2)cfg.Ethernet.loadStatus.state=SettingsLoadState::failed;
  const bool prepared=stage.prepare(cfg,rf,opts,h,bytes,true,0,0,0);TEST_ASSERT_EQUAL(source!=2,prepared);if(!prepared){TEST_ASSERT_EQUAL(0,FakeNvs::sets);continue;}
  Transceiver live;live.config=rf;Effects effects;TEST_ASSERT_TRUE(stage.apply(cfg,live,{7,GitUpdateExternalOwner::restore},{9},effects).saved());TEST_ASSERT_EQUAL(source==0?1:14,cfg.Ethernet.PWRPin);FakeNvs::reset();
 }
#endif
}
static void source_expiry_between_namespaces_or_before_ram_never_runs_later_effects(){
 for(bool lastNamespace:{false,true}){FakeNvs::reset();settingsDurability()=SettingsDurability{};claimHeld=true;auto cfg=current();transceiver_config_t rf;qualifySettings(rf);restore_options_t opts;opts.settings=true;auto bytes=records();SelectedSettingsRestore stage;TEST_ASSERT_TRUE(stage.prepare(cfg,rf,opts,header(),bytes,true,0,0,0));
  if(lastNamespace)FakeNvs::beforeGet=[] {if(!FakeNvs::handles.empty() && FakeNvs::handles.begin()->second=="NTP")claimHeld=false;};
  else FakeNvs::beforeGet=[] {claimHeld=false;};
  Transceiver live;live.config=rf;Effects effects;const auto out=stage.apply(cfg,live,{7,GitUpdateExternalOwner::restore},{9},effects);
  TEST_ASSERT_FALSE(out.saved());TEST_ASSERT_EQUAL(RestoreSettingsState::partial,out.state);TEST_ASSERT_EQUAL(0,out.appliedNamespaces);TEST_ASSERT_EQUAL(lastNamespace?3:1,out.verifiedNamespaces);TEST_ASSERT_EQUAL_STRING("ESPSomfyRTS",cfg.hostname);TEST_ASSERT_EQUAL(0,effects.completedCalls);TEST_ASSERT_EQUAL(0,effects.time);TEST_ASSERT_FALSE(settingsDurability().status().restartAllowed());
  if(!lastNamespace)TEST_ASSERT_FALSE(FakeNvs::values.count("NTP/ntpServer"));FakeNvs::beforeGet=nullptr;claimHeld=true;
 }
}
int main(){UNITY_BEGIN();RUN_TEST(retained_gpio_blocks_new_bus_adoption_before_any_namespace_write);RUN_TEST(source_expiry_between_namespaces_or_before_ram_never_runs_later_effects);RUN_TEST(fresh_owner_and_pause_are_required_before_first_write);RUN_TEST(failed_and_successful_staging_wipe_private_memory_and_allocation_failure_has_no_effects);RUN_TEST(projected_gpio_uses_candidate_pins_and_none_never_requires_an_owner);RUN_TEST(legacy_absent_fields_are_preserved_only_when_qualified);RUN_TEST(ethernet_adoption_requires_source_identity_and_complete_record_fields);RUN_TEST(fully_covered_restore_preserves_omitted_credentials_and_notifies_only_after_save);RUN_TEST(unknown_omitted_fields_or_incomplete_records_never_write_or_adopt);RUN_TEST(all_selected_values_are_preflighted_before_any_persistence);RUN_TEST(partial_namespace_or_radio_marker_failure_never_adopts_other_drafts);RUN_TEST(full_coverage_can_repair_cfg_ntp_ip_but_not_uncovered_wifi_or_security);return UNITY_END();}
