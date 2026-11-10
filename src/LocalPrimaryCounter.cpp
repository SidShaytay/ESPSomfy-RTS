#include "LocalPrimaryCounter.h"
#include <stdio.h>
#include <string.h>
namespace LocalUi {
using namespace EntityAdmin;
void PrimaryCounterDraft::clear() {
  editor.cancel();
  expected = {};
  proposed = 0;
  page = Page::closed;
  cursor = 0;
  coarse = applied = fileSaved = retry = false;
  outcome = CounterDomainState::invalid;
}
void PrimaryCounterDraft::open(const PrimaryCounterReview &value) {
  clear();
  expected = value;
  proposed = value.primary;
  page = Page::current;
}
void PrimaryCounterDraft::tick(uint64_t now) {
  if (editing())
    editor.tick(now);
}
void PrimaryCounterDraft::turn(int direction, uint64_t now) {
  if (!active() || !direction)
    return;
  if (page == Page::edit) {
    editor.rotate(direction, now);
    return;
  }
  if (page == Page::editActions && !editor.setStep(coarse ? 1000 : 1, now))
    return;
  uint8_t count = page == Page::editActions ? 3
                  : page == Page::result    ? (retry ? 2 : 1)
                  : page == Page::pending   ? 1
                                            : 2;
  cursor = (cursor + (direction > 0 ? 1 : count - 1)) % count;
}
PrimaryCounterEffect PrimaryCounterDraft::activate(uint64_t now) {
  switch (page) {
  case Page::closed:
    return PrimaryCounterEffect::none;
  case Page::current:
    if (!cursor)
      return PrimaryCounterEffect::leave;
    if (!editor.beginInteger(proposed, 0, 65535, coarse ? 1000 : 1, now))
      return PrimaryCounterEffect::leave;
    page = Page::edit;
    cursor = 0;
    return PrimaryCounterEffect::none;
  case Page::edit:
    if (!editor.setStep(coarse ? 1000 : 1, now))
      return PrimaryCounterEffect::leave;
    page = Page::editActions;
    cursor = 0;
    return PrimaryCounterEffect::none;
  case Page::editActions:
    if (!cursor) {
      editor.cancel();
      page = Page::current;
      cursor = 0;
      return PrimaryCounterEffect::none;
    }
    if (cursor == 2) {
      coarse = !coarse;
      if (!editor.setStep(coarse ? 1000 : 1, now))
        return PrimaryCounterEffect::leave;
      page = Page::edit;
      cursor = 0;
      return PrimaryCounterEffect::none;
    }
    if (editor.apply(now) != EditOutcome::applied)
      return PrimaryCounterEffect::leave;
    {
      int64_t value;
      if (!editor.takeNumber(value))
        return PrimaryCounterEffect::leave;
      proposed = uint16_t(value);
    }
    page = Page::confirm;
    cursor = 0;
    return expected.reconciled(proposed) ? PrimaryCounterEffect::checkUnchanged
                                         : PrimaryCounterEffect::none;
  case Page::confirm:
    if (!cursor) {
      proposed = expected.primary;
      page = Page::current;
      return PrimaryCounterEffect::none;
    }
    return PrimaryCounterEffect::queueCorrection;
  case Page::pending:
    return PrimaryCounterEffect::leave;
  case Page::refused:
    if (!cursor)
      return PrimaryCounterEffect::leave;
    page = applied ? Page::result : Page::confirm;
    cursor = applied && retry ? 1 : 0;
    return PrimaryCounterEffect::none;
  case Page::result:
    return retry && !cursor ? PrimaryCounterEffect::queueFile
                            : PrimaryCounterEffect::leave;
  }
  return PrimaryCounterEffect::leave;
}
PrimaryCounterEffect PrimaryCounterDraft::back(uint64_t now) {
  if (page == Page::editActions) {
    if (!editor.setStep(coarse ? 1000 : 1, now))
      return PrimaryCounterEffect::leave;
    page = Page::edit;
    cursor = 0;
    return PrimaryCounterEffect::none;
  }
  if (page == Page::edit) {
    editor.cancel();
    page = Page::current;
    cursor = 0;
    return PrimaryCounterEffect::none;
  }
  if (page == Page::confirm) {
    proposed = expected.primary;
    page = Page::current;
    cursor = 0;
    return PrimaryCounterEffect::none;
  }
  return PrimaryCounterEffect::leave;
}
void PrimaryCounterDraft::busy() {
  page = Page::refused;
  cursor = 0;
}
void PrimaryCounterDraft::complete(CounterDomainResult result) {
  outcome = result.state;
  applied = result.applied;
  fileSaved = applied && result.file.saved();
  retry = applied && !result.file.saved();
  page = Page::result;
  cursor = retry ? 1 : 0;
  // Once persistence was attempted, no executable counter draft survives.
  editor.cancel();
  expected = {};
  proposed = 0;
}
void PrimaryCounterDraft::completeFile(ModelCommitResult file) {
  fileSaved = file.saved();
  retry = !fileSaved;
  page = Page::result;
  cursor = retry ? 1 : 0;
  outcome = fileSaved ? CounterDomainState::saved
            : file.state == ModelCommitState::uncertain
                ? CounterDomainState::modelUncertain
                : CounterDomainState::modelFailed;
}
void PrimaryCounterDraft::blockedFile() {
  retry = false;
  outcome = CounterDomainState::quarantined;
  page = Page::result;
  cursor = 0;
}
void PrimaryCounterDraft::render(FormView &out, uint8_t id) const {
  out = {};
  out.id = id;
  out.kind = Kind::shade;
  snprintf(out.title, sizeof(out.title), "#%u Primary code", unsigned(id));
  out.mode = Mode::counterReview;
  out.selected = cursor;
  out.rowCount = 2;
  out.total = 2;
  strcpy(out.rows[0].label, "Cancel");
  snprintf(out.rows[2].label, sizeof(out.rows[2].label), "#%lu Live %u",
           static_cast<unsigned long>(expected.address),
           unsigned(expected.primary));
  if (expected.stored.present)
    snprintf(out.rows[2].value, sizeof(out.rows[2].value),
             "Stored %u; %u matching aliases", unsigned(expected.stored.code),
             expected.aliasCount());
  else
    snprintf(out.rows[2].value, sizeof(out.rows[2].value),
             "Stored missing; %u matching aliases", expected.aliasCount());
  if (page == Page::current) {
    strcpy(out.rows[1].label, "Edit primary code");
    strcpy(out.hint, "Explicit edit changes ALL matching counter aliases.");
  } else if (page == Page::confirm) {
    snprintf(out.rows[1].label, sizeof(out.rows[1].label), "Set code: %u",
             unsigned(proposed));
    strcpy(out.hint,
           "Writes ALL aliases/storage/model. Deliberate lowering; no RF.");
  } else if (page == Page::edit) {
    out.mode = Mode::adjust;
    out.rowCount = out.total = 3;
    strcpy(out.rows[0].label, "Primary code draft");
    editor.preview(out.rows[0].value, sizeof(out.rows[0].value));
    strcpy(out.rows[1].label, "Step");
    snprintf(out.rows[1].value, sizeof(out.rows[1].value), "%u",
             coarse ? 1000 : 1);
    strcpy(out.rows[2].label, "Press: draft actions");
    out.rows[2].value[0] = 0;
    strcpy(out.hint, "Rotate draft. Back cancels edit.");
  } else if (page == Page::editActions) {
    out.mode = Mode::draft;
    out.rowCount = out.total = 3;
    strcpy(out.rows[0].label, "Cancel edit");
    strcpy(out.rows[1].label, "Apply to draft");
    snprintf(out.rows[2].label, sizeof(out.rows[2].label), "Step: %u",
             coarse ? 1 : 1000);
    out.rows[2].value[0] = 0;
    strcpy(out.hint, "Draft only; confirmation follows.");
  } else if (page == Page::pending) {
    strcpy(out.title, applied ? "File save queued" : "Correction queued");
    out.mode = Mode::result;
    out.rowCount = out.total = 1;
    strcpy(out.rows[0].label, "Cancel queued work");
    strcpy(
        out.hint,
        applied
            ? "File only; FIFO expires in 5s. No counter rewrite/replay."
            : "FIFO expires in 5s; fresh authority/code checks before write.");
  } else if (page == Page::refused) {
    strcpy(out.title, "Counter work busy");
    out.mode = Mode::validation;
    strcpy(out.rows[0].label, applied ? "Close result" : "Cancel correction");
    strcpy(out.rows[1].label,
           applied ? "Back to file result" : "Back to review");
    strcpy(out.hint,
           applied
               ? "Counter already applied. File busy; no automatic retry."
               : "Queue/owner/radio/restart unavailable. No automatic retry.");
  } else if (page == Page::result) {
    out.mode = Mode::result;
    out.rowCount = out.total = retry ? 2 : 1;
    strcpy(out.rows[0].label, retry ? "Retry model file save" : "Close");
    if (retry)
      strcpy(out.rows[1].label, "Close");
    if (applied && fileSaved && outcome == CounterDomainState::saved) {
      strcpy(out.title, "Counter saved");
      strcpy(out.hint,
             "ALL aliases/storage/model verified. Correction sent no RF.");
    } else if (applied && retry) {
      strcpy(out.title, "Code set; file not saved");
      strcpy(out.hint,
             "Counter applied. Retry file only; never repeat correction.");
    } else if (outcome == CounterDomainState::nvsUncertain) {
      strcpy(out.title, "Counter uncertain");
      strcpy(out.hint,
             "Stored code unknown; running code unchanged. No replay/reset.");
    } else if (outcome == CounterDomainState::quarantined) {
      strcpy(out.title, "Counter blocked");
      strcpy(out.hint,
             applied ? "Earlier correction kept. Counter recovery is blocked."
                     : "Counter recovery required. No correction was applied.");
    } else if (outcome == CounterDomainState::stale) {
      strcpy(out.title, "Counter review changed");
      strcpy(out.hint,
             "Close and reopen primary code to review fresh live/NVS state.");
    } else {
      strcpy(out.title, "Counter not changed");
      strcpy(
          out.hint,
          outcome == CounterDomainState::nvsFailed
              ? "Counter storage read/open failed. Nothing was written."
              : "Correction not verified. No success reported; close/review.");
    }
  }
  for (uint8_t i = 0; i < out.rowCount; i++)
    out.rows[i].selected = i == cursor;
}
} // namespace LocalUi
