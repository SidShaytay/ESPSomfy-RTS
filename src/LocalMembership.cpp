#include "LocalMembership.h"
#include <stdio.h>
#include <string.h>
namespace LocalUi {
using namespace Advanced;
using namespace EntityAdmin;
namespace {
constexpr char addWarning[] =
    "No RF; pairing unchanged. Group commands may affect device.";
constexpr char removeWarning[] =
    "No RF/unpair. Motor may still respond to this RF group.";
constexpr char sentinelWarning[] =
    "Clear ALL #255 placeholders only. No device/RF/pairing change.";
static_assert(sizeof(addWarning) <= 64 && sizeof(removeWarning) <= 64 &&
                  sizeof(sentinelWarning) <= 64,
              "Complete membership consequences fit borrowed hints");
} // namespace
void MembershipDraft::clearIntent() {
  chosen = {};
  list = {};
}
void MembershipDraft::clear() {
  clearIntent();
  page = Page::closed;
  cursor = returnCursor = 0;
  candidates = applied = retry = false;
  file = {};
}
bool MembershipDraft::open(bool adding, const MembershipRows &rows) {
  clear();
  if (rows.groupSlot >= 16 || rows.count > 32 || (!adding && rows.count != 32))
    return false;
  for (uint8_t i = 0; i < rows.count; i++) {
    if (adding) {
      if (!rows.ids[i] || rows.ids[i] > 32 || rows.slots[i] != 255 ||
          rows.devices[i] >= 32)
        return false;
      for (uint8_t j = 0; j < i; j++)
        if (rows.ids[i] == rows.ids[j] || rows.devices[i] == rows.devices[j])
          return false;
    } else if (rows.slots[i] != i ||
               ((!rows.ids[i] || rows.ids[i] == 255) && rows.devices[i] != 255))
      return false;
  }
  candidates = adding;
  list = rows;
  page = Page::browse;
  return true;
}
bool MembershipDraft::needsLabel(uint8_t index) const {
  return page == Page::browse && (!candidates || !list.full) &&
         index < list.count;
}
bool MembershipDraft::select(const MembershipSelection &value) {
  if (page != Page::browse || cursor >= list.count || !list.ids[cursor] ||
      (candidates && list.full))
    return false;
  if (value.groupSlot != list.groupSlot || value.id != list.ids[cursor] || value.slot != list.slots[cursor] ||
      value.deviceSlot != list.devices[cursor] || value.adding != candidates)
    return false;
  if (!memchr(value.label, 0, sizeof(value.label)) ||
      !memchr(value.groupIdentity, 0, sizeof(value.groupIdentity)) ||
      !memchr(value.deviceIdentity, 0, sizeof(value.deviceIdentity)))
    return false;
  if (value.id == 255) {
    if (value.adding || value.hasCandidate || value.candidate.ref.id || value.candidate.targetExists)
      return false;
  } else {
    if (!value.hasCandidate || !value.candidate.valid() ||
        value.candidate.ref != EntityRef{EntityKind::device, value.id})
      return false;
    if (value.adding && (!value.candidate.targetExists || value.id > 32))
      return false;
  }
  chosen = value;
  returnCursor = cursor;
  cursor = 0;
  page = Page::confirm;
  return true;
}
void MembershipDraft::turn(int direction) {
  if (!active() || !direction)
    return;
  uint8_t count = page == Page::browse
                      ? candidates ? (list.full || !list.count ? 1 : list.count)
                                   : list.count + 1
                  : page == Page::pending ? 1
                  : page == Page::result  ? (retry ? 2 : 1)
                                          : 2;
  cursor = (cursor + (direction > 0 ? 1 : count - 1)) % count;
}
MembershipEffect MembershipDraft::activate() {
  switch (page) {
  case Page::closed:
    return MembershipEffect::none;
  case Page::browse:
    if (!candidates && cursor == list.count)
      return MembershipEffect::readCandidates;
    return cursor < list.count && list.ids[cursor] &&
                   (!candidates || !list.full)
               ? MembershipEffect::select
               : MembershipEffect::none;
  case Page::confirm:
    if (!cursor) {
      chosen = {};
      cursor = returnCursor;
      page = Page::browse;
      return MembershipEffect::discardReview;
    }
    return MembershipEffect::queueChange;
  case Page::pending:
    return MembershipEffect::leave;
  case Page::result:
    return retry && !cursor ? MembershipEffect::queueFile
                            : MembershipEffect::leave;
  case Page::fault:
    if (!cursor)
      return MembershipEffect::leave;
    page = applied          ? Page::result
           : hasSelection() ? Page::confirm
                            : Page::browse;
    cursor = applied && retry ? 1 : 0;
    return MembershipEffect::none;
  }
  return MembershipEffect::none;
}
MembershipEffect MembershipDraft::back() {
  if (page == Page::confirm) {
    chosen = {};
    cursor = returnCursor;
    page = Page::browse;
    return MembershipEffect::discardReview;
  }
  if (page == Page::browse && candidates)
    return MembershipEffect::readMembers;
  return MembershipEffect::leave;
}
void MembershipDraft::queued() {
  page = Page::pending;
  cursor = 0;
}
void MembershipDraft::busy() {
  page = Page::fault;
  cursor = 0;
}
MembershipCompletion MembershipDraft::complete(const edit_result_t &result) {
  if (result.applied) {
    applied = true;
    file = result.persistence;
    retry = !file.saved();
    clearIntent();
    page = Page::result;
    cursor = retry ? 1 : 0;
    return MembershipCompletion::applied;
  }
  if (result.ok && result.noChange) {
    clear();
    return MembershipCompletion::unchanged;
  }
  busy();
  return MembershipCompletion::refused;
}
void MembershipDraft::fileComplete(ModelCommitResult result) {
  file = result;
  retry = !file.saved();
  page = Page::result;
  cursor = retry ? 1 : 0;
}
void MembershipDraft::render(FormView &out, uint8_t id,
                             const char *reason) const {
  out = {};
  out.id = id;
  out.kind = Kind::group;
  out.mode = Mode::fields;
  out.selected = cursor;
  snprintf(out.title, sizeof(out.title), "#%u Group members", unsigned(id));
  if (page == Page::browse) {
    out.offset = cursor / 3 * 3;
    out.total = candidates ? (list.full || !list.count ? 1 : list.count)
                           : list.count + 1;
    if (candidates && (list.full || !list.count)) {
      out.rowCount = 1;
      strcpy(out.rows[0].label,
             list.full ? "Group is full" : "No available devices");
      strcpy(out.hint, list.full ? "Remove an entry or unused slot first."
                                 : "Back: controller member slots");
    } else {
      strcpy(out.hint, candidates ? "Choose device; no RF on selection."
                                  : "List only; pairing unchanged.");
      for (uint8_t i = out.offset; i < out.total && out.rowCount < 3; i++) {
        auto &row = out.rows[out.rowCount++];
        if (!candidates && i == list.count)
          strcpy(row.label, "Add device (metadata)");
      }
    }
  } else if (page == Page::confirm) {
    out.mode = Mode::membershipReview;
    out.rowCount = out.total = 2;
    strcpy(out.rows[0].label, "Cancel");
    snprintf(out.rows[1].label, sizeof(out.rows[1].label),
             chosen.adding ? "Add #%u to controller list"
                           : "Remove ALL #%u entries",
             unsigned(chosen.id));
    strcpy(out.rows[0].value, chosen.groupIdentity);
    strcpy(out.rows[1].value, chosen.deviceIdentity);
    strcpy(out.rows[2].label, chosen.label);
    strcpy(out.hint, chosen.adding      ? addWarning
                     : chosen.id == 255 ? sentinelWarning
                                        : removeWarning);
  } else if (page == Page::pending) {
    out.mode = Mode::result;
    out.rowCount = out.total = 1;
    strcpy(out.title, applied ? "File save queued" : "Membership queued");
    strcpy(out.rows[0].label, "Cancel queued work");
    strcpy(out.hint,
           applied
               ? "Current file only; no membership replay. FIFO expires in 5s."
               : "FIFO expires in 5s. Fresh context checked before edit.");
  } else if (page == Page::fault) {
    out.mode = Mode::validation;
    out.rowCount = out.total = 2;
    strcpy(out.title, "Membership unavailable");
    strcpy(out.rows[0].label, applied ? "Close result" : "Cancel edit");
    strcpy(out.rows[1].label,
           applied ? "Back to file result" : "Back to review");
    snprintf(out.hint, sizeof(out.hint), "%s",
             reason && *reason ? reason
             : applied ? "List already changed. File busy; no automatic retry."
                       : "Membership refused. No change was applied.");
  } else if (page == Page::result) {
    out.mode = Mode::result;
    out.rowCount = out.total = retry ? 2 : 1;
    strcpy(out.rows[0].label, retry ? "Retry model file save" : "Close");
    if (retry)
      strcpy(out.rows[1].label, "Close");
    strcpy(out.title, file.saved() ? "Membership saved"
                      : file.state == ModelCommitState::uncertain
                          ? "File save uncertain"
                          : "File save failed");
    strcpy(
        out.hint,
        file.saved() ? "Controller list/model file verified. Pairing unchanged."
        : file.state == ModelCommitState::uncertain
            ? "List applied; file uncertain. Retry file only; no edit replay."
            : "List applied; file failed. Retry file only; never repeat edit.");
  }
  for (uint8_t i = 0; i < out.rowCount; i++)
    out.rows[i].selected = out.offset + i == out.selected;
}
} // namespace LocalUi
