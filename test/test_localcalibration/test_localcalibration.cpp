#include "LocalCalibration.h"
#include "LocalJsonAllocator.h"
#include <unity.h>
#include <stdlib.h>
#include <string.h>
#include <vector>
using namespace LocalUi;

struct Catalogue : CatalogueSource {
  bool read(CatalogueKind kind, uint8_t slot, CatalogueEntry &out) override {
    if(kind != CatalogueKind::devices || slot >= 2) return false;
    out = {};
    // Stable IDs intentionally differ from slot and display row numbers.
    out.id = slot ? 3 : 7;
    out.sortOrder = slot;
    out.identity = 1000 + out.id;
    out.revision = 20 + out.id;
    out.available = true;
    strcpy(out.name, slot ? "Compatible shutter" : "Unsupported device");
    return true;
  }
};
struct Call {
  uint32_t token;
  LiftGuideAction action;
  EntityAdmin::Authorization auth;
  bool human;
};
struct Backend : Calibration::Backend {
  EntityAdmin::Authorization auth{true, 11, 12};
  uint64_t modelRevision = 20;
  bool allowBegin = true, allowAct = true, allowSnapshot = true;
  unsigned begins = 0, activityCount = 0;
  uint8_t begunId = 0;
  LiftKind begunKind = LiftKind::legacy;
  EntityAdmin::Authorization begunAuth;
  uint32_t nextToken = 91;
  LiftGuideSnapshot state;
  std::vector<Call> calls;
  EntityAdmin::Authorization authorization() override { return auth; }
  uint64_t revision() override { return modelRevision; }
  void activity() override { ++activityCount; }
  bool available(uint8_t id) override { return id == 3; }
  bool begin(uint8_t id, LiftKind kind, EntityAdmin::Authorization authorization, uint32_t &token) override {
    ++begins; begunId = id; begunKind = kind; begunAuth = authorization;
    if(!allowBegin) return false;
    state = {}; state.id = id; state.token = token = nextToken++;
    state.stage = LiftGuideStage::reference; state.owned = true;
    return true;
  }
  bool act(uint32_t token, LiftGuideAction action, EntityAdmin::Authorization authorization, bool human) override {
    calls.push_back({token, action, authorization, human});
    if(!allowAct) return false;
    if(action == LiftGuideAction::cancel) { state.stage = LiftGuideStage::closed; state.owned = false; }
    return true;
  }
  bool snapshot(uint32_t token, EntityAdmin::Authorization, LiftGuideSnapshot &out) override {
    if(!allowSnapshot || token != state.token) return false;
    out = state;
    return true;
  }
};
struct Rig {
  Backend backend;
  Catalogue catalogue;
  JsonAllocator memory{4096, malloc, free};
  Calibration::Form form{&backend, catalogue, memory};
  EntityAdmin::FormView view() { EntityAdmin::FormView out; form.render(out); return out; }
  void chooseDevice(uint64_t at = 100) {
    TEST_ASSERT_TRUE(form.open(at));
    form.turn(1, at + 1); // Skip incompatible id7; select actual id3.
    form.activate(at + 2);
    TEST_ASSERT_EQUAL_UINT(0, backend.begins);
    TEST_ASSERT_EQUAL_UINT(3, view().total); // Direct, segmented, cancel.
  }
  void beginGuide(bool segmented = false) {
    chooseDevice();
    if(segmented) form.turn(1, 103);
    form.activate(104);
    TEST_ASSERT_EQUAL_UINT(1, backend.begins);
    TEST_ASSERT_FALSE(form.closed());
  }
};
void setUp() {}
void tearDown() {}

static void picker_refuses_unsupported_and_begins_only_after_hardware_choice() {
  Rig r;
  TEST_ASSERT_TRUE(r.form.open(100));
  const auto picker = r.view();
  TEST_ASSERT_EQUAL_UINT(2, picker.total); // Real devices only, no Add row.
  TEST_ASSERT_EQUAL_UINT(0, picker.selected);
  r.form.activate(101);
  TEST_ASSERT_EQUAL_UINT(0, r.backend.begins);
  TEST_ASSERT_TRUE(r.backend.calls.empty());
  TEST_ASSERT_FALSE(r.form.closed());
  r.form.turn(1, 102); r.form.activate(103);
  TEST_ASSERT_EQUAL_UINT(0, r.backend.begins);
  TEST_ASSERT_EQUAL_UINT(3, r.view().total);
  r.form.turn(1, 104); r.form.activate(105);
  TEST_ASSERT_EQUAL_UINT(1, r.backend.begins);
  TEST_ASSERT_EQUAL_UINT8(3, r.backend.begunId);
  TEST_ASSERT_EQUAL_UINT8(uint8_t(LiftKind::segmented), uint8_t(r.backend.begunKind));
  TEST_ASSERT_EQUAL_UINT32(11, r.backend.begunAuth.generation);
  TEST_ASSERT_EQUAL_UINT32(12, r.backend.begunAuth.configurationGeneration);
  TEST_ASSERT_TRUE(r.backend.calls.empty()); // Form has issued no RF action.
}
static void guide_forwards_current_action_once_and_resets_cursor_on_stage_change() {
  Rig r; r.beginGuide();
  TEST_ASSERT_EQUAL_UINT8(uint8_t(LiftKind::direct), uint8_t(r.backend.begunKind));
  TEST_ASSERT_EQUAL_UINT(4, r.view().total);
  r.form.activate(110, false);
  TEST_ASSERT_EQUAL_UINT(1, r.backend.calls.size());
  TEST_ASSERT_EQUAL_UINT8(uint8_t(LiftGuideAction::open), uint8_t(r.backend.calls[0].action));
  TEST_ASSERT_FALSE(r.backend.calls[0].human);
  TEST_ASSERT_EQUAL_UINT32(r.backend.state.token, r.backend.calls[0].token);
  TEST_ASSERT_EQUAL_UINT32(11, r.backend.calls[0].auth.generation);
  TEST_ASSERT_EQUAL_UINT32(12, r.backend.calls[0].auth.configurationGeneration);
  r.form.turn(1, 111); r.form.activate(112, true);
  TEST_ASSERT_EQUAL_UINT(2, r.backend.calls.size());
  TEST_ASSERT_EQUAL_UINT8(uint8_t(LiftGuideAction::confirmOpen), uint8_t(r.backend.calls[1].action));
  TEST_ASSERT_TRUE(r.backend.calls[1].human);
  r.form.turn(1, 113); // Cursor was on confirmOpen; now Stop.
  r.backend.state.stage = LiftGuideStage::readyDown;
  r.form.tick(114);
  TEST_ASSERT_EQUAL_UINT(0, r.view().selected);
  TEST_ASSERT_EQUAL_UINT(2, r.view().total);
  TEST_ASSERT_EQUAL_UINT(2, r.backend.calls.size()); // Polling invented no observation.
  r.form.activate(115, false);
  TEST_ASSERT_EQUAL_UINT(3, r.backend.calls.size());
  TEST_ASSERT_EQUAL_UINT8(uint8_t(LiftGuideAction::startDown), uint8_t(r.backend.calls[2].action));
  TEST_ASSERT_FALSE(r.backend.calls[2].human);
}
static void changed_model_or_authorization_before_begin_never_uses_stale_picker() {
  for(unsigned change = 0; change < 4; ++change) {
    Rig r; r.chooseDevice();
    if(change == 0) ++r.backend.modelRevision;
    if(change == 1) ++r.backend.auth.generation;
    if(change == 2) ++r.backend.auth.configurationGeneration;
    if(change == 3) r.backend.auth.allowed = false;
    r.form.activate(110);
    TEST_ASSERT_TRUE(r.form.closed());
    TEST_ASSERT_EQUAL_UINT(0, r.backend.begins);
    TEST_ASSERT_TRUE(r.backend.calls.empty());
    TEST_ASSERT_EQUAL_UINT(0, r.memory.used());
  }
}
static void back_and_discard_cancel_owned_session_once_without_replaying_actions() {
  Rig r; r.beginGuide();
  const auto token = r.backend.state.token;
  r.form.back(110);
  TEST_ASSERT_TRUE(r.form.closed());
  TEST_ASSERT_EQUAL_UINT(1, r.backend.calls.size());
  TEST_ASSERT_EQUAL_UINT8(uint8_t(LiftGuideAction::cancel), uint8_t(r.backend.calls[0].action));
  TEST_ASSERT_EQUAL_UINT32(token, r.backend.calls[0].token);
  TEST_ASSERT_FALSE(r.backend.calls[0].human);
  r.form.back(111); r.form.discard(); r.form.tick(112);
  TEST_ASSERT_EQUAL_UINT(1, r.backend.calls.size());
  TEST_ASSERT_EQUAL_UINT(0, r.memory.used());
}
static void refused_begin_is_correctable_and_failed_save_offers_no_rf_or_save_retry() {
  Rig r; r.chooseDevice();
  r.backend.allowBegin = false;
  r.form.activate(104);
  TEST_ASSERT_EQUAL_UINT(1, r.backend.begins);
  TEST_ASSERT_FALSE(r.form.closed());
  TEST_ASSERT_EQUAL_UINT(3, r.view().total);
  TEST_ASSERT_TRUE(r.backend.calls.empty());
  r.backend.allowBegin = true;
  r.form.activate(105);
  TEST_ASSERT_EQUAL_UINT(2, r.backend.begins);
  r.backend.state.stage = LiftGuideStage::failed;
  r.backend.state.applied = true; r.backend.state.durable = false; r.backend.state.owned = false;
  r.form.tick(110);
  TEST_ASSERT_EQUAL_UINT(1, r.view().total);
  r.form.turn(1, 111); r.form.activate(112);
  TEST_ASSERT_TRUE(r.form.closed());
  TEST_ASSERT_TRUE(r.backend.calls.size() <= 1);
  for(const auto &call : r.backend.calls)
    TEST_ASSERT_EQUAL_UINT8(uint8_t(LiftGuideAction::cancel), uint8_t(call.action));
  r.form.activate(113); r.form.discard();
  TEST_ASSERT_EQUAL_UINT(2, r.backend.begins);
  Rig completed; completed.beginGuide();
  completed.backend.state.stage = LiftGuideStage::complete;
  completed.backend.state.applied = completed.backend.state.durable = true;
  completed.backend.state.owned = false;
  completed.form.tick(110);
  TEST_ASSERT_EQUAL_UINT(1, completed.view().total);
  completed.form.activate(111);
  TEST_ASSERT_TRUE(completed.form.closed());
  for(const auto &call : completed.backend.calls)
    TEST_ASSERT_EQUAL_UINT8(uint8_t(LiftGuideAction::cancel), uint8_t(call.action));
}
static void unavailable_backend_and_allocation_failure_never_begin_or_leave_session() {
  Backend backend; Catalogue catalogue;
  JsonAllocator none{1, malloc, free};
  Calibration::Form unavailable{nullptr, catalogue, none};
  TEST_ASSERT_FALSE(unavailable.available());
  TEST_ASSERT_FALSE(unavailable.open(100));
  Calibration::Form denied{&backend, catalogue, none};
  TEST_ASSERT_FALSE(denied.open(100));
  TEST_ASSERT_TRUE(denied.closed());
  TEST_ASSERT_EQUAL_UINT(0, backend.begins);
  TEST_ASSERT_TRUE(backend.calls.empty());
  TEST_ASSERT_EQUAL_UINT(0, none.used());
}
int main() {
  UNITY_BEGIN();
  RUN_TEST(picker_refuses_unsupported_and_begins_only_after_hardware_choice);
  RUN_TEST(guide_forwards_current_action_once_and_resets_cursor_on_stage_change);
  RUN_TEST(changed_model_or_authorization_before_begin_never_uses_stale_picker);
  RUN_TEST(back_and_discard_cancel_owned_session_once_without_replaying_actions);
  RUN_TEST(refused_begin_is_correctable_and_failed_save_offers_no_rf_or_save_retry);
  RUN_TEST(unavailable_backend_and_allocation_failure_never_begin_or_leave_session);
  return UNITY_END();
}
