#define ESPSOMFY_LOCAL_UI
#define SOMFY_H
#include "LocalEntityRuntime.h"
#include "LocalJsonAllocator.h"
#include "LocalUiAuthorizationAdapter.h"
#include "LoopQueue.h"
#include "ModelCommitService.h"
#include "ShadeSettings.h"
#include "SomfyEditor.h"
#include <map>
#include <stdlib.h>
#include <string.h>
#include <unity.h>
namespace LocalUi {
static SecuritySnapshot security{AuthMode::none, true, false, 1, 1};
static bool frozen = false;
SecuritySnapshot pollAuthorization() { return security; }
bool localControlsFrozen() { return frozen; }
void authorizationActivity() {}
} // namespace LocalUi
struct Room {
  uint8_t roomId = 4;
  char name[21] = "Original";
  int8_t sortOrder = 2;
};
struct Shade : RemoteSettings, ShadeSettings {
  uint8_t roomId = 4, gpioUp = 11, gpioDown = 12, gpioMy = 13, gpioFlags = 1;
  uint64_t configurationRevision = 1;
  radio_proto proto = radio_proto::RTS;
  char name[21] = "Shade";
  bool light = true;
  uint8_t id = 7;
  uint8_t getShadeId() { return id; }
  int8_t gpioError = 0;
  bool idle = true;
  bool expireDuringValidation = false;
  uint32_t gpioRelease = 0;
  bool isIdle() { return idle; }
  int8_t validateGPIOs(radio_proto, uint8_t, uint8_t, uint8_t) {
    if (expireDuringValidation)
      ++LocalUi::security.generation;
    return gpioError;
  }
  bool isInGroup() { return false; }
  uint32_t address = 100;
  uint32_t getRemoteAddress() { return address; }
};
struct Group : RemoteSettings {
  uint8_t roomId = 4;
  uint64_t configurationRevision = 1;
  radio_proto proto = radio_proto::RTS;
  char name[21] = "Group";
  uint8_t linkedShades[32] = {7};
  uint8_t id = 9;
  uint8_t getGroupId() { return id; }
  uint32_t address = 200;
  uint32_t getRemoteAddress() { return address; }
};
struct SomfyShadeController {
  uint64_t configurationRevision = 1;
  uint64_t restoreRevision = 1;
  struct {
    uint64_t configurationRevision = 1;
    struct {
      uint8_t type = 80;
    } config;
  } transceiver;
  Room room;
  Shade shades[2];
  Group groups[2];
  Shade &shade = shades[0];
  Group &group = groups[0];
  Shade *getShadeById(uint8_t id) {
    for (auto &s : shades)
      if (s.id == id && id != 255)
        return &s;
    return nullptr;
  }
  Group *getGroupById(uint8_t id) {
    for (auto &g : groups)
      if (g.id == id && id != 255)
        return &g;
    return nullptr;
  }
  uint8_t getNextShadeId() { return 8; }
  uint8_t getNextGroupId() { return 10; }
  uint32_t getNextRemoteAddress(uint8_t id) { return 1000 + id; }
  int maxRoomOrder = 5, maxShadeOrder = 5, maxGroupOrder = 5;
  Room *getRoomById(uint8_t id) { return id == 4 ? &room : nullptr; }
  uint8_t getNextRoomId() { return 9; }
  int getMaxRoomOrder() { return maxRoomOrder; }
  int getMaxShadeOrder() { return maxShadeOrder; }
  int getMaxGroupOrder() { return maxGroupOrder; }
} somfy;
LoopQueue loopQueue;
static bool restartScheduled = false;
bool restartPending() { return restartScheduled; }
static unsigned pendingCount = 0, applies = 0, commits = 0;
uint32_t LoopQueue::pending() const { return pendingCount; }
static ArduinoJson::Allocator *expectedAllocator = nullptr;
static const char *expectedName = "New room";
static model_edit_type_t expectedType = model_edit_type_t::save_room;
edit_result_t applyModelEdit(model_edit_t &edit) {
  ++applies;
  TEST_ASSERT_EQUAL_PTR(expectedAllocator, edit.doc.allocator());
  if (expectedType == model_edit_type_t::add_group)
    return edit_result_t::completed(
        10, {ModelCommitState::saved, ModelCommitStage::none});
  if (expectedType == model_edit_type_t::delete_group) {
    ++somfy.configurationRevision;
    somfy.group.id = 255;
    return edit_result_t::completed(
        9, {ModelCommitState::failed, ModelCommitStage::write});
  }
  if (expectedType == model_edit_type_t::add_shade) {
    TEST_ASSERT_EQUAL(80, edit.doc["bitLength"].as<int>());
    TEST_ASSERT_EQUAL(10000, edit.doc["upTime"].as<int>());
    TEST_ASSERT_EQUAL(1008, edit.doc["remoteAddress"].as<int>());
    TEST_ASSERT_TRUE(
        edit.doc["gpioUp"].isNull()); // inactive defaults not wire edits
    ++somfy.configurationRevision;
    somfy.shades[1].id = 8;
    return edit_result_t::completed(
        8, {ModelCommitState::saved, ModelCommitStage::none});
  }
  if (expectedType == model_edit_type_t::save_shade ||
      expectedType == model_edit_type_t::save_group) {
    ++somfy.configurationRevision;
    return edit_result_t::completed(
        expectedType == model_edit_type_t::save_shade ? 7 : 9,
        {ModelCommitState::saved, ModelCommitStage::none});
  }
  TEST_ASSERT_EQUAL_STRING(expectedName, edit.doc["name"].as<const char *>());
  TEST_ASSERT_EQUAL(expectedType, edit.type);
  if (expectedType == model_edit_type_t::add_room) {
    TEST_ASSERT_TRUE(edit.doc["roomId"].isNull());
    TEST_ASSERT_EQUAL(0, edit.doc["sortOrder"].as<int>());
  } else
    TEST_ASSERT_EQUAL(4, edit.doc["roomId"].as<int>());
  TEST_ASSERT_EQUAL(0, edit.replyTo.client);
  ++somfy.configurationRevision;
  return edit_result_t::completed(
      4, {ModelCommitState::saved, ModelCommitStage::none});
}
ModelCommitReport applyModelCommit(const proto_reply_to_t &, ModelCommitMode) {
  ++commits;
  return {{ModelCommitState::saved, ModelCommitStage::none}};
}
#include "../../src/LocalEntityRuntime.cpp"
struct Memory : ArduinoJson::Allocator {
  unsigned active = 0;
  void *allocate(size_t n) override {
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
  void *reallocate(void *p, size_t n) override { return realloc(p, n); }
};
static std::map<void *, size_t> blocks;
static void *acquireBlock(size_t size) {
  void *p = malloc(size);
  if (p)
    blocks[p] = size;
  return p;
}
static void releaseBlock(void *p) {
  const auto found = blocks.find(p);
  TEST_ASSERT_TRUE(found != blocks.end());
  for (size_t i = 0; i < found->second; i++)
    TEST_ASSERT_EQUAL_UINT8(0, static_cast<unsigned char *>(p)[i]);
  blocks.erase(found);
  free(p);
}
void setUp() {
  LocalUi::security = {LocalUi::AuthMode::none, true, false, 1, 1};
  LocalUi::frozen = false;
  pendingCount = applies = commits = 0;
  restartScheduled = false;
  somfy.configurationRevision = 1;
  somfy.restoreRevision = 1;
  somfy.shade = {};
  somfy.group = {};
  somfy.shades[1] = {};
  somfy.shades[1].id = 255;
  somfy.groups[1] = {};
  somfy.groups[1].id = 255;
  somfy.transceiver.configurationRevision = 1;
  somfy.maxRoomOrder = somfy.maxShadeOrder = somfy.maxGroupOrder = 5;
  expectedName = "New room";
  expectedType = model_edit_type_t::save_room;
  uint8_t prepared;
  LocalUi::EntityAdmin::runtimeBackend().prepare(
      LocalUi::EntityAdmin::Kind::room, 4, prepared);
}
void tearDown() {}
using namespace LocalUi::EntityAdmin;
static void read_preserves_original_and_prepare_is_side_effect_free() {
  auto &b = runtimeBackend();
  uint8_t id = 0;
  Value value;
  TEST_ASSERT_TRUE(b.prepare(Kind::room, 0, id));
  TEST_ASSERT_EQUAL(9, id);
  TEST_ASSERT_TRUE(b.read(Kind::room, 0, proto_field_t::sortOrder, value));
  TEST_ASSERT_EQUAL(6, value.integer);
  TEST_ASSERT_TRUE(b.read(Kind::room, 4, proto_field_t::name, value));
  TEST_ASSERT_EQUAL_STRING("Original", value.text);
  TEST_ASSERT_FALSE(b.prepare(Kind::room, 5, id));
  TEST_ASSERT_EQUAL(0, applies);
  TEST_ASSERT_EQUAL(0, commits);
}
static void final_admission_checks_auth_history_freeze_and_queue() {
  auto &b = runtimeBackend();
  const Authorization admitted{true, 1, 1};
  Memory memory;
  for (unsigned i = 0; i < 5; i++) {
    JsonDocument doc(&memory);
    doc["name"] = "New room";
    doc["roomId"] = 4;
    LocalUi::security.allowed = true;
    LocalUi::security.generation = 1;
    LocalUi::security.configurationGeneration = 1;
    somfy.configurationRevision = 1;
    pendingCount = 0;
    LocalUi::frozen = false;
    if (i == 0)
      LocalUi::security.allowed = false;
    if (i == 1)
      LocalUi::security.configurationGeneration = 2;
    if (i == 2)
      somfy.configurationRevision = 2;
    if (i == 3)
      pendingCount = 1;
    if (i == 4)
      LocalUi::frozen = true;
    const auto mutation =
        b.apply(model_edit_type_t::save_room, doc, admitted, 1);
    TEST_ASSERT_EQUAL(i == 3             ? Admission::busy
                      : i == 1 || i == 2 ? Admission::stale
                                         : Admission::locked,
                      mutation.admission);
    TEST_ASSERT_EQUAL(mutation.admission,
                      b.retryFileCommit(admitted, 1).admission);
    TEST_ASSERT_EQUAL(0, applies);
    TEST_ASSERT_EQUAL(0, commits);
  }
}
static void
apply_swaps_psram_allocator_without_internal_copy_and_retry_is_file_only() {
  auto &b = runtimeBackend();
  // Default native ArduinoJson pools are larger than target pools. This cap
  // verifies bounded ownership/wipe, not the target physical allocation size.
  LocalUi::JsonAllocator memory(65536, acquireBlock, releaseBlock);
  expectedAllocator = &memory;
  JsonDocument doc(&memory);
  doc["name"] = "New room";
  doc["roomId"] = 4;
  TEST_ASSERT_TRUE(memory.used());
  const auto result =
      b.apply(model_edit_type_t::save_room, doc, {true, 1, 1}, 1);
  TEST_ASSERT_EQUAL(Admission::accepted, result.admission);
  TEST_ASSERT_TRUE(result.edit.applied);
  TEST_ASSERT_EQUAL(0, memory.used());
  TEST_ASSERT_TRUE(blocks.empty());
  TEST_ASSERT_TRUE(doc.isNull());
  TEST_ASSERT_EQUAL(Admission::accepted,
                    b.retryFileCommit({true, 1, 1}, 2).admission);
  TEST_ASSERT_EQUAL(1, applies);
  TEST_ASSERT_EQUAL(1, commits);
}
static void actual_add_room_default_128_cannot_wrap_without_replacement() {
  somfy.maxRoomOrder = 127;
  auto &backend = runtimeBackend();
  Value value;
  TEST_ASSERT_TRUE(
      backend.read(Kind::room, 0, proto_field_t::sortOrder, value));
  TEST_ASSERT_EQUAL(128, value.integer);
  Memory memory;
  expectedAllocator = &memory;
  expectedName = "A";
  expectedType = model_edit_type_t::add_room;
  Form form(backend, memory);
  TEST_ASSERT_TRUE(form.open(Kind::room, 0, 0));
  form.activate(1);
  for (int i = 0; i < 33; i++)
    form.turn(1, 1);
  form.activate(1);
  for (int i = 0; i < 64; i++)
    form.turn(1, 1);
  form.activate(1);
  form.turn(1, 1);
  form.turn(1, 1);
  form.activate(1); // name-only Save
  TEST_ASSERT_EQUAL(0, applies);
  TEST_ASSERT_EQUAL(1, form.view().selected);
  form.activate(1);
  TEST_ASSERT_EQUAL(Mode::replace, form.view().mode);
  form.turn(1, 1);
  form.activate(1);
  form.activate(1);
  form.turn(-1, 1);
  form.activate(1); // Replace -> Apply zero
  TEST_ASSERT_EQUAL(0, applies);
  form.turn(1, 1);
  form.activate(1);
  TEST_ASSERT_EQUAL(1, applies);
  TEST_ASSERT_EQUAL(0, memory.active);
}
static void shade_group_reads_preserve_legacy_and_offer_effective_defaults() {
  auto &b = runtimeBackend();
  uint8_t id;
  Value value;
  for (Kind kind : {Kind::shade, Kind::group}) {
    TEST_ASSERT_TRUE(b.prepare(kind, 0, id));
    const auto type = kind == Kind::shade ? model_edit_type_t::add_shade
                                          : model_edit_type_t::add_group;
    const auto *form = editForm(type);
    for (uint8_t i = 0; i < form->count; ++i)
      TEST_ASSERT_TRUE(b.read(kind, 0, form->fields[i], value));
    TEST_ASSERT_TRUE(b.read(kind, 0, proto_field_t::bitLength, value));
    TEST_ASSERT_EQUAL(80, value.integer);
    TEST_ASSERT_TRUE(b.read(kind, 0, proto_field_t::remoteAddress, value));
    TEST_ASSERT_EQUAL(kind == Kind::shade ? 1008 : 1010, value.integer);
    TEST_ASSERT_TRUE(b.prepare(kind, kind == Kind::shade ? 7 : 9, id));
    TEST_ASSERT_TRUE(b.read(kind, id, proto_field_t::bitLength, value));
    TEST_ASSERT_EQUAL(0, value.integer); // inherited legacy original preserved
  }
  TEST_ASSERT_TRUE(b.prepare(Kind::shade, 0, id));
  TEST_ASSERT_TRUE(b.read(Kind::shade, 0, proto_field_t::gpioUp, value));
  TEST_ASSERT_EQUAL(255, value.integer);
  TEST_ASSERT_TRUE(b.read(Kind::shade, 0, proto_field_t::upTime, value));
  TEST_ASSERT_EQUAL(10000, value.integer);
  TEST_ASSERT_EQUAL(0, applies);
  TEST_ASSERT_EQUAL(0, commits);
}
static void room_choices_have_stable_ids_first() {
  auto &b = runtimeBackend();
  uint8_t id;
  TEST_ASSERT_TRUE(b.prepare(Kind::shade, 7, id));
  TEST_ASSERT_EQUAL(2, b.choiceCount(Kind::shade, 7, proto_field_t::roomId));
  Choice choice;
  TEST_ASSERT_TRUE(
      b.choiceAt(Kind::shade, 7, proto_field_t::roomId, 0, choice));
  TEST_ASSERT_EQUAL(0, choice.value);
  TEST_ASSERT_EQUAL_STRING("Unassigned", choice.label);
  TEST_ASSERT_TRUE(
      b.choiceAt(Kind::shade, 7, proto_field_t::roomId, 1, choice));
  TEST_ASSERT_EQUAL(4, choice.value);
  TEST_ASSERT_EQUAL_STRING("#4 Original", choice.label);
  TEST_ASSERT_FALSE(
      b.choiceAt(Kind::shade, 7, proto_field_t::roomId, 2, choice));
}
static void physical_group_context_changes_block_commit_without_global_bump() {
  auto &b = runtimeBackend();
  uint8_t id;
  TEST_ASSERT_TRUE(b.prepare(Kind::group, 9, id));
  // Dangling/duplicate physical membership stays administrable.
  somfy.group.linkedShades[4] = 7;
  somfy.group.linkedShades[31] = 88;
  TEST_ASSERT_TRUE(b.prepare(Kind::group, 9, id));
  somfy.group.linkedShades[31] = 89;
  TEST_ASSERT_EQUAL(Admission::stale,
                    b.retryFileCommit({true, 1, 1}, 1).admission);
  TEST_ASSERT_EQUAL(0, commits);
}
static void invalid_fields_and_inactive_gpio_refuse_before_model_history() {
  auto &b = runtimeBackend();
  uint8_t id;
  struct Case {
    const char *key;
    int64_t value;
    proto_field_t field;
  };
  const Case cases[] = {{"bitLength", 57, proto_field_t::bitLength},
                        {"remoteAddress", 200, proto_field_t::remoteAddress},
                        {"roomId", 3, proto_field_t::roomId},
                        {"gpioUp", 255, proto_field_t::gpioUp}};
  for (const auto &test : cases) {
    TEST_ASSERT_TRUE(b.prepare(Kind::shade, 7, id));
    Memory memory;
    JsonDocument doc(&memory);
    expectedAllocator = &memory;
    expectedType = model_edit_type_t::save_shade;
    doc["shadeId"] = 7;
    doc[test.key] = test.value;
    const auto result =
        b.apply(model_edit_type_t::save_shade, doc, {true, 1, 1}, 1);
    TEST_ASSERT_EQUAL(Admission::invalid, result.admission);
    TEST_ASSERT_EQUAL(test.field, result.field);
    TEST_ASSERT_NOT_EQUAL(0, result.reason[0]);
    TEST_ASSERT_EQUAL(0, applies);
    TEST_ASSERT_EQUAL(1, somfy.configurationRevision);
    TEST_ASSERT_EQUAL(test.value, doc[test.key].as<int64_t>());
  }
}
static void add_materializes_effective_defaults_in_original_allocator() {
  auto &b = runtimeBackend();
  uint8_t id;
  TEST_ASSERT_TRUE(b.prepare(Kind::shade, 0, id));
  Memory memory;
  expectedAllocator = &memory;
  expectedType = model_edit_type_t::add_shade;
  JsonDocument doc(&memory);
  doc["name"] = "New shade";
  const auto result = b.apply(expectedType, doc, {true, 1, 1}, 1);
  TEST_ASSERT_EQUAL(Admission::accepted, result.admission);
  TEST_ASSERT_TRUE(result.edit.applied);
  TEST_ASSERT_EQUAL(1, applies);
  TEST_ASSERT_EQUAL(0, memory.active);
}
static void mutation_requires_idle_members_but_file_retry_does_not() {
  auto &b = runtimeBackend();
  for (const auto kind : {Kind::shade, Kind::group}) {
    uint8_t id;
    TEST_ASSERT_TRUE(b.prepare(kind, kind == Kind::shade ? 7 : 9, id));
    Memory memory;
    JsonDocument doc(&memory);
    doc[kind == Kind::shade ? "shadeId" : "groupId"] = id;
    doc["name"] = "Changed";
    somfy.shade.idle = false;
    TEST_ASSERT_EQUAL(Admission::busy,
                      b.apply(kind == Kind::shade
                                  ? model_edit_type_t::save_shade
                                  : model_edit_type_t::save_group,
                              doc, {true, 1, 1}, 1)
                          .admission);
    TEST_ASSERT_EQUAL(0, applies);
    TEST_ASSERT_EQUAL(Admission::accepted,
                      b.retryFileCommit({true, 1, 1}, 1).admission);
    somfy.shade.idle = true;
    somfy.shade.gpioRelease = 123;
    TEST_ASSERT_EQUAL(Admission::busy,
                      b.apply(kind == Kind::shade
                                  ? model_edit_type_t::save_shade
                                  : model_edit_type_t::save_group,
                              doc, {true, 1, 1}, 1)
                          .admission);
    somfy.shade.gpioRelease = 0;
  }
}
static void correctable_tilt_gpio_errors_and_final_expiry_preserve_draft() {
  auto &b = runtimeBackend();
  uint8_t id;
  Memory memory;
  expectedAllocator = &memory;
  expectedType = model_edit_type_t::save_shade;
  for (unsigned scenario = 0; scenario < 3; ++scenario) {
    TEST_ASSERT_TRUE(b.prepare(Kind::shade, 7, id));
    JsonDocument doc(&memory);
    doc["shadeId"] = 7;
    doc["name"] = "Changed";
    somfy.shade.tiltType =
        scenario == 0 ? tilt_types::integrated : tilt_types::none;
    somfy.shade.gpioError = scenario == 1 ? PIN_CONFLICT_RADIO : 0;
    somfy.shade.expireDuringValidation = scenario == 2;
    const auto result = b.apply(expectedType, doc, {true, 1, 1}, 1);
    TEST_ASSERT_EQUAL(scenario == 2 ? Admission::stale : Admission::invalid,
                      result.admission);
    if (scenario == 0)
      TEST_ASSERT_EQUAL(proto_field_t::tiltType, result.field);
    TEST_ASSERT_EQUAL_STRING("Changed", doc["name"].as<const char *>());
    TEST_ASSERT_EQUAL(0, applies);
    TEST_ASSERT_EQUAL(1, somfy.configurationRevision);
  }
}
static void restart_pending_blocks_mutation_and_commit() {
  auto &b = runtimeBackend();
  restartScheduled = true;
  Memory memory;
  JsonDocument doc(&memory);
  doc["roomId"] = 4;
  doc["name"] = "New room";
  TEST_ASSERT_EQUAL(
      Admission::busy,
      b.apply(model_edit_type_t::save_room, doc, {true, 1, 1}, 1).admission);
  TEST_ASSERT_EQUAL(Admission::busy,
                    b.retryFileCommit({true, 1, 1}, 1).admission);
  TEST_ASSERT_EQUAL(0, applies);
  TEST_ASSERT_EQUAL(0, commits);
}
static void applied_delete_failure_adopts_tombstone_for_file_only_retry() {
  auto &b = runtimeBackend();
  uint8_t id;
  TEST_ASSERT_TRUE(b.prepare(Kind::group, 9, id));
  Memory memory;
  expectedAllocator = &memory;
  expectedType = model_edit_type_t::delete_group;
  JsonDocument doc(&memory);
  doc["groupId"] = 9;
  const auto result = b.apply(expectedType, doc, {true, 1, 1}, 1);
  TEST_ASSERT_TRUE(result.edit.applied);
  TEST_ASSERT_FALSE(result.edit.durable());
  TEST_ASSERT_EQUAL(9, result.edit.id);
  TEST_ASSERT_EQUAL(1, applies);
  TEST_ASSERT_EQUAL(Admission::accepted,
                    b.retryFileCommit({true, 1, 1}, 2).admission);
  TEST_ASSERT_EQUAL(1, commits);
  // A same-ID recreation invalidates retry even if history is corrupt/stale.
  somfy.group.id = 9;
  TEST_ASSERT_EQUAL(Admission::stale,
                    b.retryFileCommit({true, 1, 1}, 2).admission);
  TEST_ASSERT_EQUAL(1, commits);
  TEST_ASSERT_EQUAL(1, applies);
}
static void allocation_failure_in_materialized_defaults_has_no_effects() {
  struct Denied : ArduinoJson::Allocator {
    void *allocate(size_t) override { return nullptr; }
    void deallocate(void *) override {}
    void *reallocate(void *, size_t) override { return nullptr; }
  } memory;
  auto &b = runtimeBackend();
  uint8_t id;
  TEST_ASSERT_TRUE(b.prepare(Kind::shade, 0, id));
  JsonDocument doc(&memory);
  doc.to<JsonObject>();
  const auto result =
      b.apply(model_edit_type_t::add_shade, doc, {true, 1, 1}, 1);
  TEST_ASSERT_EQUAL(Admission::noMemory, result.admission);
  TEST_ASSERT_EQUAL(0, applies);
  TEST_ASSERT_EQUAL(1, somfy.configurationRevision);
}
static void hidden_new_sort_order_cannot_wrap_on_device_or_group_add() {
  auto &b = runtimeBackend();
  somfy.maxShadeOrder = somfy.maxGroupOrder = 127;
  for (const auto kind : {Kind::shade, Kind::group}) {
    uint8_t id;
    TEST_ASSERT_TRUE(b.prepare(kind, 0, id));
    Memory memory;
    expectedAllocator = &memory;
    expectedType = kind == Kind::shade ? model_edit_type_t::add_shade
                                       : model_edit_type_t::add_group;
    JsonDocument doc(&memory);
    doc["name"] = "Offered";
    const auto result = b.apply(expectedType, doc, {true, 1, 1}, 1);
    TEST_ASSERT_EQUAL(Admission::invalid, result.admission);
    TEST_ASSERT_EQUAL(proto_field_t::count, result.field);
    TEST_ASSERT_EQUAL_STRING(kind == Kind::shade
                                 ? "Reorder devices before adding"
                                 : "Reorder groups before adding",
                             result.reason);
    TEST_ASSERT_EQUAL(0, applies);
    TEST_ASSERT_EQUAL(1, somfy.configurationRevision);
    TEST_ASSERT_EQUAL_STRING("Offered", doc["name"].as<const char *>());
  }
}
int main() {
  UNITY_BEGIN();
  RUN_TEST(hidden_new_sort_order_cannot_wrap_on_device_or_group_add);
  RUN_TEST(applied_delete_failure_adopts_tombstone_for_file_only_retry);
  RUN_TEST(allocation_failure_in_materialized_defaults_has_no_effects);
  RUN_TEST(mutation_requires_idle_members_but_file_retry_does_not);
  RUN_TEST(correctable_tilt_gpio_errors_and_final_expiry_preserve_draft);
  RUN_TEST(restart_pending_blocks_mutation_and_commit);
  RUN_TEST(invalid_fields_and_inactive_gpio_refuse_before_model_history);
  RUN_TEST(add_materializes_effective_defaults_in_original_allocator);
  RUN_TEST(shade_group_reads_preserve_legacy_and_offer_effective_defaults);
  RUN_TEST(room_choices_have_stable_ids_first);
  RUN_TEST(physical_group_context_changes_block_commit_without_global_bump);
  RUN_TEST(actual_add_room_default_128_cannot_wrap_without_replacement);
  RUN_TEST(read_preserves_original_and_prepare_is_side_effect_free);
  RUN_TEST(final_admission_checks_auth_history_freeze_and_queue);
  RUN_TEST(
      apply_swaps_psram_allocator_without_internal_copy_and_retry_is_file_only);
  return UNITY_END();
}
