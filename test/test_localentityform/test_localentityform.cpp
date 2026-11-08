#include "LocalEntityForm.h"
#include <stdlib.h>
#include <string.h>
#include <unity.h>
using namespace LocalUi::EntityAdmin;
struct Memory : ArduinoJson::Allocator {
  unsigned active = 0;
  bool fail = false;
  void *allocate(size_t n) override {
    if (fail)
      return nullptr;
    auto *p = malloc(n);
    if (p)
      ++active;
    return p;
  }
  void deallocate(void *p) override {
    if (p) {
      --active;
      free(p);
    }
  }
  void *reallocate(void *p, size_t n) override {
    return fail ? nullptr : realloc(p, n);
  }
};
struct Fake : Backend {
  Authorization auth{true, 1, 1};
  uint64_t rev = 1;
  unsigned applies = 0, commits = 0;
  bool late = false, busy = false;
  int originalOrder = 0;
  const char *originalName = "Room";
  ModelCommitState outcome = ModelCommitState::saved;
  JsonDocument received;
  model_edit_type_t receivedType;
  Authorization authorization() override { return auth; }
  uint64_t revision() override { return rev; }
  bool prepare(Kind kind, uint8_t id, uint8_t &offered) override {
    offered = id ? id : 9;
    return kind == Kind::room;
  }
  bool read(Kind, uint8_t, proto_field_t f, Value &v) override {
    v = {};
    v.kind = protoField(f).kind;
    if (f == proto_field_t::name)
      strcpy(v.text, originalName);
    else
      v.integer = originalOrder;
    return true;
  }
  ApplyResult apply(model_edit_type_t t, JsonDocument &d, Authorization a,
                    uint64_t r) override {
    if (late || !auth.allowed || a.generation != auth.generation || r != rev)
      return {edit_result_t::error("stale"), Admission::stale};
    if (busy)
      return {{}, Admission::busy};
    ++applies;
    receivedType = t;
    received.set(d);
    ++rev;
    return {edit_result_t::completed(9, {outcome, ModelCommitStage::none})};
  }
  CommitResult retryFileCommit(Authorization a, uint64_t r) override {
    if (!auth.allowed || r != rev || a.generation != auth.generation)
      return {};
    if (busy)
      return {{}, Admission::busy};
    ++commits;
    return {{outcome, ModelCommitStage::none}};
  }
  void activity() override {}
};
void setUp() {}
void tearDown() {}
static void turn(Form &f, unsigned count) {
  while (count--)
    f.turn(1, 1);
}
static void editOrder(Form &f) {
  turn(f, 1);
  f.activate(1);
  f.turn(1, 1);
  f.activate(1); // value -> action list
  f.turn(-1, 1);
  f.activate(1); // explicit Apply (default Cancel)
}
static void room_rotation_apply_and_save_are_separate() {
  Memory memory;
  Fake backend;
  Form form(backend, memory);
  TEST_ASSERT_TRUE(form.open(Kind::room, 4, 0));
  editOrder(form);
  TEST_ASSERT_TRUE(form.view().dirty);
  TEST_ASSERT_EQUAL(0, backend.applies);
  turn(form, 1);
  form.activate(1); // Save
  TEST_ASSERT_EQUAL(1, backend.applies);
  TEST_ASSERT_EQUAL(model_edit_type_t::save_room, backend.receivedType);
  TEST_ASSERT_EQUAL(4, backend.received["roomId"].as<int>());
  TEST_ASSERT_EQUAL(1, backend.received["sortOrder"].as<int>());
  TEST_ASSERT_TRUE(backend.received["name"].isNull());
  TEST_ASSERT_EQUAL(Mode::result, form.view().mode);
  TEST_ASSERT_EQUAL(Notice::saved, form.view().notice);
  TEST_ASSERT_EQUAL(0, memory.active);
}
static void applied_add_failure_adopts_id_and_only_retries_commit() {
  Memory memory;
  Fake backend;
  backend.outcome = ModelCommitState::failed;
  Form form(backend, memory);
  TEST_ASSERT_TRUE(form.open(Kind::room, 0, 0));
  editOrder(form);
  turn(form, 1);
  form.activate(1);
  TEST_ASSERT_EQUAL(1, backend.applies);
  TEST_ASSERT_EQUAL(9, form.view().id);
  TEST_ASSERT_EQUAL(Notice::appliedNotSaved, form.view().notice);
  TEST_ASSERT_EQUAL(0, memory.active);
  form.turn(1, 1);
  form.activate(1); // retry, default Close
  TEST_ASSERT_EQUAL(1, backend.commits);
  TEST_ASSERT_EQUAL(1, backend.applies);
  backend.outcome = ModelCommitState::saved;
  form.activate(1);
  TEST_ASSERT_EQUAL(2, backend.commits);
  TEST_ASSERT_EQUAL(1, backend.applies);
  TEST_ASSERT_EQUAL(Notice::saved, form.view().notice);
}
static void
deletion_defaults_cancel_and_requires_explicit_identity_confirmation() {
  Memory memory;
  Fake backend;
  Form form(backend, memory);
  form.open(Kind::room, 4, 0);
  turn(form, 4);
  form.activate(1);
  TEST_ASSERT_EQUAL(Mode::confirm, form.view().mode);
  TEST_ASSERT_NOT_NULL(strstr(form.view().hint, "4"));
  form.activate(1);
  TEST_ASSERT_EQUAL(0, backend.applies); // Cancel default
  form.activate(1);
  form.turn(1, 1);
  form.activate(1);
  TEST_ASSERT_EQUAL(1, backend.applies);
  TEST_ASSERT_EQUAL(model_edit_type_t::delete_room, backend.receivedType);
}
static void stale_auth_and_memory_failure_wipe_every_draft() {
  for (unsigned mode = 0; mode < 4; mode++) {
    Memory memory;
    Fake backend;
    Form form(backend, memory);
    form.open(Kind::room, 4, 0);
    editOrder(form);
    if (mode == 0)
      ++backend.rev;
    else if (mode == 1)
      backend.auth.allowed = false;
    else if (mode == 2)
      ++backend.auth.configurationGeneration;
    else {
      form.discard();
    }
    form.tick(2);
    TEST_ASSERT_TRUE(form.closed());
    TEST_ASSERT_EQUAL(0, memory.active);
    TEST_ASSERT_EQUAL(0, backend.applies);
  }
  Memory memory;
  Fake backend;
  Form form(backend, memory);
  form.open(Kind::room, 4, 0);
  memory.fail = true;
  editOrder(form);
  TEST_ASSERT_TRUE(form.closed());
  TEST_ASSERT_EQUAL(Notice::noMemory, form.view().notice);
  TEST_ASSERT_EQUAL(0, backend.applies);
}
static void empty_save_and_late_stale_do_not_mutate() {
  Memory memory;
  Fake backend;
  Form form(backend, memory);
  form.open(Kind::room, 4, 0);
  turn(form, 2);
  form.activate(1);
  TEST_ASSERT_TRUE(form.closed());
  TEST_ASSERT_EQUAL(Notice::unchanged, form.view().notice);
  TEST_ASSERT_EQUAL(0, backend.applies);
  form.open(Kind::room, 4, 0);
  editOrder(form);
  backend.late = true;
  turn(form, 1);
  form.activate(1);
  TEST_ASSERT_EQUAL(0, backend.applies);
  TEST_ASSERT_EQUAL(Notice::changed, form.view().notice);
}
static void invalid_original_never_normalizes_and_busy_retains_draft() {
  for (int order : {-129, 128}) {
    Memory memory;
    Fake backend;
    backend.originalOrder = order;
    Form form(backend, memory);
    TEST_ASSERT_TRUE(form.open(Kind::room, 4, 0));
    TEST_ASSERT_NOT_NULL(strstr(form.view().rows[1].value, "Invalid"));
    form.turn(1, 1);
    form.activate(1);
    form.activate(1);
    TEST_ASSERT_EQUAL(Mode::fields, form.view().mode);
    TEST_ASSERT_EQUAL(Notice::invalid, form.view().notice);
    TEST_ASSERT_FALSE(form.view().dirty);
    TEST_ASSERT_EQUAL(0, backend.applies);
  }
  Memory memory;
  Fake backend;
  Form form(backend, memory);
  form.open(Kind::room, 4, 0);
  editOrder(form);
  backend.busy = true;
  turn(form, 1);
  form.activate(1);
  TEST_ASSERT_EQUAL(Notice::busy, form.view().notice);
  TEST_ASSERT_TRUE(form.view().dirty);
  TEST_ASSERT_EQUAL(0, backend.applies);
  backend.busy = false;
  form.activate(1);
  TEST_ASSERT_EQUAL(1, backend.applies);
  TEST_ASSERT_EQUAL(0, memory.active);
}
static void timeout_and_out_of_band_discard_wipe_active_editor_and_draft() {
  for (unsigned mode = 0; mode < 3; mode++) {
    Memory memory;
    Fake backend;
    Form form(backend, memory);
    form.open(Kind::room, 4, 0);
    editOrder(form);
    form.activate(1); // Dirty field: enter draft menu, then explicitly Edit.
    form.turn(1, 1); form.activate(1);
    TEST_ASSERT_EQUAL(Mode::adjust, form.view().mode);
    if (mode == 0)
      form.tick(30001);
    else if (mode == 1) {
      backend.auth.allowed = false;
      form.discard();
      form.discard();
    } else
      form.tick(0);
    TEST_ASSERT_TRUE(form.closed());
    TEST_ASSERT_EQUAL(0, memory.active);
    TEST_ASSERT_EQUAL(0, backend.applies);
  }
}
static void
add_order_128_requires_explicit_replacement_without_losing_name_draft() {
  Memory memory;
  Fake backend;
  backend.originalOrder = 128;
  Form form(backend, memory);
  TEST_ASSERT_TRUE(form.open(Kind::room, 0, 0));
  form.activate(1);
  form.activate(1);
  turn(form, 97);
  form.activate(1); // append space; Apply name
  turn(form, 2);
  form.activate(1); // name-only Save must not wrap offered order
  TEST_ASSERT_EQUAL(0, backend.applies);
  TEST_ASSERT_EQUAL(Mode::fields, form.view().mode);
  TEST_ASSERT_EQUAL(1, form.view().selected);
  TEST_ASSERT_TRUE(form.view().dirty);
  form.activate(1);
  TEST_ASSERT_EQUAL(Mode::replace, form.view().mode);
  form.activate(1); // Cancel is default
  TEST_ASSERT_EQUAL(Mode::fields, form.view().mode);
  TEST_ASSERT_TRUE(form.view().dirty);
  form.activate(1);
  form.turn(1, 1);
  form.activate(1); // explicit Replace
  TEST_ASSERT_EQUAL(Mode::adjust, form.view().mode);
  form.back(1); // field cancellation still preserves original and name draft
  TEST_ASSERT_NOT_NULL(strstr(form.view().rows[1].value, "Invalid"));
  form.activate(1);
  form.turn(1, 1);
  form.activate(1);
  form.turn(1, 1);
  form.activate(1);
  form.turn(-1, 1);
  form.activate(1);
  TEST_ASSERT_EQUAL(0, backend.applies);
  turn(form, 1);
  form.activate(1);
  TEST_ASSERT_EQUAL(1, backend.applies);
  TEST_ASSERT_EQUAL(1, backend.received["sortOrder"].as<int>());
  TEST_ASSERT_EQUAL_STRING("Room ",
                           backend.received["name"].as<const char *>());
}
static void malformed_utf8_name_has_separate_cancel_or_replace_flow() {
  Memory memory;
  Fake backend;
  backend.originalName = "\xff";
  Form form(backend, memory);
  TEST_ASSERT_TRUE(form.open(Kind::room, 4, 0));
  editOrder(form);
  form.turn(-1, 1);
  form.activate(1);
  TEST_ASSERT_EQUAL(Mode::replace, form.view().mode);
  TEST_ASSERT_TRUE(form.view().dirty);
  form.activate(1);
  TEST_ASSERT_EQUAL(Mode::fields, form.view().mode);
  TEST_ASSERT_EQUAL_STRING("Invalid UTF-8", form.view().rows[0].value);
  form.activate(1);
  form.turn(1, 1);
  form.activate(1);
  TEST_ASSERT_EQUAL(Mode::text, form.view().mode);
  TEST_ASSERT_EQUAL_STRING("", form.view().rows[0].value);
  turn(form, 33);
  form.activate(1);
  turn(form, 64);
  form.activate(1); // A, then explicit Apply
  TEST_ASSERT_EQUAL(0, backend.applies);
  turn(form, 2);
  form.activate(1);
  TEST_ASSERT_EQUAL(1, backend.applies);
  TEST_ASSERT_EQUAL_STRING("A", backend.received["name"].as<const char *>());
  TEST_ASSERT_EQUAL(1, backend.received["sortOrder"].as<int>());
}
static void
unrelated_edit_preserves_malformed_legacy_name_without_replacement() {
  Memory memory;
  Fake backend;
  backend.originalName = "\xff";
  Form form(backend, memory);
  TEST_ASSERT_TRUE(form.open(Kind::room, 4, 0));
  editOrder(form);
  form.turn(1, 1);
  form.activate(1);
  TEST_ASSERT_EQUAL(1, backend.applies);
  TEST_ASSERT_TRUE(backend.received["name"].isNull());
  TEST_ASSERT_EQUAL(1, backend.received["sortOrder"].as<int>());
  TEST_ASSERT_EQUAL_UINT8(255,
                          static_cast<unsigned char>(backend.originalName[0]));
}
struct ChoiceBackend : Fake {
  bool missingChoice = false;
  Admission preflight = Admission::accepted;
  uint8_t choiceCount(Kind, uint8_t, proto_field_t field) override {
    return field == proto_field_t::sortOrder ? 2 : 0;
  }
  bool choiceAt(Kind, uint8_t, proto_field_t field, uint8_t index, Choice &out) override {
    if (field != proto_field_t::sortOrder || index > 1 || missingChoice) return false;
    out = {}; out.value = index ? 7 : 0;
    strcpy(out.label, index ? "Office #7" : "Unassigned");
    return true;
  }
  ApplyResult apply(model_edit_type_t t, JsonDocument &d, Authorization a, uint64_t r) override {
    if (preflight != Admission::accepted)
      return {{}, preflight, proto_field_t::sortOrder};
    return Fake::apply(t, d, a, r);
  }
};
static void typed_choice_displays_label_and_saves_value_instead_of_index() {
  Memory memory; ChoiceBackend backend; Form form(backend, memory);
  TEST_ASSERT_TRUE(form.open(Kind::room, 4, 0));
  TEST_ASSERT_EQUAL_STRING("Unassigned", form.view().rows[1].value);
  form.turn(1, 1); form.activate(1); form.turn(1, 1);
  TEST_ASSERT_EQUAL_STRING("Office #7", form.view().rows[0].value);
  form.activate(1);
  TEST_ASSERT_EQUAL(2, form.view().total); // Choices have no numeric Change step.
  TEST_ASSERT_EQUAL(1, form.view().selected); // Cancel by default.
  form.turn(-1, 1); form.activate(1);
  TEST_ASSERT_EQUAL_STRING("Office #7", form.view().rows[1].value);
  TEST_ASSERT_EQUAL(0, backend.applies);
  form.turn(1, 1); form.activate(1);
  TEST_ASSERT_EQUAL(1, backend.applies);
  TEST_ASSERT_EQUAL(7, backend.received["sortOrder"].as<int>());
}
static void missing_choice_and_invalid_preflight_never_drop_or_apply_drafts() {
  Memory memory; ChoiceBackend backend; Form form(backend, memory);
  form.open(Kind::room, 4, 0); editOrder(form);
  backend.preflight = Admission::invalid;
  form.turn(1, 1); form.activate(1);
  TEST_ASSERT_EQUAL(Mode::validation, form.view().mode);
  TEST_ASSERT_EQUAL(Notice::invalid, form.view().notice);
  TEST_ASSERT_TRUE(form.view().dirty);
  TEST_ASSERT_EQUAL(0, backend.applies);
  form.activate(1); // Revise, never replay Save.
  TEST_ASSERT_EQUAL(Mode::fields, form.view().mode);
  TEST_ASSERT_EQUAL(1, form.view().selected);
  backend.preflight = Admission::accepted;
  backend.missingChoice = true;
  form.activate(1); // Dirty field menu, then explicitly Edit.
  form.turn(1, 1); form.activate(1);
  TEST_ASSERT_TRUE(form.closed());
  TEST_ASSERT_EQUAL(Notice::changed, form.view().notice);
  TEST_ASSERT_EQUAL(0, memory.active);
  TEST_ASSERT_EQUAL(0, backend.applies);
}
static void no_memory_preflight_wipes_draft_without_edit() {
  Memory memory; ChoiceBackend backend; Form form(backend, memory);
  form.open(Kind::room, 4, 0); editOrder(form);
  backend.preflight = Admission::noMemory;
  form.turn(1, 1); form.activate(1);
  TEST_ASSERT_TRUE(form.closed());
  TEST_ASSERT_EQUAL(Notice::noMemory, form.view().notice);
  TEST_ASSERT_EQUAL(0, memory.active);
  TEST_ASSERT_EQUAL(0, backend.applies);
}
static void changed_choice_must_resolve_again_before_final_save() {
  Memory memory; ChoiceBackend backend; Form form(backend, memory);
  form.open(Kind::room, 4, 0); editOrder(form);
  form.turn(1, 1); // Save is already selected and rendered.
  backend.missingChoice = true;
  form.activate(1);
  TEST_ASSERT_TRUE(form.closed());
  TEST_ASSERT_EQUAL(Notice::changed, form.view().notice);
  TEST_ASSERT_EQUAL(0, backend.applies);
  TEST_ASSERT_EQUAL(0, memory.active);
}
struct DeviceBackend : Fake {
  int protocol = 0;
  Kind preparedKind = Kind::shade;
  Admission preflight = Admission::accepted;
  proto_field_t refusedField = proto_field_t::remoteAddress;
  bool malformedReason = false;
  bool prepare(Kind kind, uint8_t id, uint8_t &offered) override {
    preparedKind = kind; offered = id ? id : 9; return true;
  }
  bool read(Kind, uint8_t, proto_field_t field, Value &v) override {
    v = {}; v.kind = protoField(field).kind;
    if (v.kind == 's') strcpy(v.text, "Office");
    else if (field == proto_field_t::proto) v.integer = protocol;
    else if (field == proto_field_t::remoteAddress) v.integer = 0x123456;
    else if (field == proto_field_t::bitLength) v.integer = 56;
    else if (field == proto_field_t::upTime || field == proto_field_t::downTime || field == proto_field_t::tiltTime) v.integer = 10000;
    else if (field == proto_field_t::stepSize) v.integer = 100;
    else if (field == proto_field_t::repeats) v.integer = 1;
    else if (field == proto_field_t::gpioUp || field == proto_field_t::gpioDown || field == proto_field_t::gpioMy) v.integer = 255;
    return true;
  }
  ApplyResult apply(model_edit_type_t type, JsonDocument &d, Authorization a, uint64_t r) override {
    if (preflight != Admission::accepted) {
      ApplyResult result; result.admission = preflight; result.field = refusedField;
      if (malformedReason) memset(result.reason, 'X', sizeof(result.reason));
      else strcpy(result.reason, "Remote address is already in use.");
      return result;
    }
    return Fake::apply(type, d, a, r);
  }
};
static void selectField(Form &form, uint8_t index) {
  TEST_ASSERT_EQUAL(Mode::fields, form.view().mode);
  while (form.view().selected < index) form.turn(1, 1);
  while (form.view().selected > index) form.turn(-1, 1);
}
static void adjustField(Form &form, uint8_t index, int turns) {
  selectField(form, index); form.activate(1);
  TEST_ASSERT_EQUAL(Mode::adjust, form.view().mode);
  for (int i = 0; i < abs(turns); i++) form.turn(turns > 0 ? 1 : -1, 1);
  form.activate(1); form.turn(-1, 1); form.activate(1);
  TEST_ASSERT_EQUAL(Mode::fields, form.view().mode);
}
static void device_and_group_saves_require_separate_default_cancel_confirmation() {
  for (const auto kind : {Kind::shade, Kind::group}) {
    Memory memory; DeviceBackend backend; Form form(backend, memory);
    TEST_ASSERT_TRUE(form.open(kind, 4, 0));
    const uint8_t protoIndex = kind == Kind::shade ? 6 : 4;
    const uint8_t count = kind == Kind::shade ? 21 : 7;
    adjustField(form, protoIndex, 1);
    TEST_ASSERT_EQUAL(0, backend.applies);
    selectField(form, count); form.activate(1);
    TEST_ASSERT_EQUAL(Mode::confirm, form.view().mode);
    TEST_ASSERT_EQUAL(0, form.view().selected);
    TEST_ASSERT_EQUAL(0, backend.applies);
    form.activate(1);
    TEST_ASSERT_EQUAL(Mode::fields, form.view().mode);
    TEST_ASSERT_EQUAL(count, form.view().selected);
    TEST_ASSERT_TRUE(form.view().dirty);
    form.activate(1); form.turn(1, 1); form.activate(1);
    TEST_ASSERT_EQUAL(1, backend.applies);
    TEST_ASSERT_EQUAL(1, backend.received["proto"].as<int>());
    TEST_ASSERT_EQUAL(4, backend.received[kind == Kind::shade ? "shadeId" : "groupId"].as<int>());
  }
}
static void dirty_field_revert_is_explicit_and_preserves_other_drafts() {
  Memory memory; DeviceBackend backend; Form form(backend, memory);
  form.open(Kind::shade, 4, 0);
  adjustField(form, 7, 1); // Up time, retain this separate draft.
  adjustField(form, 6, 1); // Protocol RTS -> RTW.
  form.activate(1);
  TEST_ASSERT_EQUAL(Mode::draft, form.view().mode);
  TEST_ASSERT_EQUAL(0, form.view().selected);
  form.activate(1); // Default Cancel retains both drafts.
  TEST_ASSERT_TRUE(form.view().dirty);
  form.activate(1); form.turn(1, 1); form.activate(1); // Edit same draft.
  TEST_ASSERT_EQUAL(Mode::adjust, form.view().mode);
  TEST_ASSERT_EQUAL_STRING("RTW", form.view().rows[0].value);
  form.back(1);
  form.activate(1); form.turn(1, 1); form.turn(1, 1); form.activate(1); // Revert only protocol.
  TEST_ASSERT_EQUAL(Mode::fields, form.view().mode);
  TEST_ASSERT_EQUAL_STRING("RTS", form.view().rows[0].value); // Protocol is first row of page6.
  TEST_ASSERT_TRUE(form.view().dirty);
  selectField(form, 21); form.activate(1); form.turn(1, 1); form.activate(1);
  TEST_ASSERT_EQUAL(1, backend.applies);
  TEST_ASSERT_TRUE(backend.received["proto"].isNull());
  TEST_ASSERT_EQUAL(10001, backend.received["upTime"].as<int>());
}
static void inactive_gpio_draft_requires_revert_or_protocol_change_without_silent_drop() {
  Memory memory; DeviceBackend backend; backend.protocol = 9;
  Form form(backend, memory); form.open(Kind::shade, 4, 0);
  adjustField(form, 17, -1); // GPIO draft only; no output.
  adjustField(form, 7, 1);
  adjustField(form, 6, -4); // GPIO remote -> RTS makes GPIO draft inactive.
  selectField(form, 21); form.activate(1); form.turn(1, 1); form.activate(1);
  TEST_ASSERT_EQUAL(Mode::validation, form.view().mode);
  TEST_ASSERT_NOT_NULL(strstr(form.view().hint, "Revert"));
  TEST_ASSERT_EQUAL(0, backend.applies);
  TEST_ASSERT_TRUE(form.view().dirty);
  form.activate(1); // Revise exact offending field.
  TEST_ASSERT_EQUAL(17, form.view().selected);
  form.activate(1);
  TEST_ASSERT_EQUAL(Mode::draft, form.view().mode);
  TEST_ASSERT_EQUAL(2, form.view().total); // Cancel or Revert; no editing an inactive field.
  TEST_ASSERT_EQUAL(0, form.view().selected);
  form.turn(1, 1); form.activate(1);
  selectField(form, 21); form.activate(1); form.turn(1, 1); form.activate(1);
  TEST_ASSERT_EQUAL(1, backend.applies);
  TEST_ASSERT_TRUE(backend.received["gpioUp"].isNull());
  TEST_ASSERT_EQUAL(0, backend.received["proto"].as<int>());
  TEST_ASSERT_EQUAL(10001, backend.received["upTime"].as<int>());
}
static void inactive_field_is_readable_and_revision_returns_to_protocol_without_edit() {
  Memory memory; DeviceBackend backend; Form form(backend, memory);
  form.open(Kind::shade, 4, 0); selectField(form, 17);
  TEST_ASSERT_NOT_NULL(strstr(form.view().rows[2].label, "Inactive"));
  form.activate(1);
  TEST_ASSERT_EQUAL(Mode::validation, form.view().mode);
  TEST_ASSERT_FALSE(form.view().dirty);
  form.activate(1);
  TEST_ASSERT_EQUAL(Mode::fields, form.view().mode);
  TEST_ASSERT_EQUAL(6, form.view().selected);
  TEST_ASSERT_EQUAL(0, backend.applies);
}
static void correctable_refusal_shows_owned_reason_and_never_retries_on_acknowledgment() {
  Memory memory; DeviceBackend backend; Form form(backend, memory);
  form.open(Kind::shade, 4, 0); adjustField(form, 6, 1);
  backend.preflight = Admission::invalid;
  selectField(form, 21); form.activate(1); form.turn(1, 1); form.activate(1);
  TEST_ASSERT_EQUAL(Mode::validation, form.view().mode);
  TEST_ASSERT_EQUAL_STRING("Remote address is already in use.", form.view().hint);
  TEST_ASSERT_EQUAL(0, form.view().selected);
  TEST_ASSERT_TRUE(form.view().dirty);
  form.activate(1);
  TEST_ASSERT_EQUAL(Mode::fields, form.view().mode);
  TEST_ASSERT_EQUAL(2, form.view().selected);
  TEST_ASSERT_EQUAL(0, backend.applies);
  selectField(form, 21); form.activate(1); form.turn(1, 1); form.activate(1);
  form.turn(1, 1); form.activate(1); // Cancel form explicitly wipes.
  TEST_ASSERT_TRUE(form.closed());
  TEST_ASSERT_EQUAL(0, memory.active);
}
static void malformed_refusal_and_expired_validation_wipe_without_mutation() {
  for (unsigned scenario = 0; scenario < 3; scenario++) {
    Memory memory; DeviceBackend backend; Form form(backend, memory);
    form.open(Kind::shade, 4, 0); adjustField(form, 6, 1);
    backend.preflight = Admission::invalid; backend.malformedReason = scenario == 0;
    selectField(form, 21); form.activate(1); form.turn(1, 1); form.activate(1);
    if (scenario == 1) { backend.auth.allowed = false; form.tick(2); }
    if (scenario == 2) form.tick(120001);
    TEST_ASSERT_TRUE(form.closed());
    TEST_ASSERT_EQUAL(0, memory.active);
    TEST_ASSERT_EQUAL(0, backend.applies);
  }
}
int main() {
  UNITY_BEGIN();
  RUN_TEST(changed_choice_must_resolve_again_before_final_save);
  RUN_TEST(dirty_field_revert_is_explicit_and_preserves_other_drafts);
  RUN_TEST(inactive_gpio_draft_requires_revert_or_protocol_change_without_silent_drop);
  RUN_TEST(inactive_field_is_readable_and_revision_returns_to_protocol_without_edit);
  RUN_TEST(correctable_refusal_shows_owned_reason_and_never_retries_on_acknowledgment);
  RUN_TEST(malformed_refusal_and_expired_validation_wipe_without_mutation);
  RUN_TEST(device_and_group_saves_require_separate_default_cancel_confirmation);
  RUN_TEST(typed_choice_displays_label_and_saves_value_instead_of_index);
  RUN_TEST(missing_choice_and_invalid_preflight_never_drop_or_apply_drafts);
  RUN_TEST(no_memory_preflight_wipes_draft_without_edit);
  RUN_TEST(unrelated_edit_preserves_malformed_legacy_name_without_replacement);
  RUN_TEST(
      add_order_128_requires_explicit_replacement_without_losing_name_draft);
  RUN_TEST(malformed_utf8_name_has_separate_cancel_or_replace_flow);
  RUN_TEST(invalid_original_never_normalizes_and_busy_retains_draft);
  RUN_TEST(timeout_and_out_of_band_discard_wipe_active_editor_and_draft);
  RUN_TEST(room_rotation_apply_and_save_are_separate);
  RUN_TEST(applied_add_failure_adopts_id_and_only_retries_commit);
  RUN_TEST(
      deletion_defaults_cancel_and_requires_explicit_identity_confirmation);
  RUN_TEST(stale_auth_and_memory_failure_wipe_every_draft);
  RUN_TEST(empty_save_and_late_stale_do_not_mutate);
  return UNITY_END();
}
