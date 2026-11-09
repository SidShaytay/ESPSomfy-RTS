#include "LocalMembership.h"
#include <string.h>
#include <unity.h>
using namespace LocalUi;
using namespace LocalUi::Advanced;
using namespace LocalUi::EntityAdmin;
void setUp() {}
void tearDown() {}
static MembershipRows members() {
  MembershipRows rows;
  rows.count = 32;rows.groupSlot=15;
  memset(rows.devices, 255, sizeof(rows.devices));
  for (unsigned i = 0; i < 32; i++)
    rows.slots[i] = i;
  rows.ids[0] = 7;
  rows.devices[0] = 31;
  rows.ids[29] = 88;
  rows.ids[31] = 255;
  return rows;
}
static MembershipSelection selected(const MembershipRows &rows, unsigned index,
                                    bool adding = false) {
  MembershipSelection out;
  out.id = rows.ids[index];out.groupSlot=rows.groupSlot;
  out.slot = rows.slots[index];
  out.deviceSlot = rows.devices[index];
  out.adding = adding;
  out.hasCandidate = out.id != 255;
  if (out.hasCandidate) {
    auto &r = out.candidate;
    r.ref = {EntityKind::device, out.id};
    r.targetExists = out.deviceSlot != 255;
    r.modelRevision = r.restoreRevision = r.radioRevision = 1;
    if (r.targetExists) {
      r.identity = 1007;
      r.revision = 1;
      r.slotCount = 1;
      r.members[0] = {out.id, true, r.identity, 1};
    }
  }
  snprintf(out.label, sizeof(out.label), "#%u Device", out.id);
  strcpy(out.groupIdentity, "Group#16 16777215 RTV auto80");
  if (out.id == 255)
    strcpy(out.deviceIdentity, "Unused placeholder #255");
  else
    snprintf(out.deviceIdentity, sizeof(out.deviceIdentity),
             "Dev#%u 1007 RTS 56b", out.id);
  return out;
}
static void all_physical_slots_and_virtual_add_are_bounded() {
  MembershipDraft draft;
  const auto rows = members();
  TEST_ASSERT_TRUE(draft.open(false, rows));
  FormView view;
  draft.render(view, 16, "");
  TEST_ASSERT_EQUAL(33, view.total);
  TEST_ASSERT_EQUAL(3, view.rowCount);
  TEST_ASSERT_EQUAL(0, view.offset);
  TEST_ASSERT_TRUE(draft.needsLabel(0));
  draft.turn(1);
  TEST_ASSERT_EQUAL(MembershipEffect::none,
                    draft.activate()); // Free slot is inspect-only.
  for (unsigned i = 1; i < 32; i++)
    draft.turn(1);
  draft.render(view, 16, "");
  TEST_ASSERT_EQUAL(32, view.selected);
  TEST_ASSERT_EQUAL(30, view.offset);
  TEST_ASSERT_EQUAL_STRING("Add device (metadata)", view.rows[2].label);
  TEST_ASSERT_FALSE(draft.needsLabel(32));
  TEST_ASSERT_EQUAL(MembershipEffect::readCandidates, draft.activate());
}
static void review_is_default_cancel_and_255_never_fabricates_device_context() {
  MembershipDraft draft;
  const auto rows = members();
  draft.open(false, rows);
  for (unsigned i = 0; i < 31; i++)
    draft.turn(1);
  TEST_ASSERT_EQUAL(MembershipEffect::select, draft.activate());
  auto choice = selected(rows, 31);
  TEST_ASSERT_TRUE(draft.select(choice));
  FormView view;
  draft.render(view, 16, "");
  TEST_ASSERT_EQUAL(Mode::membershipReview, view.mode);
  TEST_ASSERT_EQUAL(0, view.selected);
  TEST_ASSERT_EQUAL_STRING("Cancel", view.rows[0].label);
  TEST_ASSERT_NOT_NULL(strstr(view.rows[1].label, "255"));
  TEST_ASSERT_EQUAL_STRING(choice.groupIdentity, view.rows[0].value);
  TEST_ASSERT_EQUAL_STRING(choice.deviceIdentity, view.rows[1].value);
  TEST_ASSERT_NOT_NULL(strstr(view.hint, "placeholders"));
  TEST_ASSERT_EQUAL(MembershipEffect::discardReview, draft.activate());
  TEST_ASSERT_FALSE(draft.selection().id);
  draft.activate();
  choice = selected(rows, 31);
  choice.hasCandidate = true;
  TEST_ASSERT_FALSE(draft.select(choice)); // No fake EntityRef for sentinel.
  choice = selected(rows, 31);
  TEST_ASSERT_TRUE(draft.select(choice));
  draft.turn(1);
  TEST_ASSERT_EQUAL(MembershipEffect::queueChange, draft.activate());
}
static void present_and_missing_removal_keep_the_motor_consequence_visible() {
  for (unsigned index : {0u, 29u}) {
    MembershipDraft draft;
    const auto rows = members();
    draft.open(false, rows);
    for (unsigned i = 0; i < index; i++)
      draft.turn(1);
    TEST_ASSERT_TRUE(draft.select(selected(rows, index)));
    FormView view;
    draft.render(view, 16, "");
    TEST_ASSERT_NOT_NULL(strstr(view.hint, "still respond"));
    TEST_ASSERT_NOT_NULL(strstr(view.hint, "No RF/unpair"));
  }
}
static void
candidates_use_stable_ids_and_partial_or_failed_saves_never_replay() {
  MembershipRows rows;
  rows.count = 1;rows.groupSlot=15;
  rows.ids[0] = 32;
  rows.slots[0] = 255;
  rows.devices[0] = 31;
  MembershipDraft draft;
  TEST_ASSERT_TRUE(draft.open(true, rows));
  TEST_ASSERT_EQUAL(MembershipEffect::select, draft.activate());
  auto choice = selected(rows, 0, true);
  TEST_ASSERT_TRUE(draft.select(choice));
  FormView view;
  draft.render(view, 16, "");
  TEST_ASSERT_EQUAL(0, view.selected);
  TEST_ASSERT_NOT_NULL(strstr(view.hint, "pairing unchanged"));
  TEST_ASSERT_NOT_NULL(strstr(view.hint, "may affect"));
  draft.turn(1);
  TEST_ASSERT_EQUAL(MembershipEffect::queueChange, draft.activate());
  draft.queued();
  auto result = edit_result_t::completed(
      16, {ModelCommitState::uncertain, ModelCommitStage::verifyBytes});
  TEST_ASSERT_EQUAL(MembershipCompletion::applied, draft.complete(result));
  draft.render(view, 16, "");
  TEST_ASSERT_EQUAL(1, view.selected);
  TEST_ASSERT_EQUAL_STRING("Retry model file save", view.rows[0].label);
  TEST_ASSERT_FALSE(draft.selection().id);
  TEST_ASSERT_EQUAL(0, draft.rows().count);
  draft.turn(-1);
  TEST_ASSERT_EQUAL(MembershipEffect::queueFile, draft.activate());
  draft.queued();
  draft.render(view, 16, "");
  TEST_ASSERT_EQUAL_STRING("File save queued", view.title);
  draft.fileComplete({ModelCommitState::saved});
  draft.render(view, 16, "");
  TEST_ASSERT_EQUAL_STRING("Membership saved", view.title);
  auto unchanged = edit_result_t::unchanged(16);
  draft.open(true, rows);
  TEST_ASSERT_EQUAL(MembershipCompletion::unchanged, draft.complete(unchanged));
  edit_result_t invalid;
  invalid.ok = true;
  draft.open(true, rows);
  TEST_ASSERT_EQUAL(
      MembershipCompletion::refused,
      draft.complete(invalid)); // applied=false alone is not unchanged.
  rows.full = true;
  draft.open(true, rows);
  TEST_ASSERT_EQUAL(MembershipEffect::none, draft.activate());
  draft.render(view, 16, "");
  TEST_ASSERT_EQUAL_STRING("Group is full", view.rows[0].label);
}
int main() {
  UNITY_BEGIN();
  RUN_TEST(present_and_missing_removal_keep_the_motor_consequence_visible);
  RUN_TEST(all_physical_slots_and_virtual_add_are_bounded);
  RUN_TEST(review_is_default_cancel_and_255_never_fabricates_device_context);
  RUN_TEST(candidates_use_stable_ids_and_partial_or_failed_saves_never_replay);
  return UNITY_END();
}
