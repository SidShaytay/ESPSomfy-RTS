#include "LocalOrder.h"
#include <string.h>
#include <unity.h>
using namespace LocalUi;
using namespace LocalUi::EntityAdmin;
struct Memory : ArduinoJson::Allocator {
  unsigned allocations = 0;
  void *allocate(size_t) override {
    ++allocations;
    return nullptr;
  }
  void deallocate(void *) override {}
  void *reallocate(void *, size_t) override {
    ++allocations;
    return nullptr;
  }
};
struct Model : Backend {
  OrderIds live{4, 27, 9}, physical{5, 1, 12};
  Authorization auth{true, 1, 1};
  uint64_t history = 1;
  uint8_t count = 3;
  bool normalize = false;
  unsigned saves = 0, commits = 0, labels = 0, captures = 0;
  Admission refusal = Admission::accepted, retryRefusal = Admission::accepted;
  ModelCommitState file = ModelCommitState::saved;
  Authorization authorization() override { return auth; }
  uint64_t revision() override { return history; }
  void activity() override {}
  bool prepare(Kind, uint8_t, uint8_t &) override { return false; }
  bool read(Kind, uint8_t, proto_field_t, Value &) override { return false; }
  ApplyResult apply(model_edit_type_t, JsonDocument &, Authorization,
                    uint64_t) override {
    return {};
  }
  CommitResult retryFileCommit(Authorization, uint64_t) override { return {}; }
  bool readOrder(Kind, OrderIds &ids, OrderIds &slots, uint8_t &total,
                 bool &needed) override {
    ++captures;
    memcpy(ids, live, sizeof(live));
    memcpy(slots, physical, sizeof(physical));
    total = count;
    needed = normalize;
    return true;
  }
  bool readOrderLabel(Kind, uint8_t slot, uint8_t id, Value &out) override {
    ++labels;
    for (uint8_t i = 0; i < count; i++)
      if (live[i] == id && physical[i] == slot) {
        out = {};
        out.kind = 's';
        strcpy(out.text, "Duplicate name");
        return true;
      }
    return false;
  }
  ApplyResult applyOrder(Kind, const uint8_t *ids, uint8_t total,
                         ArduinoJson::Allocator &, Authorization,
                         uint64_t) override {
    if (refusal != Admission::accepted)
      return {{}, refusal};
    ++saves;
    TEST_ASSERT_EQUAL(count, total);
    OrderIds positions{};
    for (uint8_t i = 0; i < count; i++)
      for (uint8_t j = 0; j < count; j++)
        if (live[j] == ids[i])
          positions[i] = physical[j];
    memcpy(live, ids, count);
    memcpy(physical, positions, sizeof(physical));
    normalize = false;
    ++history;
    return {edit_result_t::completed(0, {file, ModelCommitStage::write})};
  }
  CommitResult retryOrderFile(Authorization, uint64_t) override {
    if (retryRefusal != Admission::accepted)
      return {{}, retryRefusal};
    ++commits;
    return {{file, ModelCommitStage::write}};
  }
};
void setUp() {}
void tearDown() {}
static void move_is_draft_only_and_back_restores_previous_draft() {
  Model model;
  Memory memory;
  Ordering::Form form(model, memory);
  FormView view;
  TEST_ASSERT_TRUE(form.open(Kind::shade, 0));
  form.activate(1);
  form.turn(1, 1);
  form.render(view);
  TEST_ASSERT_EQUAL(27, model.live[1]);
  TEST_ASSERT_EQUAL(4, view.id);
  TEST_ASSERT_EQUAL(1, view.selected);
  form.back(1);
  form.render(view);
  TEST_ASSERT_EQUAL(4, view.id);
  TEST_ASSERT_EQUAL(0, view.selected);
  form.activate(1);
  form.turn(1, 1);
  form.activate(1); // Apply local move.
  form.render(view);
  TEST_ASSERT_TRUE(view.dirty);
  TEST_ASSERT_EQUAL(4, view.id);
  form.activate(1);
  form.turn(1, 1);
  form.back(1); // Cancel a second move.
  form.render(view);
  TEST_ASSERT_EQUAL(4, view.id);
  TEST_ASSERT_EQUAL(1, view.selected);
  TEST_ASSERT_EQUAL(0, model.saves);
  TEST_ASSERT_EQUAL(0, memory.allocations);
  TEST_ASSERT_EQUAL(4, model.live[0]);
}
static void
explicit_default_cancel_save_normalizes_and_applied_failure_retries_file_only() {
  Model model;
  model.normalize = true;
  model.file = ModelCommitState::failed;
  Memory memory;
  Ordering::Form form(model, memory);
  FormView view;
  form.open(Kind::shade, 0);
  for (int i = 0; i < 3; i++)
    form.turn(1, 1);
  form.activate(1);
  form.render(view);
  TEST_ASSERT_EQUAL(Mode::confirm, view.mode);
  TEST_ASSERT_EQUAL(0, view.selected);
  form.activate(1);
  TEST_ASSERT_EQUAL(0, model.saves); // Cancel confirmation.
  form.activate(1);
  form.turn(1, 1);
  form.activate(1);
  form.render(view);
  TEST_ASSERT_EQUAL(Mode::result, view.mode);
  TEST_ASSERT_EQUAL(Notice::appliedNotSaved, view.notice);
  TEST_ASSERT_EQUAL(1, model.saves);
  TEST_ASSERT_FALSE(view.dirty);
  model.file = ModelCommitState::saved;
  form.turn(1, 1);
  form.activate(1);
  TEST_ASSERT_EQUAL(1, model.commits);
  TEST_ASSERT_EQUAL(1, model.saves);
  form.activate(1);
  TEST_ASSERT_TRUE(form.closed());
}
static void all_32_entries_are_bounded_and_labels_use_cached_slots() {
  Model model;
  model.count = 32;
  for (uint8_t i = 0; i < 32; i++) {
    model.live[i] = i + 1;
    model.physical[i] = 31 - i;
  }
  Memory memory;
  Ordering::Form form(model, memory);
  FormView view;
  form.open(Kind::shade, 0);
  const auto captures = model.captures;
  for (int i = 0; i < 31; i++)
    form.turn(1, 1);
  form.render(view);
  TEST_ASSERT_EQUAL(32, view.id);
  TEST_ASSERT_EQUAL(34, view.total);
  TEST_ASSERT_LESS_OR_EQUAL(3, view.rowCount);
  TEST_ASSERT_EQUAL(captures, model.captures);
  TEST_ASSERT_EQUAL(0, memory.allocations);
  TEST_ASSERT_TRUE(view.protectedFrame);
}
static void stale_authority_timeout_and_cancel_never_save() {
  for (unsigned scenario = 0; scenario < 5; scenario++) {
    Model model;
    Memory memory;
    Ordering::Form form(model, memory);
    form.open(Kind::group, 0);
    form.activate(1);
    form.turn(1, 1);
    if (scenario == 0)
      model.auth.allowed = false;
    if (scenario == 1)
      ++model.auth.configurationGeneration;
    if (scenario == 2)
      ++model.history;
    if (scenario == 3)
      form.tick(Ordering::Form::timeoutMs + 1);
    if (scenario == 4)
      form.discard();
    else
      form.tick(2);
    TEST_ASSERT_TRUE(form.closed());
    TEST_ASSERT_EQUAL(0, model.saves);
    TEST_ASSERT_EQUAL(0, model.commits);
  }
}
static void
busy_file_retry_preserves_result_without_replaying_an_empty_order() {
  Model model;
  model.normalize = true;
  model.file = ModelCommitState::failed;
  Memory memory;
  Ordering::Form form(model, memory);
  FormView view;
  form.open(Kind::shade, 0);
  for (int i = 0; i < 3; i++)
    form.turn(1, 1);
  form.activate(1);
  form.turn(1, 1);
  form.activate(1);
  model.retryRefusal = Admission::busy;
  form.turn(1, 1);
  form.activate(1);
  form.render(view);
  TEST_ASSERT_EQUAL(Mode::result, view.mode);
  TEST_ASSERT_EQUAL(Notice::busy, view.notice);
  model.retryRefusal = Admission::accepted;
  model.file = ModelCommitState::saved;
  form.turn(1, 1);
  form.activate(1);
  TEST_ASSERT_EQUAL(1, model.commits);
  TEST_ASSERT_EQUAL(1, model.saves);
}
static void no_change_empty_cancel_and_failed_save_do_not_replay() {
  for (const auto kind : {Kind::room, Kind::shade, Kind::group}) {
    Model model;
    Memory memory;
    Ordering::Form form(model, memory);
    form.open(kind, 0);
    for (int i = 0; i < 3; i++)
      form.turn(1, 1);
    form.activate(1);
    TEST_ASSERT_TRUE(form.closed());
    TEST_ASSERT_EQUAL(Notice::unchanged, form.notice());
    TEST_ASSERT_EQUAL(0, model.saves);
    model.count = 0;
    form.open(kind, 2);
    form.activate(3);
    TEST_ASSERT_TRUE(form.closed());
    TEST_ASSERT_EQUAL(0, model.saves);
    model.count = 3;
    model.normalize = true;
    form.open(kind, 4);
    for (int i = 0; i < 3; i++)
      form.turn(1, 5);
    form.activate(5);
    form.back(5);
    form.back(5);
    TEST_ASSERT_TRUE(form.closed());
    TEST_ASSERT_EQUAL(0, model.saves);
  }
}
static void busy_and_memory_refusal_and_stale_file_result_are_explicit() {
  for (const auto refusal : {Admission::busy, Admission::noMemory}) {
    Model model;
    model.normalize = true;
    model.refusal = refusal;
    Memory memory;
    Ordering::Form form(model, memory);
    FormView view;
    form.open(Kind::shade, 0);
    for (int i = 0; i < 3; i++)
      form.turn(1, 1);
    form.activate(1);
    form.turn(1, 1);
    form.activate(1);
    form.render(view);
    TEST_ASSERT_EQUAL(0, model.saves);
    if (refusal == Admission::busy) {
      TEST_ASSERT_FALSE(form.closed());
      TEST_ASSERT_EQUAL(Notice::busy, view.notice);
      TEST_ASSERT_TRUE(view.dirty);
    } else {
      TEST_ASSERT_TRUE(form.closed());
      TEST_ASSERT_EQUAL(Notice::noMemory, form.notice());
    }
  }
  Model model;
  model.normalize = true;
  model.file = ModelCommitState::uncertain;
  Memory memory;
  Ordering::Form form(model, memory);
  form.open(Kind::shade, 0);
  for (int i = 0; i < 3; i++)
    form.turn(1, 1);
  form.activate(1);
  form.turn(1, 1);
  form.activate(1);
  ++model.history;
  form.turn(1, 1);
  form.activate(1);
  TEST_ASSERT_TRUE(form.closed());
  TEST_ASSERT_EQUAL(0, model.commits);
  TEST_ASSERT_EQUAL(1, model.saves);
}
static void
malformed_snapshot_or_slot_label_and_clock_regression_close_safely() {
  for (unsigned scenario = 0; scenario < 4; scenario++) {
    Model model;
    Memory memory;
    Ordering::Form form(model, memory);
    FormView view;
    if (scenario == 0)
      model.live[1] = model.live[0];
    if (scenario == 1)
      model.physical[1] = model.physical[0];
    const bool opened = form.open(Kind::shade, 100);
    if (scenario < 2)
      TEST_ASSERT_FALSE(opened);
    else if (scenario == 2) {
      model.physical[0] = 6;
      form.render(view);
      TEST_ASSERT_TRUE(form.closed());
    } else {
      form.tick(200);
      form.turn(1, 199);
      TEST_ASSERT_TRUE(form.closed());
      TEST_ASSERT_EQUAL(Notice::expired, form.notice());
    }
    TEST_ASSERT_EQUAL(0, model.saves);
    TEST_ASSERT_EQUAL(0, model.commits);
  }
}
int main() {
  UNITY_BEGIN();
  RUN_TEST(no_change_empty_cancel_and_failed_save_do_not_replay);
  RUN_TEST(busy_and_memory_refusal_and_stale_file_result_are_explicit);
  RUN_TEST(malformed_snapshot_or_slot_label_and_clock_regression_close_safely);
  RUN_TEST(busy_file_retry_preserves_result_without_replaying_an_empty_order);
  RUN_TEST(move_is_draft_only_and_back_restores_previous_draft);
  RUN_TEST(
      explicit_default_cancel_save_normalizes_and_applied_failure_retries_file_only);
  RUN_TEST(all_32_entries_are_bounded_and_labels_use_cached_slots);
  RUN_TEST(stale_authority_timeout_and_cancel_never_save);
  return UNITY_END();
}
