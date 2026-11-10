#define ESPSOMFY_LOCAL_UI
#include <unity.h>
#include "LocalSettingsRuntime.h"
#include "LocalUiAuthorizationAdapter.h"
#include "DeviceOps.h"
#include "LocalOperationAdmission.h"
#include "LoopQueue.h"
LoopQueue loopQueue;
static uint32_t pendingItems=0;
uint32_t LoopQueue::pending() const { return pendingItems; }
static LocalUi::SecuritySnapshot state;
static bool frozen=false;
static uint64_t sectionRevision=1;
static int saves=0,reads=0,activities=0;
static uint32_t origin=99;
namespace LocalUi {
SecuritySnapshot pollAuthorization() { return state; }
void authorizationActivity() { ++activities; }
bool localControlsFrozen() { return frozen; }
}
uint64_t deviceSettingsRevision(settings_section_t section) { return uint8_t(section)<5?sectionRevision:0; }
bool readDeviceSettings(settings_section_t,SettingsSnapshotSink &) { ++reads; return true; }
settings_apply_result_t applyDeviceSettings(settings_section_t section,JsonObject request,uint32_t caller) {
  TEST_ASSERT_EQUAL(settings_section_t::general,section);
  TEST_ASSERT_EQUAL_STRING("local-name",request["hostname"].as<const char *>());
  ++saves; origin=caller; return {};
}
static bool pendingRestart=false;
bool restartPending(){return pendingRestart;}
#include "../../src/LocalSettingsRuntime.cpp"
void setUp() { pendingRestart=false; state={LocalUi::AuthMode::pin,true,false,3,5,0}; frozen=false;sectionRevision=1;saves=reads=activities=0;origin=99;pendingItems=0; }
void tearDown() {}
struct Sink : SettingsSnapshotSink { void field(proto_field_t,const settings_value_t &) override {} };
static JsonDocument request() { JsonDocument doc; doc["hostname"]="local-name"; return doc; }
static void current_authorized_draft_uses_loop_owned_shared_domain() {
  auto &backend=LocalUi::Settings::runtimeBackend(); auto doc=request();
  const auto expected=backend.authorization();
  TEST_ASSERT_TRUE(backend.apply(settings_section_t::general,doc.as<JsonObject>(),expected,1).saved());
  TEST_ASSERT_EQUAL(1,saves);TEST_ASSERT_EQUAL(0,origin);
  Sink sink; TEST_ASSERT_TRUE(backend.read(settings_section_t::general,sink));TEST_ASSERT_EQUAL(1,reads);
  backend.activity();TEST_ASSERT_EQUAL(1,activities);
}
static void fresh_expiry_or_auth_replacement_refuses_before_storage() {
  auto &backend=LocalUi::Settings::runtimeBackend();auto doc=request();const auto admitted=backend.authorization();
  state.allowed=false;
  auto result=backend.apply(settings_section_t::general,doc.as<JsonObject>(),admitted,1);
  TEST_ASSERT_FALSE(result.saved());TEST_ASSERT_EQUAL(settings_apply_refusal_t::unauthorized,result.refusal);
  state.allowed=true;++state.generation;
  result=backend.apply(settings_section_t::general,doc.as<JsonObject>(),admitted,1);
  TEST_ASSERT_EQUAL(settings_apply_refusal_t::stale,result.refusal);
  --state.generation;++state.configurationGeneration;
  result=backend.apply(settings_section_t::general,doc.as<JsonObject>(),admitted,1);
  TEST_ASSERT_EQUAL(settings_apply_refusal_t::stale,result.refusal);TEST_ASSERT_EQUAL(0,saves);
}
static void same_value_section_save_and_invalid_history_refuse_stale_drafts() {
  auto &backend=LocalUi::Settings::runtimeBackend();auto doc=request();const auto admitted=backend.authorization();
  ++sectionRevision;
  TEST_ASSERT_EQUAL(settings_apply_refusal_t::stale,backend.apply(settings_section_t::general,doc.as<JsonObject>(),admitted,1).refusal);
  TEST_ASSERT_EQUAL(settings_apply_refusal_t::stale,backend.apply(settings_section_t::general,doc.as<JsonObject>(),admitted,0).refusal);
  TEST_ASSERT_EQUAL(settings_apply_refusal_t::stale,backend.apply(settings_section_t(255),doc.as<JsonObject>(),admitted,1).refusal);
  auto missing=admitted;missing.generation=0;
  TEST_ASSERT_EQUAL(settings_apply_refusal_t::stale,backend.apply(settings_section_t::general,doc.as<JsonObject>(),missing,2).refusal);
  TEST_ASSERT_EQUAL(0,saves);
}
static void frozen_or_locked_service_neither_reads_nor_saves() {
  auto &backend=LocalUi::Settings::runtimeBackend();auto doc=request();const auto admitted=backend.authorization();Sink sink;
  frozen=true;
  TEST_ASSERT_FALSE(backend.authorization().allowed);TEST_ASSERT_FALSE(backend.read(settings_section_t::general,sink));
  TEST_ASSERT_EQUAL(settings_apply_refusal_t::unauthorized,backend.apply(settings_section_t::general,doc.as<JsonObject>(),admitted,1).refusal);
  frozen=false;state.allowed=false;
  TEST_ASSERT_FALSE(backend.read(settings_section_t::general,sink));
  TEST_ASSERT_EQUAL(0,saves);TEST_ASSERT_EQUAL(0,reads);
}
static void pending_work_refuses_save_without_hiding_later_auth_changes() {
  auto &backend=LocalUi::Settings::runtimeBackend();auto doc=request();const auto expected=backend.authorization();
  pendingItems=1;
  auto result=backend.apply(settings_section_t::general,doc.as<JsonObject>(),expected,1);
  TEST_ASSERT_EQUAL(settings_apply_refusal_t::busy,result.refusal);
  TEST_ASSERT_FALSE(result.saved());TEST_ASSERT_EQUAL(0,saves);
  pendingItems=0;
  TEST_ASSERT_TRUE(backend.apply(settings_section_t::general,doc.as<JsonObject>(),expected,1).saved());
  TEST_ASSERT_EQUAL(1,saves);
  pendingItems=1;state.allowed=false;
  TEST_ASSERT_EQUAL(settings_apply_refusal_t::unauthorized,backend.apply(settings_section_t::general,doc.as<JsonObject>(),expected,1).refusal);
  TEST_ASSERT_EQUAL(1,saves);
}
static void admitted_or_queued_restart_refuses_local_settings_before_effects() {
  auto &backend=LocalUi::Settings::runtimeBackend();auto doc=request();
  const auto expected=backend.authorization();pendingRestart=true;
  const auto result=backend.apply(settings_section_t::general,doc.as<JsonObject>(),expected,1);
  TEST_ASSERT_EQUAL(settings_apply_refusal_t::busy,result.refusal);TEST_ASSERT_EQUAL(0,saves);
}
int main() {
  UNITY_BEGIN();RUN_TEST(admitted_or_queued_restart_refuses_local_settings_before_effects);
  RUN_TEST(current_authorized_draft_uses_loop_owned_shared_domain);
  RUN_TEST(fresh_expiry_or_auth_replacement_refuses_before_storage);
  RUN_TEST(same_value_section_save_and_invalid_history_refuse_stale_drafts);
  RUN_TEST(frozen_or_locked_service_neither_reads_nor_saves);
  RUN_TEST(pending_work_refuses_save_without_hiding_later_auth_changes);
  return UNITY_END();
}
