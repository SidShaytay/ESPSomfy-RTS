#include "LocalAdvanced.h"
#include "LocalJsonAllocator.h"
#include <map>
#include <stdlib.h>
#include <string.h>
#include <unity.h>
using namespace LocalUi;
using namespace EntityAdmin;
using namespace Advanced;
static unsigned releases;
static size_t peak;
static std::map<void *, size_t> blocks;
static void *acquire(size_t n) {
  if (n > peak)
    peak = n;
  void *p = malloc(n);
  if (p)
    blocks[p] = n;
  return p;
}
static void release(void *p) {
  for (size_t i = 0; i < blocks[p]; i++)
    TEST_ASSERT_EQUAL_UINT8(0, static_cast<unsigned char *>(p)[i]);
  blocks.erase(p);
  ++releases;
  free(p);
}
struct Model : Advanced::Backend {
  Authorization auth{true, 1, 1};
  uint64_t history = 1;
  unsigned saves = 0, retries = 0, targetReads = 0;
  bool marked = false;
  bool stale = false, badLabel = false;
  uint8_t total = 1;
  uint32_t repeater = 1234;
  Admission refusal = Admission::accepted, retryRefusal = Admission::accepted;
  ModelCommitState file = ModelCommitState::saved;
  Authorization authorization() override { return auth; }
  uint64_t revision() override { return history; }
  void activity() override {}
  bool targets(Scope, OrderIds &ids, OrderIds &slots, uint8_t &count) override {
    ++targetReads;
    for (uint8_t i = 0; i < total; i++) {
      ids[i] = i ? (i >= 27 ? i + 1 : i) : 27;
      slots[i] = 31 - i;
    }
    count = total;
    return true;
  }
  bool targetLabel(Target target, uint8_t slot, Choice &out) override {
    if (slot == 31) {
      out.value = 27;
      strcpy(out.label, "#27 Office");
    } else {
      out.value = target.id;
      snprintf(out.label, sizeof(out.label), "#%u Device", 31 - slot);
    }
    if (badLabel)
      ++out.value;
    return true;
  }
  bool capture(Target target, Context &out) override {
    out = {};
    out.scope = target.scope;
    out.history = history;
    if (target.scope != Scope::repeaters) {
      auto &r = out.raw;
      r.ref = {target.scope == Scope::devices ? EntityKind::device
                                              : EntityKind::group,
               target.id};
      r.targetExists = true;
      r.identity = 100;
      r.revision = r.modelRevision = r.restoreRevision = r.radioRevision = 1;
      if (stale)
        ++r.identity;
    } else
      out.repeaters[0] = repeater;
    return true;
  }
  bool paired(Target, bool &out) override {
    out = marked;
    return true;
  }
  uint8_t itemCount(Target) override { return 1; }
  bool itemAt(Target, uint8_t index, Item &out) override {
    if (index)
      return false;
    out = {};
    out.key = 1234;
    out.slot = 6;
    out.missing = true;
    strcpy(out.label, "1234");
    return true;
  }
  ApplyResult apply(Target, const Action &action, const Context &,
                    Authorization, uint64_t,
                    ArduinoJson::Allocator &) override {
    if (refusal != Admission::accepted)
      return {{}, refusal};
    ++saves;
    marked = action.paired;
    ++history;
    return {edit_result_t::completed(27, {file, ModelCommitStage::write})};
  }
  CommitResult retryFile(Target, const Context &, Authorization,
                         uint64_t) override {
    if (retryRefusal != Admission::accepted)
      return {{}, retryRefusal};
    ++retries;
    return {{ModelCommitState::saved}};
  }
};
void setUp() {
  releases = 0;
  peak = 0;
  TEST_ASSERT_TRUE(blocks.empty());
}
void tearDown() { TEST_ASSERT_TRUE(blocks.empty()); }
static void cancel_is_default_and_rotation_never_applies() {
  Model model;
  JsonAllocator memory(4096, acquire, release);
  Advanced::Form form(model, memory);
  TEST_ASSERT_TRUE(form.open(Scope::devices, 10));
  form.activate(11); // choose #27
  FormView view;
  form.render(view);
  TEST_ASSERT_EQUAL_STRING("#27 Office", view.title);
  form.activate(12); // paired marker -> confirm
  for (int i = 0; i < 30; i++)
    form.render(view);
  TEST_ASSERT_EQUAL(1, model.targetReads);
  TEST_ASSERT_LESS_OR_EQUAL(2304, peak);
  TEST_ASSERT_EQUAL(0, model.saves);
  TEST_ASSERT_EQUAL(Mode::confirm, view.mode);
  TEST_ASSERT_EQUAL_STRING("Marker only. Motor pairing is unchanged.",
                           view.hint);
  TEST_ASSERT_EQUAL(0, view.selected);
  form.activate(13); // default Cancel
  TEST_ASSERT_EQUAL(0, model.saves);
  form.discard();
  TEST_ASSERT_EQUAL(0, memory.used());
}
static void applied_failure_retries_file_only() {
  Model model;
  model.file = ModelCommitState::failed;
  JsonAllocator memory(4096, acquire, release);
  Advanced::Form form(model, memory);
  form.open(Scope::devices, 10);
  form.activate(11);
  form.activate(12);
  form.turn(1, 13);
  form.activate(14);
  FormView view;
  form.render(view);
  TEST_ASSERT_EQUAL(Mode::result, view.mode);
  TEST_ASSERT_EQUAL(Notice::appliedNotSaved, view.notice);
  TEST_ASSERT_EQUAL(1, model.saves);
  form.turn(-1, 15);
  form.activate(16);
  TEST_ASSERT_EQUAL(1, model.retries);
  TEST_ASSERT_EQUAL(1, model.saves);
  TEST_ASSERT_TRUE(form.closed());
  TEST_ASSERT_EQUAL(0, memory.used());
}
static void context_and_authorization_changes_close_and_free() {
  for (unsigned which = 0; which < 3; which++) {
    Model model;
    JsonAllocator memory(4096, acquire, release);
    Advanced::Form form(model, memory);
    form.open(Scope::devices, 10);
    form.activate(11);
    form.activate(12);
    if (which == 0)
      ++model.auth.generation;
    else if (which == 1)
      model.stale = true;
    else
      ++model.history;
    form.tick(13);
    TEST_ASSERT_TRUE(form.closed());
    TEST_ASSERT_EQUAL(0, memory.used());
    TEST_ASSERT_EQUAL(0, model.saves);
  }
}
static void allocation_timeout_and_repeater_context() {
  Model model;
  JsonAllocator none(0, acquire, release);
  Advanced::Form denied(model, none);
  TEST_ASSERT_FALSE(denied.open(Scope::devices, 1));
  TEST_ASSERT_EQUAL(Notice::noMemory, denied.notice());
  JsonAllocator memory(4096, acquire, release);
  Advanced::Form form(model, memory);
  form.open(Scope::repeaters, 10);
  FormView view;
  form.render(view);
  TEST_ASSERT_EQUAL_STRING("Repeaters", view.title);
  form.activate(11);
  form.render(view);
  TEST_ASSERT_EQUAL(Mode::confirm, view.mode);
  TEST_ASSERT_TRUE(view.protectedFrame);
  form.tick(120011);
  TEST_ASSERT_TRUE(form.closed());
  TEST_ASSERT_EQUAL(0, memory.used());
  TEST_ASSERT_EQUAL(0, model.saves);
}
static void full_paging_repeater_change_and_admission_failure() {
  Model model;
  model.total = 32;
  JsonAllocator memory(4096, acquire, release);
  Advanced::Form form(model, memory);
  TEST_ASSERT_TRUE(form.open(Scope::devices, 10));
  for (unsigned i = 0; i < 31; i++)
    form.turn(1, 11 + i);
  FormView view;
  form.render(view);
  TEST_ASSERT_EQUAL(32, view.total);
  TEST_ASSERT_EQUAL(2, view.rowCount);
  TEST_ASSERT_EQUAL(30, view.offset);
  form.discard();
  form.open(Scope::repeaters, 100);
  model.repeater = 2345;
  form.tick(101);
  TEST_ASSERT_TRUE(form.closed());
  TEST_ASSERT_EQUAL(Notice::changed, form.notice());
  model.total = 1;
  form.open(Scope::devices, 200);
  form.activate(201);
  form.activate(202);
  form.turn(1, 203);
  model.refusal = Admission::noMemory;
  form.activate(204);
  TEST_ASSERT_TRUE(form.closed());
  TEST_ASSERT_EQUAL(Notice::noMemory, form.notice());
  TEST_ASSERT_EQUAL(0, model.saves);
  TEST_ASSERT_EQUAL(0, memory.used());
}
static void file_retry_busy_keeps_commit_only_result() {
  Model model;
  model.file = ModelCommitState::failed;
  JsonAllocator memory(4096, acquire, release);
  Advanced::Form form(model, memory);
  form.open(Scope::devices, 10);
  form.activate(11);
  form.activate(12);
  form.turn(1, 13);
  form.activate(14);
  model.retryRefusal = Admission::busy;
  form.turn(-1, 15);
  form.activate(16);
  FormView view;
  form.render(view);
  TEST_ASSERT_EQUAL(Mode::result, view.mode);
  TEST_ASSERT_EQUAL(1, view.selected);
  TEST_ASSERT_EQUAL(1, model.saves);
  TEST_ASSERT_EQUAL(0, model.retries);
  model.retryRefusal = Admission::accepted;
  form.turn(-1, 17);
  form.activate(18);
  TEST_ASSERT_EQUAL(1, model.retries);
  TEST_ASSERT_EQUAL(1, model.saves);
}
static void stable_label_identity_is_required() {
  for (unsigned picker = 0; picker < 2; picker++) {
    Model model;
    JsonAllocator memory(4096, acquire, release);
    Advanced::Form form(model, memory);
    form.open(Scope::devices, 10);
    if (!picker)
      form.activate(11);
    model.badLabel = true;
    FormView view;
    form.render(view);
    TEST_ASSERT_TRUE(form.closed());
    TEST_ASSERT_EQUAL(Notice::changed, view.notice);
    TEST_ASSERT_EQUAL(0, memory.used());
  }
}
static void confirmation_names_exact_association_and_configuration_effect() {
  Model model;
  JsonAllocator memory(4096, acquire, release);
  Advanced::Form form(model, memory);
  for (auto scope : {Scope::devices, Scope::groups, Scope::repeaters}) {
    form.open(scope, 10);
    if (scope != Scope::repeaters)
      form.activate(11);
    if (scope == Scope::devices) {
      form.turn(1, 12);
      form.activate(13);
    }
    form.activate(14);
    FormView view;
    form.render(view);
    TEST_ASSERT_EQUAL(Mode::confirm, view.mode);
    TEST_ASSERT_NOT_NULL(strstr(view.rows[1].label, "1234"));
    TEST_ASSERT_EQUAL_STRING(
        scope == Scope::groups ? "Remove all stale links. No physical unpair."
        : scope == Scope::repeaters ? "Stop future repeats. No physical unpair."
                                    : "Remove association. No physical unpair.",
        view.hint);
    TEST_ASSERT_EQUAL(0, view.selected);
    form.discard();
  }
  TEST_ASSERT_EQUAL(0, model.saves);
}
int main() {
  UNITY_BEGIN();
  RUN_TEST(cancel_is_default_and_rotation_never_applies);
  RUN_TEST(applied_failure_retries_file_only);
  RUN_TEST(context_and_authorization_changes_close_and_free);
  RUN_TEST(allocation_timeout_and_repeater_context);
  RUN_TEST(full_paging_repeater_change_and_admission_failure);
  RUN_TEST(file_retry_busy_keeps_commit_only_result);
  RUN_TEST(stable_label_identity_is_required);
  RUN_TEST(confirmation_names_exact_association_and_configuration_effect);
  return UNITY_END();
}
