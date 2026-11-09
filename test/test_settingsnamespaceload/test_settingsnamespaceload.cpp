#include <unity.h>
#include <nvs.h>
#include "ConfigSettings.h"
#include "SettingsRead.h"
#include "Transceiver.h"
#include <string.h>
void setUp(){FakeNvs::reset();}void tearDown(){TEST_ASSERT_TRUE(FakeNvs::handles.empty());}
static void text(const char *space,const char *key,const char *value){nvs_handle_t h;nvs_open(space,NVS_READWRITE,&h);nvs_set_str(h,key,value);nvs_close(h);}
static void integer(const char *space,const char *key,int8_t value){nvs_handle_t h;nvs_open(space,NVS_READWRITE,&h);nvs_set_i8(h,key,value);nvs_close(h);}
static void countedReads(){FakeNvs::opens=FakeNvs::reads=FakeNvs::closes=FakeNvs::sets=FakeNvs::commits=FakeNvs::erases=FakeNvs::readOnlyOpens=FakeNvs::writeOpens=0;}
static void first_boot_all_namespace_loads_are_readonly_and_do_not_apply_or_migrate(){
 ConfigSettings cfg;TEST_ASSERT_FALSE(cfg.loadStatus.readable());
 TEST_ASSERT_TRUE(cfg.load());TEST_ASSERT_TRUE(cfg.NTP.load());TEST_ASSERT_TRUE(cfg.IP.load());TEST_ASSERT_TRUE(cfg.WIFI.load());TEST_ASSERT_TRUE(cfg.MQTT.load());
 TEST_ASSERT_EQUAL(SettingsLoadState::knownEmpty,cfg.loadStatus.state);TEST_ASSERT_EQUAL(conn_types_t::wifi,cfg.connType);
 TEST_ASSERT_FALSE(cfg.legacyMigrationPending);TEST_ASSERT_EQUAL(0,FakeNvs::writeOpens);TEST_ASSERT_EQUAL(0,FakeNvs::sets);TEST_ASSERT_EQUAL(0,FakeNvs::erases);
}
static void failed_namespace_reads_never_publish_partial_or_default_fields(){
 text("WIFI","ssid","known");text("WIFI","passphrase","secret");countedReads();
 for(int read=1;read<=6;++read){
  WifiSettings cfg;strcpy(cfg.ssid,"prior");strcpy(cfg.passphrase,"prior secret");FakeNvs::reads=0;FakeNvs::failRead=read;
  TEST_ASSERT_FALSE(cfg.load());TEST_ASSERT_FALSE(cfg.loadStatus.readable());TEST_ASSERT_EQUAL_STRING("prior",cfg.ssid);TEST_ASSERT_EQUAL_STRING("prior secret",cfg.passphrase);
 }
 TEST_ASSERT_EQUAL(0,FakeNvs::writeOpens);TEST_ASSERT_EQUAL(0,FakeNvs::sets);
}
static void readable_invalid_connection_is_not_substituted_or_migrated(){
 integer("CFG","connType",99);text("CFG","hostname","kept");countedReads();ConfigSettings cfg;
 TEST_ASSERT_FALSE(cfg.load());TEST_ASSERT_TRUE(cfg.loadStatus.readable());TEST_ASSERT_FALSE(cfg.loadStatus.usable());TEST_ASSERT_EQUAL(99,uint8_t(cfg.connType));TEST_ASSERT_EQUAL_STRING("kept",cfg.hostname);
 TEST_ASSERT_FALSE(cfg.legacyMigrationPending);TEST_ASSERT_EQUAL(0,FakeNvs::writeOpens);
}
static void legacy_source_is_staged_without_cleanup_and_failed_read_is_not_migration(){
 text("WIFI","hostname","legacy");countedReads();ConfigSettings cfg;
 TEST_ASSERT_TRUE(cfg.load());TEST_ASSERT_TRUE(cfg.legacyMigrationPending);TEST_ASSERT_EQUAL_STRING("legacy",cfg.hostname);TEST_ASSERT_EQUAL(0,FakeNvs::writeOpens);TEST_ASSERT_EQUAL(0,FakeNvs::erases);
 FakeNvs::reads=0;FakeNvs::failRead=1;ConfigSettings failed;
 TEST_ASSERT_FALSE(failed.load());TEST_ASSERT_FALSE(failed.loadStatus.readable());TEST_ASSERT_FALSE(failed.legacyMigrationPending);TEST_ASSERT_EQUAL(0,FakeNvs::writeOpens);
}
static void invalid_ip_originals_remain_visible_and_only_explicit_valid_replacement_clears_them(){
 text("IP","ip","999.2.3.4");text("IP","gateway","not an IP");text("IP","subnet","255.255.255.0");countedReads();IPSettings cfg;
 TEST_ASSERT_FALSE(cfg.load());TEST_ASSERT_TRUE(cfg.loadStatus.readable());TEST_ASSERT_FALSE(cfg.loadStatus.usable());
 TEST_ASSERT_EQUAL_STRING("999.2.3.4",cfg.invalidAddress[0]);TEST_ASSERT_EQUAL_STRING("not an IP",cfg.invalidAddress[1]);
 JsonDocument doc;doc["ip"]="10.2.3.4";auto obj=doc.as<JsonObject>();TEST_ASSERT_TRUE(cfg.fromJSON(obj));
 TEST_ASSERT_EQUAL_STRING("",cfg.invalidAddress[0]);TEST_ASSERT_EQUAL_STRING("not an IP",cfg.invalidAddress[1]);
 TEST_ASSERT_FALSE(cfg.save().saved());TEST_ASSERT_EQUAL(0,FakeNvs::writeOpens);
 doc.clear();doc["gateway"]="";obj=doc.as<JsonObject>();TEST_ASSERT_TRUE(cfg.fromJSON(obj));
 TEST_ASSERT_TRUE(cfg.save().saved());TEST_ASSERT_TRUE(cfg.loadStatus.usable());TEST_ASSERT_EQUAL_STRING("0.0.0.0",cfg.gateway.toString().c_str());
}
static void mqtt_signed_port_preserves_unsigned_runtime_domain(){
 nvs_handle_t h;nvs_open("MQTT",NVS_READWRITE,&h);nvs_set_i16(h,"port",-1);nvs_close(h);countedReads();MQTTSettings cfg;
 TEST_ASSERT_TRUE(cfg.load());TEST_ASSERT_EQUAL_UINT16(65535,cfg.port);TEST_ASSERT_EQUAL(0,FakeNvs::writeOpens);
}
struct Capture : SettingsSnapshotSink {
 unsigned fields=0;char original[65]={};
 void field(proto_field_t key,const settings_value_t &value)override {++fields;if(key==proto_field_t::ip)strlcpy(original,value.text,sizeof(original));}
};
static void unavailable_sections_never_publish_defaults_and_other_sections_remain_usable(){
 ConfigSettings cfg;TEST_ASSERT_TRUE(cfg.load());TEST_ASSERT_TRUE(cfg.NTP.load());TEST_ASSERT_TRUE(cfg.WIFI.load());
 FakeNvs::failRead=FakeNvs::reads+1;TEST_ASSERT_FALSE(cfg.IP.load());
 transceiver_config_t radio;Capture network;
 TEST_ASSERT_FALSE(readSettingsSnapshot(settings_section_t::network,cfg,radio,network));TEST_ASSERT_EQUAL(0,network.fields);
 Capture general;TEST_ASSERT_TRUE(readSettingsSnapshot(settings_section_t::general,cfg,radio,general));TEST_ASSERT_GREATER_THAN(0,general.fields);
 const auto opens=FakeNvs::opens;TEST_ASSERT_FALSE(cfg.IP.save().saved());TEST_ASSERT_EQUAL(opens,FakeNvs::opens);
}
static void readable_invalid_ip_snapshot_is_exact_and_unrelated_namespace_save_does_not_repair_it(){
 text("IP","ip","bad-original");countedReads();ConfigSettings cfg;TEST_ASSERT_TRUE(cfg.load());TEST_ASSERT_TRUE(cfg.WIFI.load());TEST_ASSERT_FALSE(cfg.IP.load());TEST_ASSERT_TRUE(cfg.MQTT.load());
 Capture snapshot;transceiver_config_t radio;TEST_ASSERT_TRUE(readSettingsSnapshot(settings_section_t::network,cfg,radio,snapshot));TEST_ASSERT_EQUAL_STRING("bad-original",snapshot.original);
 TEST_ASSERT_TRUE(cfg.MQTT.save().saved());TEST_ASSERT_FALSE(cfg.IP.loadStatus.usable());TEST_ASSERT_EQUAL_STRING("bad-original",cfg.IP.invalidAddress[0]);
 TEST_ASSERT_FALSE(cfg.IP.save().saved());
}
int main(int,char**){UNITY_BEGIN();RUN_TEST(unavailable_sections_never_publish_defaults_and_other_sections_remain_usable);RUN_TEST(readable_invalid_ip_snapshot_is_exact_and_unrelated_namespace_save_does_not_repair_it);RUN_TEST(first_boot_all_namespace_loads_are_readonly_and_do_not_apply_or_migrate);RUN_TEST(failed_namespace_reads_never_publish_partial_or_default_fields);RUN_TEST(readable_invalid_connection_is_not_substituted_or_migrated);RUN_TEST(legacy_source_is_staged_without_cleanup_and_failed_read_is_not_migration);RUN_TEST(invalid_ip_originals_remain_visible_and_only_explicit_valid_replacement_clears_them);RUN_TEST(mqtt_signed_port_preserves_unsigned_runtime_domain);return UNITY_END();}
