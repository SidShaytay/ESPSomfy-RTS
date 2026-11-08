#define ESPSOMFY_LOCAL_UI
#define SOMFY_H
#include "LocalEntityRuntime.h"
#include "LocalJsonAllocator.h"
#include "LocalUiAuthorizationAdapter.h"
#include "LoopQueue.h"
#include "ModelCommitService.h"
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
struct SomfyShadeController {
  uint64_t configurationRevision = 1;
  Room room;
  Room *getRoomById(uint8_t id) { return id == 4 ? &room : nullptr; }
  uint8_t getNextRoomId() { return 9; }
  int getMaxRoomOrder() { return 5; }
} somfy;
LoopQueue loopQueue;
static unsigned pendingCount = 0, applies = 0, commits = 0;
uint32_t LoopQueue::pending() const { return pendingCount; }
static ArduinoJson::Allocator *expectedAllocator = nullptr;
edit_result_t applyModelEdit(model_edit_t &edit) {
  ++applies;
  TEST_ASSERT_EQUAL_PTR(expectedAllocator, edit.doc.allocator());
  TEST_ASSERT_EQUAL_STRING("New room", edit.doc["name"].as<const char *>());
  TEST_ASSERT_EQUAL(model_edit_type_t::save_room, edit.type);
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
  somfy.configurationRevision = 1;
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
int main() {
  UNITY_BEGIN();
  RUN_TEST(read_preserves_original_and_prepare_is_side_effect_free);
  RUN_TEST(final_admission_checks_auth_history_freeze_and_queue);
  RUN_TEST(
      apply_swaps_psram_allocator_without_internal_copy_and_retry_is_file_only);
  return UNITY_END();
}
