#include "ModelLoadTrust.h"
#include "SomfyEditor.h"
#include <string>
#include <unity.h>
#include <vector>
#define SOMFY_H
#define PROTO_SOCKET_H
static std::vector<std::string> effects;
struct SomfyShade {
  unsigned configs = 0;
  void configChanged() {
    ++configs;
    effects.push_back("shade");
  }
};
struct SomfyGroup {
  unsigned configs = 0;
  void configChanged() {
    ++configs;
    effects.push_back("group");
  }
};
struct SomfyRoom {};
struct SomfyShadeController {
  ModelLoadTrust modelLoad{ModelLoadStatus::loaded};
  SomfyShade shade;
  SomfyGroup group;
  SomfyRoom room;
  SomfyShade *getShadeById(uint8_t id) { return id == 22 ? &shade : nullptr; }
  SomfyGroup *getGroupById(uint8_t id) { return id == 11 ? &group : nullptr; }
  SomfyRoom *getRoomById(uint8_t id) { return id == 7 ? &room : nullptr; }
} somfy;
struct RecordingSocket {
  void replyEdit(const proto_reply_to_t &to, model_edit_type_t,
                 const edit_result_t &) {
    if (to.id)
      effects.push_back("reply");
  }
  void modelPersistence(ModelCommitReport,uint32_t){}
  void room(SomfyRoom &) { effects.push_back("room"); }
  void order(sort_list_t) { effects.push_back("order"); }
  void repeaters() { effects.push_back("repeaters"); }
  void shadeRemoved(uint8_t) { effects.push_back("shadeRemoved"); }
  void groupRemoved(uint8_t) { effects.push_back("groupRemoved"); }
  void roomRemoved(uint8_t) { effects.push_back("roomRemoved"); }
} protoSocket;
static edit_result_t mutation;
static unsigned mutations = 0, cancellations = 0;
edit_result_t mutateModelEdit(model_edit_t &) {
  ++mutations;
  effects.push_back("apply");
  return mutation;
}
void cancelModelEdit(model_edit_t *) { ++cancellations; }
#include "../../src/ModelEditRuntime.cpp"
void setUp() {
  effects.clear();
  somfy = {};
  mutations = cancellations = 0;
  mutation = edit_result_t::completed(
      22, {ModelCommitState::failed, ModelCommitStage::write});
}
void tearDown() {}
static void applied_unsaved_edit_replies_then_fans_out_once_and_preserves_id() {
  model_edit_t edit;
  edit.type = model_edit_type_t::add_shade;
  edit.replyTo = {1, 7};
  const auto result = applyModelEdit(edit);
  TEST_ASSERT_TRUE(result.applied);
  TEST_ASSERT_FALSE(result.ok);
  TEST_ASSERT_EQUAL(22, result.id);
  TEST_ASSERT_EQUAL(1, mutations);
  TEST_ASSERT_EQUAL(1, somfy.shade.configs);
  TEST_ASSERT_EQUAL(3, effects.size());
  TEST_ASSERT_EQUAL_STRING("apply", effects[0].c_str());
  TEST_ASSERT_EQUAL_STRING("reply", effects[1].c_str());
  TEST_ASSERT_EQUAL_STRING("shade", effects[2].c_str());
}
static void
local_apply_has_same_fanout_without_socket_reply_and_delete_is_not_replayed() {
  model_edit_t edit;
  edit.type = model_edit_type_t::delete_shade;
  const auto result = applyModelEdit(edit);
  TEST_ASSERT_TRUE(result.applied);
  TEST_ASSERT_EQUAL(1, mutations);
  TEST_ASSERT_EQUAL(2, effects.size());
  TEST_ASSERT_EQUAL_STRING("shadeRemoved", effects[1].c_str());
}
static void refused_edit_never_fans_out_and_queued_owner_frees_it_once() {
  model_edit_t edit;
  edit.type = model_edit_type_t::save_shade;
  edit.replyTo = {1, 7};
  mutation = edit_result_t::error("refused");
  runModelEdit(&edit);
  TEST_ASSERT_EQUAL(1, mutations);
  TEST_ASSERT_EQUAL(1, cancellations);
  TEST_ASSERT_EQUAL(0, somfy.shade.configs);
  TEST_ASSERT_EQUAL(2, effects.size());
  TEST_ASSERT_EQUAL_STRING("reply", effects[1].c_str());
}
static void failed_boot_refuses_edit_before_mutation() {
  somfy.modelLoad.fail();model_edit_t edit;const auto result=applyModelEdit(edit);
  TEST_ASSERT_FALSE(result.ok);TEST_ASSERT_FALSE(result.applied);TEST_ASSERT_EQUAL(0,mutations);TEST_ASSERT_TRUE(effects.empty());
}
int main() {
  UNITY_BEGIN();
  RUN_TEST(failed_boot_refuses_edit_before_mutation);
  RUN_TEST(applied_unsaved_edit_replies_then_fans_out_once_and_preserves_id);
  RUN_TEST(
      local_apply_has_same_fanout_without_socket_reply_and_delete_is_not_replayed);
  RUN_TEST(refused_edit_never_fans_out_and_queued_owner_frees_it_once);
  return UNITY_END();
}
