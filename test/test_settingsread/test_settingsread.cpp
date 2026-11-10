#include "QualifiedSettings.h"
#include <unity.h>
#include <map>
#include <string>
#include "SettingsRead.h"
#include "ConfigSettings.h"
#include "Transceiver.h"
struct Snapshot : SettingsSnapshotSink {
  std::map<proto_field_t,settings_value_t> values;
  void field(proto_field_t field,const settings_value_t &value) override {
    TEST_ASSERT_EQUAL(protoField(field).kind,value.kind);
    TEST_ASSERT_TRUE(values.emplace(field,value).second);
  }
};
void setUp() {}
void tearDown() {}
static void test_snapshot_covers_all_editable_metadata_without_secret_values() {
  ConfigSettings settings;qualifySettings(settings);
  transceiver_config_t radio;qualifySettings(radio);
  strcpy(settings.WIFI.passphrase,"wifi-secret-sentinel");
  strcpy(settings.MQTT.password,"mqtt-secret-sentinel");
  strcpy(settings.Security.password,"login-secret-sentinel");
  strcpy(settings.Security.pin,"4PIN");
  for(uint8_t sectionIndex=0;sectionIndex<5;sectionIndex++) {
    const auto section=static_cast<settings_section_t>(sectionIndex);
    Snapshot snapshot;
    TEST_ASSERT_TRUE(readSettingsSnapshot(section,settings,radio,snapshot));
    for(uint8_t i=0;i<settingsFieldCount(section,HAS_ETHERNET);i++) {
      const auto &metadata=*settingsFieldAt(section,i,HAS_ETHERNET);
      if(metadata.secret()) {
        TEST_ASSERT_FALSE(snapshot.values.count(metadata.field));
        TEST_ASSERT_TRUE(snapshot.values.at(metadata.presenceField()).boolean);
      }
      else TEST_ASSERT_TRUE(snapshot.values.count(metadata.field));
    }
    for(const auto &item:snapshot.values) {
      TEST_ASSERT_NULL(strstr(item.second.text,"secret-sentinel"));
      TEST_ASSERT_NULL(strstr(item.second.text,"4PIN"));
      TEST_ASSERT_NOT_NULL(memchr(item.second.text,0,sizeof(item.second.text)));
    }
  }
  TEST_ASSERT_EQUAL_STRING("wifi-secret-sentinel",settings.WIFI.passphrase);
  TEST_ASSERT_EQUAL_STRING("mqtt-secret-sentinel",settings.MQTT.password);
  TEST_ASSERT_EQUAL_STRING("login-secret-sentinel",settings.Security.password);
}
static void test_defaults_addresses_and_presence_are_explicit() {
  ConfigSettings settings;qualifySettings(settings);
  transceiver_config_t radio;qualifySettings(radio);
  Snapshot snapshot;
  TEST_ASSERT_TRUE(readSettingsSnapshot(settings_section_t::network,settings,radio,snapshot));
  TEST_ASSERT_FALSE(snapshot.values.at(proto_field_t::hasPassphrase).boolean);
  TEST_ASSERT_EQUAL(bool(HAS_ETHERNET),snapshot.values.at(proto_field_t::hasEthernet).boolean);
  TEST_ASSERT_EQUAL_STRING("",snapshot.values.at(proto_field_t::ip).text);
  TEST_ASSERT_EQUAL_STRING("",snapshot.values.at(proto_field_t::gateway).text);
  TEST_ASSERT_EQUAL_STRING("255.255.255.0",snapshot.values.at(proto_field_t::subnet).text);
  settings.IP.ip=IPAddress(255,255,255,255);
  snapshot.values.clear();
  TEST_ASSERT_TRUE(readSettingsSnapshot(settings_section_t::network,settings,radio,snapshot));
  TEST_ASSERT_EQUAL_STRING("255.255.255.255",snapshot.values.at(proto_field_t::ip).text);
  snapshot.values.clear();
  TEST_ASSERT_TRUE(readSettingsSnapshot(settings_section_t::security,settings,radio,snapshot));
  TEST_ASSERT_FALSE(snapshot.values.at(proto_field_t::hasPassword).boolean);
  TEST_ASSERT_FALSE(snapshot.values.at(proto_field_t::hasPin).boolean);
  TEST_ASSERT_EQUAL(0,snapshot.values.at(proto_field_t::securityType).integer);
  snapshot.values.clear();
  TEST_ASSERT_FALSE(readSettingsSnapshot(settings_section_t(255),settings,radio,snapshot));
  TEST_ASSERT_TRUE(snapshot.values.empty());
}
static void test_bounded_utf8_text_is_copied_and_owned_by_sink() {
  ConfigSettings settings;qualifySettings(settings);
  transceiver_config_t radio;qualifySettings(radio);
  for(size_t i=0;i<64;i+=2) { settings.MQTT.hostname[i]='\xc3'; settings.MQTT.hostname[i+1]='\xa9'; }
  settings.MQTT.hostname[64]=0;
  Snapshot snapshot;
  TEST_ASSERT_TRUE(readSettingsSnapshot(settings_section_t::mqtt,settings,radio,snapshot));
  TEST_ASSERT_EQUAL(64,strlen(snapshot.values.at(proto_field_t::hostname).text));
  TEST_ASSERT_EQUAL_STRING(settings.MQTT.hostname,snapshot.values.at(proto_field_t::hostname).text);
  strcpy(settings.MQTT.hostname,"changed later");
  TEST_ASSERT_EQUAL(64,strlen(snapshot.values.at(proto_field_t::hostname).text));
}
static void test_read_values_roundtrip_metadata_to_validator_without_secret_prefill() {
  ConfigSettings settings;qualifySettings(settings);
  settings.connType=conn_types_t::wifi;
  transceiver_config_t radio;qualifySettings(radio);
  for(uint8_t sectionIndex=0;sectionIndex<5;sectionIndex++) {
    const auto section=static_cast<settings_section_t>(sectionIndex);
    Snapshot snapshot;
    TEST_ASSERT_TRUE(readSettingsSnapshot(section,settings,radio,snapshot));
    JsonDocument payload,expanded;
    payload[protoField(proto_field_t::section).key]=settingsSectionName(section);
    for(uint8_t i=0;i<settingsFieldCount(section,HAS_ETHERNET);i++) {
      const auto &metadata=*settingsFieldAt(section,i,HAS_ETHERNET);
      if(metadata.secret()) continue;
      const auto &value=snapshot.values.at(metadata.field);
      const char *key=metadata.schema().key;
      switch(value.kind) {
        case 'b': payload[key]=value.boolean ? 1 : 0; break;
        case 'i': payload[key]=value.integer; break;
        case 'n': payload[key]=value.number; break;
        case 's': payload[key]=value.text; break;
      }
    }
    proto_error_buffer_t error;
    settings_section_t parsed;
    const char *bad=expandSettingsPayload(payload.as<JsonObjectConst>(),HAS_ETHERNET,parsed,expanded,error);
    TEST_ASSERT_NULL_MESSAGE(bad,bad);
    TEST_ASSERT_EQUAL(section,parsed);
    TEST_ASSERT_TRUE(expanded["password"].isNull());
    TEST_ASSERT_TRUE(expanded["pin"].isNull());
    TEST_ASSERT_TRUE(expanded["passphrase"].isNull());
  }
}
int main() {
  UNITY_BEGIN();
  RUN_TEST(test_snapshot_covers_all_editable_metadata_without_secret_values);
  RUN_TEST(test_defaults_addresses_and_presence_are_explicit);
  RUN_TEST(test_bounded_utf8_text_is_copied_and_owned_by_sink);
  RUN_TEST(test_read_values_roundtrip_metadata_to_validator_without_secret_prefill);
  return UNITY_END();
}
