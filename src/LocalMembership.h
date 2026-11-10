#pragma once
#include "LocalEntityView.h"
#include "LocalTargetRead.h"
#include "ModelEditor.h"
namespace LocalUi {
namespace Advanced {
struct MembershipRows {
  uint8_t ids[32]{}, slots[32]{}, devices[32]{};
  uint8_t count = 0, groupSlot = 255;
  bool full = false;
};
// A 255 placeholder has no device reference. Other missing raw IDs retain an
// exact device tombstone, without pretending they are configured candidates.
struct MembershipSelection {
  RawTargetContext candidate;
  char label[32]{}, groupIdentity[48]{}, deviceIdentity[48]{};
  uint8_t id = 0, slot = 255, deviceSlot = 255, groupSlot = 255, occurrences = 0;
  bool adding = false, hasCandidate = false;
};
} // namespace Advanced
enum class MembershipEffect : uint8_t {
  none,
  leave,
  readCandidates,
  readMembers,
  select,
  discardReview,
  queueChange,
  queueFile
};
enum class MembershipCompletion : uint8_t { refused, unchanged, applied };
// No backend, owner, allocation, clock or frame is retained here. The Advanced
// parent supplies exact authority/context, labels, FIFO and its borrowed view.
class MembershipDraft {
  Advanced::MembershipSelection chosen;
  Advanced::MembershipRows list;
  enum class Page : uint8_t {
    closed,
    browse,
    confirm,
    pending,
    result,
    fault
  } page = Page::closed;
  uint8_t cursor = 0, returnCursor = 0;
  bool candidates = false, applied = false, retry = false;
  ModelCommitResult file;
  void clearIntent();

public:
  bool open(bool adding, const Advanced::MembershipRows &);
  void clear();
  bool active() const { return page != Page::closed; }
  bool pending() const { return page == Page::pending; }
  bool fileOnly() const { return applied; }
  bool hasSelection() const { return chosen.id != 0; }
  bool adding() const { return candidates; }
  uint8_t index() const { return cursor; }
  const Advanced::MembershipRows &rows() const { return list; }
  const Advanced::MembershipSelection &selection() const { return chosen; }
  bool needsLabel(uint8_t index) const;
  bool select(const Advanced::MembershipSelection &);
  void turn(int direction);
  MembershipEffect activate();
  MembershipEffect back();
  void queued();
  void busy();
  MembershipCompletion complete(const edit_result_t &);
  void fileComplete(ModelCommitResult);
  void render(EntityAdmin::FormView &, uint8_t groupId,
              const char *reason) const;
};
static_assert(sizeof(MembershipDraft) <= 808,
              "Membership fits the existing Advanced typed union");
} // namespace LocalUi
