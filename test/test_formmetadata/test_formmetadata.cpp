#include <unity.h>
#include <string>
#include "ProtoSettings.h"
#include "ProtoEdits.h"
void setUp() {}
void tearDown() {}
static void test_all_settings_sections_enumerate_authoritative_editable_fields() {
  const uint8_t counts[]={4,18,9,4,14};
  for(uint8_t i=0;i<5;i++) {
    const auto section=static_cast<settings_section_t>(i);
    TEST_ASSERT_EQUAL(counts[i],settingsFieldCount(section,true));
    for(uint8_t n=0;n<counts[i];n++) {
      const auto *field=settingsFieldAt(section,n,true);
      TEST_ASSERT_NOT_NULL(field);
      TEST_ASSERT_EQUAL_PTR(field,settingsFieldAt(section,n,true));
      TEST_ASSERT_NOT_NULL(field->schema().key);
      TEST_ASSERT_NOT_NULL(field->routeName());
      for(uint8_t before=0;before<n;before++) TEST_ASSERT_NOT_EQUAL(field->field,settingsFieldAt(section,before,true)->field);
    }
    TEST_ASSERT_NULL(settingsFieldAt(section,counts[i],true));
  }
  TEST_ASSERT_EQUAL(11,settingsFieldCount(settings_section_t::network,false));
  for(uint8_t n=0;n<settingsFieldCount(settings_section_t::network,false);n++)
    TEST_ASSERT_FALSE(settingsFieldAt(settings_section_t::network,n,false)->ethernetOnly());
  TEST_ASSERT_EQUAL(0,settingsFieldCount(settings_section_t(255),true));
  TEST_ASSERT_NULL(settingsFieldAt(settings_section_t(255),0,true));
}
static void test_add_save_entity_forms_share_fields_but_require_identity_only_on_save() {
  const model_edit_type_t adds[]={model_edit_type_t::add_room,model_edit_type_t::add_shade,model_edit_type_t::add_group};
  const model_edit_type_t saves[]={model_edit_type_t::save_room,model_edit_type_t::save_shade,model_edit_type_t::save_group};
  const proto_field_t ids[]={proto_field_t::roomId,proto_field_t::shadeId,proto_field_t::groupId};
  const uint8_t counts[]={2,21,7};
  for(uint8_t i=0;i<3;i++) {
    const auto *add=editForm(adds[i]), *save=editForm(saves[i]);
    TEST_ASSERT_NOT_NULL(add); TEST_ASSERT_NOT_NULL(save);
    TEST_ASSERT_EQUAL(counts[i],add->count);
    TEST_ASSERT_EQUAL_PTR(add->fields,save->fields);
    TEST_ASSERT_EQUAL(proto_field_t::count,add->identity);
    TEST_ASSERT_EQUAL(ids[i],save->identity);
  }
  TEST_ASSERT_NULL(editForm(model_edit_type_t::link_remote));
  TEST_ASSERT_NULL(editForm(model_edit_type_t(255)));
}
static const char *checkSetting(settings_section_t section, const settings_field_t &field, JsonVariantConst value, bool ethernet, JsonDocument &expanded, proto_error_buffer_t &error) {
  JsonDocument payload;
  payload[protoField(proto_field_t::section).key]=settingsSectionName(section);
  payload[field.schema().key]=value;
  settings_section_t parsed;
  return expandSettingsPayload(payload.as<JsonObjectConst>(),ethernet,parsed,expanded,error);
}
static void test_metadata_bounds_and_choices_roundtrip_through_real_settings_validator() {
  for(uint8_t sectionIndex=0;sectionIndex<5;sectionIndex++) {
    auto section=static_cast<settings_section_t>(sectionIndex);
    for(uint8_t i=0;i<settingsFieldCount(section,true);i++) {
      const auto &field=*settingsFieldAt(section,i,true);
      JsonDocument value, expanded;
      proto_error_buffer_t error;
      const char kind=field.schema().kind;
      if(kind=='i' || kind=='n') {
        for(int which=0;which<4;which++) {
          int64_t number=which==0 ? field.lo : which==1 ? field.hi : which==2 ? int64_t(field.lo)-1 : int64_t(field.hi)+1;
          if(kind=='n') value.set(double(number)/100.0); else value.set(number);
          const char *bad=checkSetting(section,field,value.as<JsonVariantConst>(),true,expanded,error);
          if(which<2) TEST_ASSERT_NULL_MESSAGE(bad,bad); else TEST_ASSERT_NOT_NULL(bad);
        }
        for(uint8_t c=0;c<field.choiceCount;c++) {
          value.set(field.choices[c]);
          TEST_ASSERT_NULL(checkSetting(section,field,value.as<JsonVariantConst>(),true,expanded,error));
        }
        if(field.choiceCount) {
          int32_t gap=field.lo;
          while(gap<=field.hi) {
            bool listed=false;
            for(uint8_t c=0;c<field.choiceCount;c++) listed |= gap==field.choices[c];
            if(!listed) break;
            ++gap;
          }
          TEST_ASSERT_LESS_OR_EQUAL(field.hi,gap);
          value.set(gap);
          TEST_ASSERT_NOT_NULL(checkSetting(section,field,value.as<JsonVariantConst>(),true,expanded,error));
        }
      }
      else if(kind=='s') {
        std::string text(field.hi,'x');
        value.set(field.ipAddress() ? "255.255.255.255" : text.c_str());
        TEST_ASSERT_NULL(checkSetting(section,field,value.as<JsonVariantConst>(),true,expanded,error));
        text.push_back('x'); value.set(text.c_str());
        TEST_ASSERT_NOT_NULL(checkSetting(section,field,value.as<JsonVariantConst>(),true,expanded,error));
        value.set("");
        TEST_ASSERT_NULL(checkSetting(section,field,value.as<JsonVariantConst>(),true,expanded,error));
      }
      else { value.set(1); TEST_ASSERT_NULL(checkSetting(section,field,value.as<JsonVariantConst>(),true,expanded,error)); }
      TEST_ASSERT_TRUE(!expanded[field.routeName()].isNull());
      // All kinds refuse an object, regardless of their numeric/text bounds.
      value.to<JsonObject>()["invalid"]=true;
      TEST_ASSERT_NOT_NULL(checkSetting(section,field,value.as<JsonVariantConst>(),true,expanded,error));
      if(field.ethernetOnly()) {
        value.set(field.lo);
        TEST_ASSERT_EQUAL_STRING("This device has no Ethernet",checkSetting(section,field,value.as<JsonVariantConst>(),false,expanded,error));
      }
      TEST_ASSERT_EQUAL(field.secret(),field.presenceField()!=proto_field_t::count);
    }
  }
}
static void test_every_entity_form_field_uses_the_real_edit_allowlist_and_kind() {
  const model_edit_type_t operations[]={model_edit_type_t::add_room,model_edit_type_t::add_shade,model_edit_type_t::add_group,
    model_edit_type_t::save_room,model_edit_type_t::save_shade,model_edit_type_t::save_group};
  for(auto operation: operations) {
    const auto &form=*editForm(operation);
    for(uint8_t i=0;i<form.count;i++) {
      const auto &field=protoField(form.fields[i]);
      JsonDocument payload,expanded;
      proto_error_buffer_t error;
      if(field.kind=='s') payload[field.key]="name";
      else if(field.kind=='b') payload[field.key]=1;
      else payload[field.key]=field.def;
      TEST_ASSERT_NULL(expandEditPayload(operation,payload.as<JsonObjectConst>(),expanded,error));
      TEST_ASSERT_TRUE(!expanded[field.name].isNull());
      payload[field.key].to<JsonObject>()["wrong-kind"]=1;
      TEST_ASSERT_NOT_NULL(expandEditPayload(operation,payload.as<JsonObjectConst>(),expanded,error));
    }
    JsonDocument payload,expanded;
    proto_error_buffer_t error;
    payload[protoField(proto_field_t::position).key]=50;
    TEST_ASSERT_NOT_NULL(expandEditPayload(operation,payload.as<JsonObjectConst>(),expanded,error));
    payload.clear();
    const auto id=operation==model_edit_type_t::add_room || operation==model_edit_type_t::save_room ? proto_field_t::roomId :
      operation==model_edit_type_t::add_shade || operation==model_edit_type_t::save_shade ? proto_field_t::shadeId : proto_field_t::groupId;
    payload[protoField(id).key]=1;
    const char *bad=expandEditPayload(operation,payload.as<JsonObjectConst>(),expanded,error);
    if(form.identity==proto_field_t::count) TEST_ASSERT_NOT_NULL(bad); else TEST_ASSERT_NULL(bad);
  }
}
int main() {
  UNITY_BEGIN();
  RUN_TEST(test_all_settings_sections_enumerate_authoritative_editable_fields);
  RUN_TEST(test_add_save_entity_forms_share_fields_but_require_identity_only_on_save);
  RUN_TEST(test_metadata_bounds_and_choices_roundtrip_through_real_settings_validator);
  RUN_TEST(test_every_entity_form_field_uses_the_real_edit_allowlist_and_kind);
  return UNITY_END();
}
