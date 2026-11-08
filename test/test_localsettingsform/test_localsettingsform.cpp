#include "LocalSettingsForm.h"
#include "SettingsRevisions.h"
#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <unity.h>
using namespace LocalUi::Settings;
struct Memory : ArduinoJson::Allocator {
  bool fail = false, failedAdmission = false;
  unsigned active = 0, allocations = 0, failAfter = ~0u;
  void *allocate(size_t n) override {
    if (fail || ++allocations > failAfter) {
      failedAdmission = true;
      return nullptr;
    }
    void *p = malloc(n);
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
    if (fail)
      return nullptr;
    return realloc(p, n);
  }
};
struct Fake : Backend {
  Authorization auth{true, 1, 1};
  SettingsRevisions revisions;
  bool rejectLate = false, ethernet = false, fixedPins = true;
  proto_field_t overrideField = proto_field_t::count;
  int32_t overrideInteger = 0;
  float overrideNumber = 0;
  unsigned saves = 0, activities = 0;
  settings_save_state_t outcome = settings_save_state_t::saved;
  JsonDocument applied;
  Authorization authorization() override { return auth; }
  uint64_t revision(settings_section_t s) override { return revisions.get(s); }
  bool hasEthernet() const override { return ethernet; }
  bool fixedRadioPins() const override { return fixedPins; }
  void activity() override { ++activities; }
  bool read(settings_section_t s, SettingsSnapshotSink &sink) override {
    for (unsigned i = 0; i < settingsFieldCount(s, ethernet); ++i) {
      const auto &m = *settingsFieldAt(s, i, ethernet);
      settings_value_t v;
      v.kind = m.schema().kind;
      if (m.secret()) {
        strcpy(v.text, "STORED-SECRET-SENTINEL");
        sink.field(m.field, v);
        v.kind = 'b';
        v.boolean = true;
        sink.field(m.presenceField(), v);
        continue;
      }
      v.integer = m.lo;
      v.number = float(m.lo) / 100;
      if (m.field == proto_field_t::hostname)
        strcpy(v.text, "old-host");
      if (m.field == proto_field_t::ssid)
        strcpy(v.text, "wifi");
      if (m.field == proto_field_t::port)
        v.integer = 1883;
      if (m.field == proto_field_t::frequency)
        v.number = 433.421;
      if (m.field == overrideField) {
        v.integer = overrideInteger;
        v.number = overrideNumber;
      }
      sink.field(m.field, v);
    }
    return true;
  }
  settings_apply_result_t apply(settings_section_t section, JsonObject request,
                                Authorization expected, uint64_t rev) override {
    if (!auth.allowed || expected.generation != auth.generation)
      return settings_apply_result_t::refused(
          settings_apply_refusal_t::unauthorized);
    if (rejectLate || rev != revision(section) ||
        expected.configurationGeneration != auth.configurationGeneration)
      return settings_apply_result_t::refused(settings_apply_refusal_t::stale);
    ++saves;
    applied.set(request);
    settings_apply_result_t r;
    r.persistence.state = outcome;
    return r;
  }
};
void setUp() {}
void tearDown() {}
static void turnTo(Form &f, unsigned n, uint64_t t = 1) {
  while (n--)
    f.turn(1, t);
}
static void chooseNumericApply(Form &f) {
  f.activate(1);
  turnTo(f, 2, 1);
  f.activate(1);
}
static void rotation_and_field_apply_do_not_persist_until_explicit_save() {
  Memory mem;
  Fake backend;
  Form form(backend, mem);
  TEST_ASSERT_TRUE(form.openSection(settings_section_t::general, 0));
  turnTo(form, 1);
  form.activate(1);
  form.turn(1, 1);
  TEST_ASSERT_EQUAL(0, backend.saves);
  chooseNumericApply(form);
  TEST_ASSERT_EQUAL(Mode::fields, form.view().mode);
  TEST_ASSERT_TRUE(form.view().dirty);
  TEST_ASSERT_EQUAL(0, backend.saves);
  turnTo(form, 3);
  form.activate(1);
  TEST_ASSERT_EQUAL(1, backend.saves);
  TEST_ASSERT_TRUE(form.closed());
  TEST_ASSERT_EQUAL(Notice::saved, form.view().notice);
  TEST_ASSERT_TRUE(backend.applied["ssdpBroadcast"].as<bool>());
  TEST_ASSERT_EQUAL(1, backend.applied.size());
}
static void cancel_auth_revision_and_timeout_close_and_release_drafts() {
  Memory mem;
  Fake backend;
  Form form(backend, mem);
  form.openSection(settings_section_t::general, 0);
  turnTo(form, 1);
  form.activate(1);
  form.turn(1, 1);
  chooseNumericApply(form);
  TEST_ASSERT_GREATER_THAN(0, mem.active);
  backend.revisions.completed(settings_section_t::general);
  form.tick(8);
  TEST_ASSERT_TRUE(form.closed());
  TEST_ASSERT_EQUAL(Notice::changed, form.view().notice);
  TEST_ASSERT_EQUAL(0, mem.active);
  TEST_ASSERT_EQUAL(0, backend.saves);
  form.open(10);
  backend.auth.allowed = false;
  form.tick(11);
  TEST_ASSERT_TRUE(form.closed());
  backend.auth.allowed = true;
  form.open(20);
  form.tick(20 + Form::timeoutMs);
  TEST_ASSERT_EQUAL(Notice::expired, form.view().notice);
}
static void risky_save_defaults_to_cancel_and_partial_retains_explicit_retry() {
  Memory mem;
  Fake backend;
  Form form(backend, mem);
  form.openSection(settings_section_t::security, 0);
  form.activate(1);
  form.turn(1, 1);
  chooseNumericApply(form);
  turnTo(form, 4);
  form.activate(1);
  TEST_ASSERT_EQUAL(Mode::confirm, form.view().mode);
  form.activate(1);
  TEST_ASSERT_EQUAL(0, backend.saves);
  form.activate(1);
  form.turn(1, 1);
  backend.outcome = settings_save_state_t::partial;
  form.activate(1);
  TEST_ASSERT_EQUAL(1, backend.saves);
  TEST_ASSERT_FALSE(form.closed());
  TEST_ASSERT_EQUAL(Notice::partial, form.view().notice);
  TEST_ASSERT_TRUE(form.view().dirty);
  form.back(1);
  TEST_ASSERT_TRUE(form.closed());
  TEST_ASSERT_EQUAL(0, mem.active);
}
static void
secret_views_expose_presence_only_and_keep_does_not_copy_stored_secret() {
  Memory mem;
  Fake backend;
  Form form(backend, mem);
  form.openSection(settings_section_t::network, 0);
  turnTo(form, 2);
  TEST_ASSERT_EQUAL_STRING("Set", form.view().rows[2].value);
  form.activate(1);
  TEST_ASSERT_EQUAL(Mode::secret, form.view().mode);
  TEST_ASSERT_EQUAL_STRING("Keep", form.view().rows[0].label);
  form.activate(1);
  TEST_ASSERT_FALSE(form.view().dirty);
  TEST_ASSERT_EQUAL(0, mem.active);
  form.activate(1);
  form.turn(1, 1);
  form.activate(1);
  TEST_ASSERT_EQUAL(Mode::text, form.view().mode);
  TEST_ASSERT_EQUAL_STRING("", form.view().rows[0].value);
  TEST_ASSERT_TRUE(form.view().protectedFrame);
  form.back(1);
  TEST_ASSERT_EQUAL(0, backend.saves);
}
static void readonly_pins_and_no_ethernet_connection_never_open_editor() {
  Memory mem;
  Fake backend;
  Form form(backend, mem);
  form.openSection(settings_section_t::network, 0);
  form.activate(1);
  TEST_ASSERT_EQUAL(Mode::fields, form.view().mode);
  TEST_ASSERT_EQUAL(Notice::readOnly, form.view().notice);
  form.openSection(settings_section_t::radio, 0);
  turnTo(form, 3);
  form.activate(1);
  TEST_ASSERT_EQUAL(Notice::readOnly, form.view().notice);
}
static void allocation_failure_closes_and_never_applies() {
  Memory mem;
  Fake backend;
  Form form(backend, mem);
  form.openSection(settings_section_t::general, 0);
  turnTo(form, 1);
  form.activate(1);
  form.turn(1, 1);
  mem.fail = true;
  chooseNumericApply(form);
  TEST_ASSERT_TRUE(form.closed());
  TEST_ASSERT_EQUAL(Notice::noMemory, form.view().notice);
  TEST_ASSERT_EQUAL(0, backend.saves);
  TEST_ASSERT_EQUAL(0, mem.active);
}
static void pickerSelect(Form &f, unsigned &selected, unsigned next) {
  while (selected < next) {
    f.turn(1, 1);
    ++selected;
  }
  while (selected > next) {
    f.turn(-1, 1);
    --selected;
  }
}
static void textApply(Form &f, const char *text) {
  unsigned selected = 0;
  for (const char *p = text; *p; p++) {
    pickerSelect(f, selected, unsigned(*p) - 32);
    f.activate(1);
  }
  pickerSelect(f, selected, 97);
  f.activate(1);
}
static void
secret_replace_is_changed_only_and_never_echoes_stored_or_fresh_bytes() {
  Memory mem;
  Fake backend;
  Form form(backend, mem);
  form.openSection(settings_section_t::network, 0);
  turnTo(form, 2);
  form.activate(1);
  form.turn(1, 1);
  form.activate(1);
  unsigned picker = 0;
  pickerSelect(form, picker, 'X' - 32);
  form.activate(1);
  TEST_ASSERT_EQUAL_STRING("*", form.view().rows[0].value);
  pickerSelect(form, picker, 97);
  form.activate(1);
  TEST_ASSERT_TRUE(form.view().dirty);
  TEST_ASSERT_EQUAL_STRING("Replace pending", form.view().rows[2].value);
  turnTo(form, settingsFieldCount(settings_section_t::network, false) - 2);
  form.activate(1);
  form.turn(1, 1);
  form.activate(1);
  TEST_ASSERT_EQUAL(1, backend.saves);
  TEST_ASSERT_EQUAL(1, backend.applied.size());
  TEST_ASSERT_EQUAL_STRING("X",
                           backend.applied["passphrase"].as<const char *>());
  TEST_ASSERT_NULL(backend.applied["password"].as<const char *>());
}
static void
invalid_ip_is_refused_by_shared_validator_without_polluting_draft() {
  Memory mem;
  Fake backend;
  Form form(backend, mem);
  form.openSection(settings_section_t::network, 0);
  turnTo(form, 6);
  form.activate(1);
  textApply(form, "999.1.1.1");
  TEST_ASSERT_EQUAL(Notice::invalid, form.view().notice);
  TEST_ASSERT_FALSE(form.view().dirty);
  TEST_ASSERT_EQUAL(0, backend.saves);
  form.activate(1);
  textApply(form, "192.168.1.2");
  TEST_ASSERT_TRUE(form.view().dirty);
  form.back(1);
  TEST_ASSERT_TRUE(form.closed());
  TEST_ASSERT_EQUAL(0, mem.active);
}
static void
numeric_coarse_steps_reach_bounds_and_frequency_preserves_third_decimal() {
  Memory mem;
  Fake backend;
  Form form(backend, mem);
  form.openSection(settings_section_t::mqtt, 0);
  turnTo(form, 4);
  form.activate(1);
  form.activate(1);
  form.turn(1, 1);
  for (unsigned i = 0; i < 4; i++)
    form.activate(1); // 10,100,1000,10000
  form.turn(-1, 1);
  form.activate(1); // Edit value
  for (unsigned i = 0; i < 7; i++)
    form.turn(1, 1); // upper bound with seven detents
  chooseNumericApply(form);
  turnTo(form, 5);
  form.activate(1);
  form.turn(1, 1);
  form.activate(1);
  TEST_ASSERT_EQUAL(65535, backend.applied["port"].as<int>());
  form.openSection(settings_section_t::radio, 0);
  turnTo(form, 9);
  form.activate(1);
  form.turn(1, 1);
  chooseNumericApply(form);
  turnTo(form, 5);
  form.activate(1);
  form.turn(1, 1);
  form.activate(1);
  TEST_ASSERT_FLOAT_WITHIN(0.00002, 433.422,
                           backend.applied["frequency"].as<float>());
}
static void late_backend_recheck_and_failed_persistence_never_claim_saved() {
  Memory mem;
  Fake backend;
  Form form(backend, mem);
  form.openSection(settings_section_t::general, 0);
  turnTo(form, 1);
  form.activate(1);
  form.turn(1, 1);
  chooseNumericApply(form);
  turnTo(form, 3);
  backend.outcome = settings_save_state_t::failed;
  form.activate(1);
  TEST_ASSERT_EQUAL(Notice::failed, form.view().notice);
  TEST_ASSERT_TRUE(form.view().dirty);
  backend.rejectLate = true;
  form.activate(1);
  TEST_ASSERT_TRUE(form.closed());
  TEST_ASSERT_EQUAL(Notice::changed, form.view().notice);
  TEST_ASSERT_EQUAL(0, mem.active);
  TEST_ASSERT_EQUAL(1, backend.saves);
}
static void validation_and_save_transient_admission_fail_closed() {
  Memory mem;
  Fake backend;
  Form form(backend, mem);
  form.openSection(settings_section_t::general, 0);
  turnTo(form, 1);
  form.activate(1);
  form.turn(1, 1);
  mem.failAfter = 1;
  chooseNumericApply(form);
  TEST_ASSERT_TRUE(form.closed());
  TEST_ASSERT_EQUAL(Notice::noMemory, form.view().notice);
  TEST_ASSERT_EQUAL(0, backend.saves);
  TEST_ASSERT_EQUAL(0, mem.active);
  mem.failAfter = ~0u;
  form.openSection(settings_section_t::general, 0);
  turnTo(form, 1);
  form.activate(1);
  form.turn(1, 1);
  chooseNumericApply(form);
  turnTo(form, 3);
  mem.fail = true;
  form.activate(1);
  TEST_ASSERT_TRUE(form.closed());
  TEST_ASSERT_EQUAL(Notice::noMemory, form.view().notice);
  TEST_ASSERT_EQUAL(0, backend.saves);
  TEST_ASSERT_EQUAL(0, mem.active);
}
static void every_authoritative_section_field_is_reachable_with_board_filter() {
  Memory mem;
  Fake backend;
  Form form(backend, mem);
  for (unsigned eth = 0; eth < 2; eth++) {
    backend.ethernet = eth;
    for (unsigned section = 0; section < 5; section++) {
      const auto kind = static_cast<settings_section_t>(section);
      TEST_ASSERT_TRUE(form.openSection(kind, 0));
      const auto count = settingsFieldCount(kind, eth);
      TEST_ASSERT_EQUAL(count + 2, form.view().total);
      for (unsigned field = 0; field < count; field++) {
        TEST_ASSERT_EQUAL(field, form.view().selected);
        TEST_ASSERT_FALSE(form.closed());
        form.turn(1, 1);
      }
      TEST_ASSERT_EQUAL_STRING("Save", form.view().rows[2].label);
      form.back(1);
    }
  }
  TEST_ASSERT_EQUAL(0, backend.saves);
  TEST_ASSERT_EQUAL(0, mem.active);
}
static void
editor_timeout_and_auth_history_are_checked_on_input_without_tick() {
  Memory mem;
  Fake backend;
  Form form(backend, mem);
  form.openSection(settings_section_t::general, 0);
  form.activate(1);
  form.activate(30001);
  TEST_ASSERT_TRUE(form.closed());
  TEST_ASSERT_EQUAL(Notice::expired, form.view().notice);
  form.openSection(settings_section_t::general, 0);
  backend.auth.generation++;
  form.turn(1, 1);
  TEST_ASSERT_TRUE(form.closed());
  TEST_ASSERT_EQUAL(Notice::locked, form.view().notice);
}
static void
restoring_previous_field_after_rejection_fails_closed_on_admission() {
  Memory mem;
  Fake backend;
  Form form(backend, mem);
  form.openSection(settings_section_t::network, 0);
  turnTo(form, 6);
  form.activate(1);
  textApply(form, "192.168.123.123");
  TEST_ASSERT_TRUE(form.view().dirty);
  form.activate(1);
  unsigned selected = 0;
  pickerSelect(form, selected, 95);
  for (unsigned i = 0; i < 15; i++)
    form.activate(1);
  const char *invalid = "999.1.1.1";
  for (const char *p = invalid; *p; p++) {
    pickerSelect(form, selected, unsigned(*p) - 32);
    form.activate(1);
  }
  pickerSelect(form, selected, 97);
  // New value, section name, expanded object and expanded value fit.
  // Restoring the previous IP after rejection is the next allocation.
  mem.failAfter = mem.allocations + 4;
  form.activate(1);
  TEST_ASSERT_TRUE(mem.failedAdmission);
  TEST_ASSERT_TRUE(form.closed());
  TEST_ASSERT_EQUAL(Notice::noMemory, form.view().notice);
  TEST_ASSERT_EQUAL(0, mem.active);
  TEST_ASSERT_EQUAL(0, backend.saves);
}
static void
invalid_originals_stay_visible_and_cannot_be_applied_as_supported_values() {
  struct Invalid {
    settings_section_t section;
    uint8_t index;
    proto_field_t field;
    int32_t integer;
    float number;
  };
  const Invalid cases[] = {
      {settings_section_t::radio, 0, proto_field_t::bitLength, 57, 0},
      {settings_section_t::radio, 1, proto_field_t::proto, 99, 0},
      {settings_section_t::security, 0, proto_field_t::securityType, 99, 0},
      {settings_section_t::mqtt, 4, proto_field_t::port, 0, 0},
      {settings_section_t::mqtt, 4, proto_field_t::port, 65536, 0},
      {settings_section_t::radio, 9, proto_field_t::frequency, 0, 299},
      {settings_section_t::radio, 9, proto_field_t::frequency, 0, 929},
      {settings_section_t::radio, 9, proto_field_t::frequency, 0, NAN},
      {settings_section_t::radio, 9, proto_field_t::frequency, 0, INFINITY},
      {settings_section_t::radio, 9, proto_field_t::frequency, 0, -INFINITY},
      {settings_section_t::radio, 9, proto_field_t::frequency, 0, FLT_MAX}};
  for (const auto &value : cases) {
    Memory mem;
    Fake backend;
    backend.overrideField = value.field;
    backend.overrideInteger = value.integer;
    backend.overrideNumber = value.number;
    Form form(backend, mem);
    TEST_ASSERT_TRUE(form.openSection(value.section, 0));
    turnTo(form, value.index);
    if (value.field == proto_field_t::proto ||
        value.field == proto_field_t::securityType)
      TEST_ASSERT_EQUAL_STRING(
          "Invalid (99)",
          form.view().rows[value.index < 3 ? value.index : 2].value);
    form.activate(1);
    TEST_ASSERT_EQUAL(Mode::fields, form.view().mode);
    TEST_ASSERT_EQUAL(Notice::invalid, form.view().notice);
    TEST_ASSERT_FALSE(form.view().dirty);
    TEST_ASSERT_EQUAL(value.index, form.view().selected);
    // Repeated pressing cannot enter an editor or persist a replacement.
    form.activate(1);
    TEST_ASSERT_EQUAL(Mode::fields, form.view().mode);
    TEST_ASSERT_EQUAL(Notice::invalid, form.view().notice);
    TEST_ASSERT_EQUAL(0, mem.active);
    TEST_ASSERT_EQUAL(0, backend.saves);
    TEST_ASSERT_EQUAL(value.integer, backend.overrideInteger);
    if (isnan(value.number))
      TEST_ASSERT_TRUE(isnan(backend.overrideNumber));
    else
      TEST_ASSERT_TRUE(value.number == backend.overrideNumber);
  }
}
static void
invalid_original_rejection_keeps_other_changed_fields_and_valid_float_boundaries_editable() {
  Memory mem;
  Fake backend;
  Form form(backend, mem);
  form.openSection(settings_section_t::mqtt, 0);
  form.activate(1);
  form.turn(1, 1);
  chooseNumericApply(form);
  TEST_ASSERT_TRUE(form.view().dirty);
  backend.overrideField = proto_field_t::port;
  backend.overrideInteger = 0;
  turnTo(form, 4);
  form.activate(1);
  TEST_ASSERT_EQUAL(Mode::fields, form.view().mode);
  TEST_ASSERT_TRUE(form.view().dirty);
  TEST_ASSERT_EQUAL(Notice::invalid, form.view().notice);
  form.back(1);
  TEST_ASSERT_EQUAL(0, mem.active);
  backend.overrideField = proto_field_t::rxBandwidth;
  backend.overrideNumber = 58.03f;
  form.openSection(settings_section_t::radio, 0);
  turnTo(form, 10);
  form.activate(1);
  TEST_ASSERT_EQUAL(Mode::adjust, form.view().mode);
  TEST_ASSERT_EQUAL_STRING("58.03", form.view().rows[0].value);
  form.back(1);
  TEST_ASSERT_EQUAL(0, backend.saves);
}
static void unknown_connection_is_labelled_invalid_and_never_retargeted() {
  Memory mem;
  Fake backend;
  backend.ethernet = true;
  backend.overrideField = proto_field_t::connMode;
  backend.overrideInteger = 99;
  Form form(backend, mem);
  form.openSection(settings_section_t::network, 0);
  TEST_ASSERT_EQUAL_STRING("Invalid (99)", form.view().rows[0].value);
  form.activate(1);
  TEST_ASSERT_EQUAL(Mode::fields, form.view().mode);
  TEST_ASSERT_EQUAL(Notice::invalid, form.view().notice);
  TEST_ASSERT_FALSE(form.view().dirty);
  TEST_ASSERT_EQUAL(99, backend.overrideInteger);
  TEST_ASSERT_EQUAL(0, backend.saves);
}
static void
out_of_band_discard_wipes_secret_draft_and_active_editor_even_when_locked() {
  Memory mem;
  Fake backend;
  Form form(backend, mem);
  form.openSection(settings_section_t::network, 0);
  turnTo(form, 2);
  form.activate(1);
  form.turn(1, 1);
  form.activate(1);
  textApply(form, "X");
  TEST_ASSERT_TRUE(form.view().dirty);
  TEST_ASSERT_GREATER_THAN(0, mem.active);
  form.turn(-1, 1);
  form.activate(1);
  TEST_ASSERT_EQUAL(Mode::text, form.view().mode);
  backend.auth.allowed = false;
  const auto activity = backend.activities;
  form.discard();
  TEST_ASSERT_TRUE(form.closed());
  TEST_ASSERT_EQUAL(Notice::none, form.view().notice);
  TEST_ASSERT_EQUAL(0, mem.active);
  TEST_ASSERT_EQUAL(0, backend.saves);
  TEST_ASSERT_EQUAL(activity, backend.activities);
  TEST_ASSERT_EQUAL_STRING("", form.view().rows[0].value);
  form.discard();
  TEST_ASSERT_TRUE(form.closed());
  TEST_ASSERT_EQUAL(0, mem.active);
  TEST_ASSERT_EQUAL(0, backend.saves);
}
static void empty_save_closes_without_storage_or_allocating_a_request() {
  for(uint8_t i=0;i<5;i++) {
    Memory memory;Fake backend;Form form(backend,memory);
    const auto section=settings_section_t(i);
    TEST_ASSERT_TRUE(form.openSection(section,1));
    turnTo(form,settingsFieldCount(section,false));
    form.activate(1);
    TEST_ASSERT_TRUE(form.closed());
    TEST_ASSERT_EQUAL(Notice::unchanged,form.view().notice);
    TEST_ASSERT_EQUAL(0,backend.saves);
    TEST_ASSERT_EQUAL(0,memory.active);
    TEST_ASSERT_EQUAL(0,memory.allocations);
  }
}
int main() {
  UNITY_BEGIN();
  RUN_TEST(empty_save_closes_without_storage_or_allocating_a_request);
  RUN_TEST(rotation_and_field_apply_do_not_persist_until_explicit_save);
  RUN_TEST(cancel_auth_revision_and_timeout_close_and_release_drafts);
  RUN_TEST(risky_save_defaults_to_cancel_and_partial_retains_explicit_retry);
  RUN_TEST(
      secret_views_expose_presence_only_and_keep_does_not_copy_stored_secret);
  RUN_TEST(readonly_pins_and_no_ethernet_connection_never_open_editor);
  RUN_TEST(allocation_failure_closes_and_never_applies);
  RUN_TEST(
      secret_replace_is_changed_only_and_never_echoes_stored_or_fresh_bytes);
  RUN_TEST(invalid_ip_is_refused_by_shared_validator_without_polluting_draft);
  RUN_TEST(
      numeric_coarse_steps_reach_bounds_and_frequency_preserves_third_decimal);
  RUN_TEST(late_backend_recheck_and_failed_persistence_never_claim_saved);
  RUN_TEST(restoring_previous_field_after_rejection_fails_closed_on_admission);
  RUN_TEST(validation_and_save_transient_admission_fail_closed);
  RUN_TEST(every_authoritative_section_field_is_reachable_with_board_filter);
  RUN_TEST(editor_timeout_and_auth_history_are_checked_on_input_without_tick);
  RUN_TEST(
      invalid_originals_stay_visible_and_cannot_be_applied_as_supported_values);
  RUN_TEST(
      invalid_original_rejection_keeps_other_changed_fields_and_valid_float_boundaries_editable);
  RUN_TEST(unknown_connection_is_labelled_invalid_and_never_retargeted);
  RUN_TEST(
      out_of_band_discard_wipes_secret_draft_and_active_editor_even_when_locked);
  return UNITY_END();
}
