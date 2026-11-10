#include "ModelEditor.h"
#include "ProtoFrame.h"
#include "ProtoModelResults.h"
#include "ProtoParse.h"
#include <ArduinoJson.h>
#include <unity.h>
void setUp() {}
void tearDown() {}
static void applied_failure_retains_id_and_actual_change_fanout() {
  const auto r = edit_result_t::completed(
      22, {ModelCommitState::failed, ModelCommitStage::sync});
  TEST_ASSERT_FALSE(r.ok);
  TEST_ASSERT_TRUE(r.applied);
  TEST_ASSERT_FALSE(r.durable());
  TEST_ASSERT_EQUAL(22, r.id);
  TEST_ASSERT_EQUAL(22, editChanges(model_edit_type_t::add_shade, r).shadeId);
  TEST_ASSERT_EQUAL(
      22, editChanges(model_edit_type_t::delete_shade, r).removedShadeId);
  auto refused = edit_result_t::error("bad input");
  refused.id = 22;
  TEST_ASSERT_EQUAL(0,
                    editChanges(model_edit_type_t::add_shade, refused).shadeId);
  TEST_ASSERT_TRUE(edit_result_t::completed(
                       22, {ModelCommitState::saved, ModelCommitStage::none})
                       .ok);
}
static void
reply_preserves_new_id_and_distinguishes_persistence_failure_from_refusal() {
  char text[256];
  BufferedJsonFormatter json;
  json.setBuffer(text, sizeof(text));
  beginFrame(json, proto_type_t::reply, 7);
  ProtoWriter writer(json);
  const auto r = edit_result_t::completed(
      22, {ModelCommitState::uncertain, ModelCommitStage::verifyRead});
  writeModelEditResult(writer, model_edit_type_t::add_shade, r);
  endFrame(json);
  TEST_ASSERT_FALSE(json.overflowed());
  JsonDocument doc;
  TEST_ASSERT_FALSE(deserializeJson(doc, text));
  auto d = doc["d"].as<JsonObject>();
  TEST_ASSERT_FALSE(protoBool(d, proto_field_t::ok));
  TEST_ASSERT_TRUE(protoBool(d, proto_field_t::applied));
  TEST_ASSERT_FALSE(protoBool(d, proto_field_t::durable));
  TEST_ASSERT_EQUAL(22, protoInt(d, proto_field_t::shadeId));
  TEST_ASSERT_EQUAL(static_cast<int>(proto_error_t::internal),
                    protoInt(d, proto_field_t::code));
  TEST_ASSERT_EQUAL(static_cast<int>(ModelCommitState::uncertain),
                    protoInt(d, proto_field_t::modelSaveState));
}
static void commit_retry_payload_is_empty_and_schema_dispatch_is_device_only() {
  JsonDocument d;
  d.to<JsonObject>();
  TEST_ASSERT_NULL(parseModelCommitPayload(d.as<JsonObjectConst>()));
  d[protoField(proto_field_t::shadeId).key] = 22;
  TEST_ASSERT_NOT_NULL(parseModelCommitPayload(d.as<JsonObjectConst>()));
  TEST_ASSERT_EQUAL(proto_op_t::device, protoOpKind(static_cast<uint8_t>(
                                            proto_type_t::commitModel)));
}
static void
saved_file_report_truthfully_distinguishes_restart_scheduled_or_deferred() {
  for (unsigned deferred = 0; deferred < 2; deferred++) {
    char text[256];
    BufferedJsonFormatter json;
    json.setBuffer(text, sizeof(text));
    beginFrame(json, proto_type_t::reply, 7);
    ProtoWriter writer(json);
    writeModelCommitReport(writer,
                           {{ModelCommitState::saved, ModelCommitStage::none},
                            !deferred,
                            bool(deferred)});
    endFrame(json);
    TEST_ASSERT_FALSE(json.overflowed());
    JsonDocument doc;
    TEST_ASSERT_FALSE(deserializeJson(doc, text));
    auto payload = doc["d"].as<JsonObject>();
    TEST_ASSERT_TRUE(protoBool(payload, proto_field_t::ok));
    TEST_ASSERT_TRUE(protoBool(payload, proto_field_t::durable));
    TEST_ASSERT_EQUAL(!deferred, protoBool(payload, proto_field_t::rebooting));
    if (deferred)
      TEST_ASSERT_NOT_NULL(strstr(protoString(payload, proto_field_t::desc),
                                  "restart not scheduled"));
  }
}
static void model_persistence_event_frames_keep_failure_and_uncertainty_visible() {
  for (const auto state : {ModelCommitState::saved, ModelCommitState::failed,
                           ModelCommitState::uncertain}) {
    char text[256];
    BufferedJsonFormatter json;
    json.setBuffer(text, sizeof(text));
    beginFrame(json, proto_type_t::modelPersistence);
    ProtoWriter writer(json);
    writeModelCommitReport(writer, {{state, ModelCommitStage::verifyRead}});
    endFrame(json);
    TEST_ASSERT_FALSE(json.overflowed());
    JsonDocument doc;
    TEST_ASSERT_FALSE(deserializeJson(doc, text));
    TEST_ASSERT_EQUAL(2, doc["v"].as<int>());
    TEST_ASSERT_EQUAL(static_cast<int>(proto_type_t::modelPersistence),
                      doc["t"].as<int>());
    TEST_ASSERT_FALSE(doc["id"].is<int>());
    const auto payload = doc["d"].as<JsonObject>();
    TEST_ASSERT_EQUAL(state == ModelCommitState::saved,
                      protoBool(payload, proto_field_t::durable));
    TEST_ASSERT_EQUAL(static_cast<int>(state),
                      protoInt(payload, proto_field_t::modelSaveState));
    TEST_ASSERT_FALSE(protoBool(payload, proto_field_t::rebooting));
    if (state != ModelCommitState::saved)
      TEST_ASSERT_NOT_NULL(protoString(payload, proto_field_t::desc));
  }
}
static void expired_upload_recovery_uses_existing_wire_fields_without_a_save_claim() {
  char text[256];BufferedJsonFormatter json;json.setBuffer(text,sizeof(text));
  beginFrame(json,proto_type_t::modelPersistence);ProtoWriter writer(json);
  ModelCommitReport report;report.refusal=RestartReason::stale;
  writeModelCommitReport(writer,report);endFrame(json);
  TEST_ASSERT_FALSE(json.overflowed());JsonDocument doc;
  TEST_ASSERT_FALSE(deserializeJson(doc,text));const auto payload=doc["d"].as<JsonObject>();
  TEST_ASSERT_FALSE(protoBool(payload,proto_field_t::ok));
  TEST_ASSERT_FALSE(protoBool(payload,proto_field_t::durable));
  TEST_ASSERT_FALSE(protoBool(payload,proto_field_t::rebooting));
  TEST_ASSERT_NOT_NULL(strstr(protoString(payload,proto_field_t::desc),"no file save or restart"));
  TEST_ASSERT_EQUAL(3,payload.size());
}
static void sealed_filesystem_reports_unsaved_ram_without_claiming_previous_disk_file() {
  const ModelCommitResult busy{ModelCommitState::failed,ModelCommitStage::filesystemBusy};
  auto edit=edit_result_t::completed(7,busy);TEST_ASSERT_TRUE(edit.applied);TEST_ASSERT_FALSE(edit.ok);
  TEST_ASSERT_NOT_NULL(strstr(edit.desc,"memory"));TEST_ASSERT_NULL(strstr(edit.desc,"Previous file"));
  char text[256];BufferedJsonFormatter json;json.setBuffer(text,sizeof(text));
  beginFrame(json,proto_type_t::modelPersistence);ProtoWriter writer(json);
  writeModelCommitReport(writer,{busy});endFrame(json);TEST_ASSERT_FALSE(json.overflowed());
  JsonDocument doc;TEST_ASSERT_FALSE(deserializeJson(doc,text));const auto payload=doc["d"].as<JsonObject>();
  TEST_ASSERT_FALSE(protoBool(payload,proto_field_t::durable));TEST_ASSERT_FALSE(protoBool(payload,proto_field_t::rebooting));
  TEST_ASSERT_EQUAL(uint8_t(proto_error_t::busy),protoInt(payload,proto_field_t::code));
  TEST_ASSERT_NOT_NULL(strstr(protoString(payload,proto_field_t::desc),"memory"));
  TEST_ASSERT_NULL(strstr(protoString(payload,proto_field_t::desc),"Previous file"));
}
int main() {
  UNITY_BEGIN();
  RUN_TEST(sealed_filesystem_reports_unsaved_ram_without_claiming_previous_disk_file);
  RUN_TEST(expired_upload_recovery_uses_existing_wire_fields_without_a_save_claim);
  RUN_TEST(applied_failure_retains_id_and_actual_change_fanout);
  RUN_TEST(
      reply_preserves_new_id_and_distinguishes_persistence_failure_from_refusal);
  RUN_TEST(commit_retry_payload_is_empty_and_schema_dispatch_is_device_only);
  RUN_TEST(
      saved_file_report_truthfully_distinguishes_restart_scheduled_or_deferred);
  RUN_TEST(model_persistence_event_frames_keep_failure_and_uncertainty_visible);
  return UNITY_END();
}
