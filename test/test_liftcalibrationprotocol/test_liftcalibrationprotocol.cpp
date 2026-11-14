#include <unity.h>
#include <ArduinoJson.h>
#include <cstdint>
#include <string>
#include "LiftCalibrationProtocol.h"

void setUp() {}
void tearDown() {}

static const char *key(proto_field_t field) { return protoField(field).key; }
static LiftCalibrationRequest sentinel() {
  LiftCalibrationRequest request;
  request.token = 0xdeadbeef;
  request.id = 29;
  request.action = 3;
  request.kind = LiftKind::segmented;
  request.human = true;
  return request;
}
static void unchanged(const LiftCalibrationRequest &before, const LiftCalibrationRequest &after) {
  TEST_ASSERT_EQUAL_UINT32(before.token, after.token);
  TEST_ASSERT_EQUAL_UINT8(before.id, after.id);
  TEST_ASSERT_EQUAL_UINT8(before.action, after.action);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(before.kind), static_cast<uint8_t>(after.kind));
  TEST_ASSERT_EQUAL(before.human, after.human);
}
static void reject(const JsonDocument &document) {
  auto output = sentinel();
  const auto before = output;
  TEST_ASSERT_NOT_NULL(parseLiftCalibrationPayload(document.as<JsonObjectConst>(), output));
  unchanged(before, output);
}
static LiftCalibrationRequest accept(const JsonDocument &document) {
  auto output = sentinel();
  const char *why = parseLiftCalibrationPayload(document.as<JsonObjectConst>(), output);
  TEST_ASSERT_NULL_MESSAGE(why, why);
  return output;
}
static JsonDocument operation(uint8_t action = LiftCalibrationRequest::poll, uint32_t token = 1) {
  JsonDocument document;
  document[key(proto_field_t::calibrationAction)] = action;
  document[key(proto_field_t::calibrationSession)] = token;
  return document;
}
static JsonDocument begin(uint8_t id = 7, uint8_t kind = 1) {
  JsonDocument document;
  document[key(proto_field_t::calibrationAction)] = LiftCalibrationRequest::begin;
  document[key(proto_field_t::shadeId)] = id;
  document[key(proto_field_t::calibrationKind)] = kind;
  return document;
}
static void setJson(JsonDocument &document, proto_field_t field, const char *json) {
  JsonDocument value;
  TEST_ASSERT_EQUAL(DeserializationError::Ok, deserializeJson(value, json).code());
  document[key(field)] = value.as<JsonVariantConst>();
}

static void test_begin_accepts_enabled_profile_and_shade_edges_without_session() {
  for(const uint8_t id : {1, 32}) for(const uint8_t kind : {1, 2}) for(bool tokenPresent : {false, true}) {
    auto document = begin(id, kind);
    if(tokenPresent) document[key(proto_field_t::calibrationSession)] = 0;
    const auto request = accept(document);
    TEST_ASSERT_EQUAL_UINT8(LiftCalibrationRequest::begin, request.action);
    TEST_ASSERT_EQUAL_UINT8(id, request.id);
    TEST_ASSERT_EQUAL_UINT8(kind, static_cast<uint8_t>(request.kind));
    TEST_ASSERT_EQUAL_UINT32(0, request.token);
    TEST_ASSERT_FALSE(request.human);
  }
}
static void test_poll_and_each_guide_action_preserve_full_uint32_token() {
  for(const uint32_t token : {1u, UINT32_MAX}) {
    for(unsigned action = 0; action <= static_cast<unsigned>(LiftGuideAction::cancel); ++action) {
      const auto request = accept(operation(action, token));
      TEST_ASSERT_EQUAL_UINT8(action, request.action);
      TEST_ASSERT_EQUAL_UINT32(token, request.token);
      TEST_ASSERT_EQUAL_UINT8(0, request.id);
      TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(LiftKind::legacy), static_cast<uint8_t>(request.kind));
      TEST_ASSERT_FALSE(request.human);
    }
    const auto request = accept(operation(LiftCalibrationRequest::poll, token));
    TEST_ASSERT_EQUAL_UINT8(LiftCalibrationRequest::poll, request.action);
    TEST_ASSERT_EQUAL_UINT32(token, request.token);
  }
}
static void test_human_marker_defaults_false_and_accepts_only_bool_or_zero_one() {
  for(const char *json : {"false", "true", "0", "1"}) {
    auto document = operation(static_cast<uint8_t>(LiftGuideAction::halfway));
    setJson(document, proto_field_t::calibrationHuman, json);
    const auto request = accept(document);
    TEST_ASSERT_EQUAL(json[0] == 't' || json[0] == '1', request.human);
  }
  for(const char *json : {"null", "-1", "2", "0.5", "\"1\"", "[]", "{}"}) {
    auto document = operation(static_cast<uint8_t>(LiftGuideAction::confirmEndpoint));
    setJson(document, proto_field_t::calibrationHuman, json);
    reject(document);
  }
}
static void test_action_is_required_exact_integer_and_known_operation() {
  auto missing = operation();
  missing.remove(key(proto_field_t::calibrationAction));
  reject(missing);
  for(const char *json : {"null", "false", "true", "-1", "1.5", "16", "99", "102", "255", "256", "4294967296", "\"0\"", "[]", "{}"}) {
    auto document = operation();
    setJson(document, proto_field_t::calibrationAction, json);
    reject(document);
  }
}
static void test_nonbegin_session_rejects_missing_zero_and_non_uint32_values() {
  auto missing = operation();
  missing.remove(key(proto_field_t::calibrationSession));
  reject(missing);
  for(const char *json : {"null", "false", "true", "0", "-1", "1.5", "4294967296", "18446744073709551615", "\"1\"", "[]", "{}"}) {
    auto document = operation();
    setJson(document, proto_field_t::calibrationSession, json);
    reject(document);
  }
}
static void test_begin_rejects_missing_or_invalid_target_kind_and_nonzero_session() {
  for(const auto field : {proto_field_t::shadeId, proto_field_t::calibrationKind}) {
    auto document = begin();
    document.remove(key(field));
    reject(document);
  }
  for(const char *json : {"null", "false", "true", "0", "33", "255", "-1", "1.5", "\"1\"", "[]", "{}"}) {
    auto document = begin();
    setJson(document, proto_field_t::shadeId, json);
    reject(document);
  }
  for(const char *json : {"null", "false", "true", "0", "3", "255", "-1", "1.5", "\"1\"", "[]", "{}"}) {
    auto document = begin();
    setJson(document, proto_field_t::calibrationKind, json);
    reject(document);
  }
  for(const char *json : {"null", "false", "true", "1", "-1", "0.5", "4294967296", "\"0\""}) {
    auto document = begin();
    setJson(document, proto_field_t::calibrationSession, json);
    reject(document);
  }
}
static void test_nonbegin_forbids_target_kind_or_any_unrelated_key_even_at_default() {
  for(const auto action : {LiftCalibrationRequest::poll, static_cast<uint8_t>(LiftGuideAction::save)}) {
    for(const auto field : {proto_field_t::shadeId, proto_field_t::calibrationKind, proto_field_t::target}) {
      auto document = operation(action);
      document[key(field)] = 0;
      reject(document);
    }
    auto document = operation(action);
    document["unknown"] = false;
    reject(document);
  }
  auto document = begin();
  document[key(proto_field_t::command)] = "Down";
  reject(document);
}
static void test_nonobjects_and_oversized_invalid_values_reject_without_partial_output() {
  for(const char *json : {"null", "[]", "[1,2]", "\"text\"", "1", "true"}) {
    JsonDocument document;
    TEST_ASSERT_EQUAL(DeserializationError::Ok, deserializeJson(document, json).code());
    reject(document);
  }
  JsonDocument empty;
  reject(empty);
  auto document = operation();
  document["unknown"] = std::string(4096, 'x');
  reject(document);
  document = operation();
  document[key(proto_field_t::calibrationSession)] = std::string(4096, '9');
  reject(document);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_begin_accepts_enabled_profile_and_shade_edges_without_session);
  RUN_TEST(test_poll_and_each_guide_action_preserve_full_uint32_token);
  RUN_TEST(test_human_marker_defaults_false_and_accepts_only_bool_or_zero_one);
  RUN_TEST(test_action_is_required_exact_integer_and_known_operation);
  RUN_TEST(test_nonbegin_session_rejects_missing_zero_and_non_uint32_values);
  RUN_TEST(test_begin_rejects_missing_or_invalid_target_kind_and_nonzero_session);
  RUN_TEST(test_nonbegin_forbids_target_kind_or_any_unrelated_key_even_at_default);
  RUN_TEST(test_nonobjects_and_oversized_invalid_values_reject_without_partial_output);
  return UNITY_END();
}
