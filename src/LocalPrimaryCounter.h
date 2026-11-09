#pragma once
#include "ExplicitCounter.h"
#include "LocalEntityView.h"
#include "LocalValueEditor.h"
namespace LocalUi {
enum class PrimaryCounterEffect : uint8_t {
  none,
  leave,
  checkUnchanged,
  queueCorrection,
  queueFile
};
// Pure bounded draft/presentation state. The parent owns authority, exact
// target, FIFO ticket, actual persistence and the borrowed protected
// presentation frame.
class PrimaryCounterDraft {
  enum class Page : uint8_t {
    closed,
    current,
    edit,
    editActions,
    confirm,
    pending,
    result,
    refused
  };
  PrimaryCounterReview expected;
  ValueEditor editor;
  uint16_t proposed = 0;
  Page page = Page::closed;
  uint8_t cursor = 0;
  bool coarse = false, applied = false, fileSaved = false, retry = false;
  CounterDomainState outcome = CounterDomainState::invalid;

public:
  void open(const PrimaryCounterReview &);
  void clear();
  bool active() const { return page != Page::closed; }
  bool pending() const { return page == Page::pending; }
  bool fileOnly() const { return applied; }
  bool expired() const {
    return editing() && editor.outcome() == EditOutcome::expired;
  }
  bool editing() const {
    return page == Page::edit || page == Page::editActions;
  }
  const PrimaryCounterReview &review() const { return expected; }
  uint16_t requested() const { return proposed; }
  void tick(uint64_t now);
  void turn(int direction, uint64_t now);
  PrimaryCounterEffect activate(uint64_t now);
  PrimaryCounterEffect back(uint64_t now);
  void queued() {
    page = Page::pending;
    cursor = 0;
  }
  void busy();
  void complete(CounterDomainResult);
  void completeFile(ModelCommitResult);
  void blockedFile();
  void render(EntityAdmin::FormView &, uint8_t targetId) const;
};
static_assert(sizeof(PrimaryCounterDraft) <= 864,
              "Primary counter draft fits the existing Advanced slab");
} // namespace LocalUi
