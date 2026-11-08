#include "EntityFields.h"
#include "LocalEntityForm.h"
#include "ProtoEdits.h"
#include <string.h>
#include <unity.h>
void setUp() {}
void tearDown() {}
static void
authoritative_edit_lists_all_have_stable_bounded_field_descriptors() {
  for (auto type :
       {model_edit_type_t::add_room, model_edit_type_t::save_room,
        model_edit_type_t::add_shade, model_edit_type_t::save_shade,
        model_edit_type_t::add_group, model_edit_type_t::save_group}) {
    const auto *form = editForm(type);
    TEST_ASSERT_NOT_NULL(form);
    for (uint8_t i = 0; i < form->count; i++) {
      const auto f = form->fields[i];
      const auto *field = entityField(type, f);
      TEST_ASSERT_NOT_NULL(field);
      TEST_ASSERT_EQUAL(f, field->field);
      TEST_ASSERT_TRUE(field == entityField(type, f));
      TEST_ASSERT_NOT_NULL(entityFieldLabel(f));
      TEST_ASSERT_TRUE(field->lo <= field->hi);
      JsonDocument value;
      if (field->schema().kind == 's') {
        value.set("12345678901234567890");
        TEST_ASSERT_TRUE(
            validEntityValue(*field, value.as<JsonVariantConst>()));
        value.set("123456789012345678901");
        TEST_ASSERT_FALSE(
            validEntityValue(*field, value.as<JsonVariantConst>()));
      } else if (field->schema().kind == 'b') {
        value.set(0);
        TEST_ASSERT_TRUE(
            validEntityValue(*field, value.as<JsonVariantConst>()));
        value.set(1);
        TEST_ASSERT_TRUE(
            validEntityValue(*field, value.as<JsonVariantConst>()));
        value.set(2);
        TEST_ASSERT_FALSE(
            validEntityValue(*field, value.as<JsonVariantConst>()));
      } else {
        for (unsigned choice = 0; choice < field->choiceCount; choice++) {
          TEST_ASSERT_NOT_NULL(field->choices[choice].label);
          value.set(field->choices[choice].value);
          TEST_ASSERT_TRUE(
              validEntityValue(*field, value.as<JsonVariantConst>()));
        }
        value.set(field->lo - 1);
        TEST_ASSERT_FALSE(
            validEntityValue(*field, value.as<JsonVariantConst>()));
        value.set(field->hi + 1);
        TEST_ASSERT_FALSE(
            validEntityValue(*field, value.as<JsonVariantConst>()));
      }
    }
  }
  TEST_ASSERT_NULL(entityField(model_edit_type_t::link_remote,
                               proto_field_t::remoteAddress));
}
static void shade_and_group_protocol_choices_match_supported_configuration() {
  const auto *shade =
      entityField(model_edit_type_t::save_shade, proto_field_t::proto);
  const auto *group =
      entityField(model_edit_type_t::save_group, proto_field_t::proto);
  TEST_ASSERT_EQUAL(5, shade->choiceCount);
  TEST_ASSERT_EQUAL(3, group->choiceCount);
  JsonDocument v;
  v.set(8);
  TEST_ASSERT_TRUE(validEntityValue(*shade, v.as<JsonVariantConst>()));
  TEST_ASSERT_FALSE(validEntityValue(*group, v.as<JsonVariantConst>()));
  v.set(99);
  TEST_ASSERT_FALSE(validEntityValue(*shade, v.as<JsonVariantConst>()));
}
static void typed_edit_roundtrips_destination_boundaries_without_wrap() {
  JsonDocument input, expanded;
  proto_error_buffer_t why = {};
  input[protoField(proto_field_t::shadeId).key] = 4;
  input[protoField(proto_field_t::upTime).key] = UINT32_MAX;
  input[protoField(proto_field_t::stepSize).key] = UINT16_MAX;
  TEST_ASSERT_NULL(expandEditPayload(model_edit_type_t::save_shade,
                                     input.as<JsonObjectConst>(), expanded,
                                     why));
  TEST_ASSERT_EQUAL_UINT32(UINT32_MAX, expanded["upTime"].as<uint32_t>());
  TEST_ASSERT_EQUAL_UINT16(UINT16_MAX, expanded["stepSize"].as<uint16_t>());
  input[protoField(proto_field_t::upTime).key] = int64_t(UINT32_MAX) + 1;
  TEST_ASSERT_NOT_NULL(expandEditPayload(model_edit_type_t::save_shade,
                                         input.as<JsonObjectConst>(), expanded,
                                         why));
  input[protoField(proto_field_t::upTime).key] = -1;
  TEST_ASSERT_NOT_NULL(expandEditPayload(model_edit_type_t::save_shade,
                                         input.as<JsonObjectConst>(), expanded,
                                         why));
}
static void default_backend_choices_use_operation_specific_values_and_labels() {
  using namespace LocalUi::EntityAdmin;
  struct Defaults : Backend {
    Authorization authorization() override { return {}; }
    uint64_t revision() override { return 1; }
    bool prepare(Kind, uint8_t, uint8_t &) override { return false; }
    bool read(Kind, uint8_t, proto_field_t, Value &) override { return false; }
    ApplyResult apply(model_edit_type_t, JsonDocument &, Authorization,
                      uint64_t) override {
      return {};
    }
    CommitResult retryFileCommit(Authorization, uint64_t) override {
      return {};
    }
    void activity() override {}
  } backend;
  Choice choice;
  TEST_ASSERT_EQUAL(5,
                    backend.choiceCount(Kind::shade, 0, proto_field_t::proto));
  TEST_ASSERT_EQUAL(3,
                    backend.choiceCount(Kind::group, 7, proto_field_t::proto));
  TEST_ASSERT_TRUE(
      backend.choiceAt(Kind::group, 7, proto_field_t::proto, 2, choice));
  TEST_ASSERT_EQUAL(2, choice.value);
  TEST_ASSERT_EQUAL_STRING("RTV", choice.label);
  TEST_ASSERT_FALSE(
      backend.choiceAt(Kind::group, 7, proto_field_t::proto, 3, choice));
  TEST_ASSERT_EQUAL_STRING("", choice.label);
  TEST_ASSERT_EQUAL(0,
                    backend.choiceCount(Kind::room, 4, proto_field_t::proto));
}
static void gpio_applicability_uses_the_reviewed_physical_protocol_mask() {
  const auto *up =
      entityField(model_edit_type_t::save_shade, proto_field_t::gpioUp);
  const auto *down =
      entityField(model_edit_type_t::save_shade, proto_field_t::gpioDown);
  const auto *my =
      entityField(model_edit_type_t::save_shade, proto_field_t::gpioMy);
  TEST_ASSERT_FALSE(entityFieldActive(*up, radio_proto::RTS));
  TEST_ASSERT_TRUE(entityFieldActive(*up, radio_proto::GP_Relay));
  TEST_ASSERT_TRUE(entityFieldActive(*down, radio_proto::GP_Relay));
  TEST_ASSERT_FALSE(entityFieldActive(*my, radio_proto::GP_Relay));
  TEST_ASSERT_TRUE(entityFieldActive(*up, radio_proto::GP_Remote));
  TEST_ASSERT_TRUE(entityFieldActive(*my, radio_proto::GP_Remote));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(gpio_applicability_uses_the_reviewed_physical_protocol_mask);
  RUN_TEST(default_backend_choices_use_operation_specific_values_and_labels);
  RUN_TEST(authoritative_edit_lists_all_have_stable_bounded_field_descriptors);
  RUN_TEST(shade_and_group_protocol_choices_match_supported_configuration);
  RUN_TEST(typed_edit_roundtrips_destination_boundaries_without_wrap);
  return UNITY_END();
}
