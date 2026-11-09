#include "LocalAdvanced.h"
#include "LocalJsonAllocator.h"
#include <new>
#include <stdio.h>
#include <string.h>
namespace LocalUi {
namespace Advanced {
using namespace EntityAdmin;
struct Form::Session {
  enum class Page : uint8_t { picker, menu, list, confirm, result, refused };
  Context context;
  OrderIds ids{}, slots{};
  Authorization auth;
  uint64_t revision = 0, observedAt = 0, activityAt = 0;
  Target target;
  Action action;
  Page page = Page::picker, returnPage = Page::menu;
  char refusal[64]{};
  uint8_t count = 0, cursor = 0, selectedSlot = 0;
  bool paired = false;
};
uint64_t Form::epoch() const { return session ? session->revision : 0; }
void Form::close(Notice n) {
  if (session) {
    session->~Session();
    allocator.deallocate(session);
    session = nullptr;
  }
  status = n;
}
void Form::touch(uint64_t now) {
  session->activityAt = now;
  backend.activity();
}
bool Form::guard(uint64_t now) {
  if (!session)
    return false;
  auto &s = *session;
  if (now < s.observedAt || now - s.activityAt >= timeoutMs) {
    close(Notice::expired);
    return false;
  }
  s.observedAt = now;
  const auto auth = backend.authorization();
  if (!auth.allowed) {
    close(Notice::locked);
    return false;
  }
  if (auth.generation != s.auth.generation ||
      auth.configurationGeneration != s.auth.configurationGeneration ||
      backend.revision() != s.revision) {
    close(Notice::changed);
    return false;
  }
  if (s.page != Session::Page::picker) {
    Context current;
    if (!backend.capture(s.target, current) || !s.context.same(current)) {
      close(Notice::changed);
      return false;
    }
  }
  return true;
}
bool Form::open(Scope scope, uint64_t now) {
  discard();
  const auto auth = backend.authorization();
  const auto revision = backend.revision();
  if (!auth.allowed || !auth.generation || !auth.configurationGeneration) {
    close(Notice::locked);
    return false;
  }
  if (!revision || (scope != Scope::devices && scope != Scope::groups &&
                    scope != Scope::repeaters)) {
    close(Notice::changed);
    return false;
  }
  static_assert(
      sizeof(Session) + JsonAllocator::allocationOverhead <= 2304,
      "Bounded lazy Advanced slab, including wiping allocator header");
  void *memory = allocator.allocate(sizeof(Session));
  if (!memory) {
    close(Notice::noMemory);
    return false;
  }
  session = new (memory) Session;
  auto &s = *session;
  s.auth = auth;
  s.revision = revision;
  s.target.scope = scope;
  s.observedAt = s.activityAt = now;
  if (scope == Scope::repeaters) {
    s.page = Session::Page::list;
    if (!backend.capture(s.target, s.context)) {
      close(Notice::changed);
      return false;
    }
  } else if (!backend.targets(scope, s.ids, s.slots, s.count) || s.count > 32) {
    close(Notice::changed);
    return false;
  }
  for (uint8_t i = 0; i < s.count; i++) {
    if (!s.ids[i] || s.ids[i] == 255 || s.slots[i] >= 32) {
      close(Notice::changed);
      return false;
    }
    for (uint8_t j = 0; j < i; j++)
      if (s.ids[i] == s.ids[j] || s.slots[i] == s.slots[j]) {
        close(Notice::changed);
        return false;
      }
  }
  return guard(now);
}
bool Form::select() {
  auto &s = *session;
  if (s.cursor >= s.count)
    return false;
  s.target.id = s.ids[s.cursor];
  s.selectedSlot = s.slots[s.cursor];
  if (!backend.capture(s.target, s.context))
    return false;
  if (s.target.scope == Scope::devices) {
    if (!backend.paired(s.target, s.paired))
      return false;
    s.page = Session::Page::menu;
  } else
    s.page = Session::Page::list;
  s.cursor = 0;
  return true;
}
void Form::turn(int direction, uint64_t now) {
  if (!guard(now) || !direction)
    return;
  touch(now);
  auto &s = *session;
  uint8_t count = 0;
  switch (s.page) {
  case Session::Page::picker:
    count = s.count;
    break;
  case Session::Page::menu:
  case Session::Page::confirm:
  case Session::Page::result:
  case Session::Page::refused:
    count = 2;
    break;
  case Session::Page::list:
    count = backend.itemCount(s.target);
    break;
  }
  if (count)
    s.cursor = (s.cursor + (direction > 0 ? 1 : count - 1)) % count;
}
void Form::refused(Admission a, const char *reason) {
  if (a == Admission::noMemory) {
    close(Notice::noMemory);
    return;
  }
  if (a == Admission::locked) {
    close(Notice::locked);
    return;
  }
  if (a == Admission::stale) {
    close(Notice::changed);
    return;
  }
  status = a == Admission::busy ? Notice::busy : Notice::failed;
  if (session->page != Session::Page::result) {
    session->page = Session::Page::refused;
    snprintf(session->refusal, sizeof(session->refusal), "%.63s",
             reason && *reason ? reason
             : a == Admission::busy
                 ? "Busy. Wait for queued or radio work to finish."
                 : "Configuration refused. Review current state.");
  }
  session->cursor = 0;
}
void Form::apply() {
  auto &s = *session;
  const auto result = backend.apply(s.target, s.action, s.context, s.auth,
                                    s.revision, allocator);
  if (result.admission != Admission::accepted) {
    refused(result.admission, result.reason);
    return;
  }
  if (!result.edit.applied) {
    refused(Admission::invalid);
    return;
  }
  if (result.edit.persistence.saved()) {
    close(Notice::saved);
    return;
  }
  status = result.edit.persistence.state == ModelCommitState::uncertain
               ? Notice::uncertain
               : Notice::appliedNotSaved;
  // The mutation has happened. Erase every executable action and picker ID;
  // only fresh current context/history and a COMMIT-only result remain.
  s.action = {};
  memset(s.ids, 0, sizeof(s.ids));
  memset(s.slots, 0, sizeof(s.slots));
  s.count = 0;
  s.revision = backend.revision();
  if (!backend.capture(s.target, s.context)) {
    close(Notice::changed);
    return;
  }
  s.page = Session::Page::result;
  s.cursor = 1;
}
void Form::activate(uint64_t now) {
  if (!guard(now))
    return;
  touch(now);
  auto &s = *session;
  switch (s.page) {
  case Session::Page::picker:
    if (!s.count) {
      close(Notice::none);
      return;
    }
    if (!select())
      close(Notice::changed);
    break;
  case Session::Page::menu:
    if (s.cursor) {
      s.page = Session::Page::list;
      s.cursor = 0;
      break;
    }
    s.action = {};
    s.action.kind = ActionKind::setPaired;
    s.action.paired = !s.paired;
    s.returnPage = Session::Page::menu;
    s.page = Session::Page::confirm;
    s.cursor = 0;
    break;
  case Session::Page::list: {
    if (!backend.itemCount(s.target)) {
      close(Notice::none);
      return;
    }
    Item item;
    if (!backend.itemAt(s.target, s.cursor, item)) {
      close(Notice::changed);
      return;
    }
    if (s.target.scope == Scope::groups && !item.missing) {
      status = Notice::invalid;
      break;
    }
    s.action = {};
    s.action.key = item.key;
    s.action.slot = item.slot;
    s.action.kind =
        s.target.scope == Scope::repeaters ? ActionKind::unlinkRepeater
        : s.target.scope == Scope::groups  ? ActionKind::removeMissingMember
                                           : ActionKind::unlinkRemote;
    s.returnPage = Session::Page::list;
    s.page = Session::Page::confirm;
    s.cursor = 0;
    break;
  }
  case Session::Page::confirm:
    if (!s.cursor) {
      s.action = {};
      s.page = s.returnPage;
      s.cursor = 0;
      status = Notice::none;
    } else
      apply();
    break;
  case Session::Page::refused:
    if (s.cursor) {
      close(Notice::none);
      return;
    }
    s.page = Session::Page::confirm;
    s.cursor = 0;
    status = Notice::none;
    break;
  case Session::Page::result: {
    if (s.cursor) {
      close(Notice::none);
      return;
    }
    const auto result =
        backend.retryFile(s.target, s.context, s.auth, s.revision);
    if (result.admission != Admission::accepted) {
      refused(result.admission);
      if (session)
        session->cursor = 1;
      return;
    }
    if (result.file.saved()) {
      close(Notice::saved);
      return;
    }
    status = result.file.state == ModelCommitState::uncertain
                 ? Notice::uncertain
                 : Notice::appliedNotSaved;
    s.cursor = 1;
    break;
  }
  }
}
void Form::back(uint64_t now) {
  if (!guard(now))
    return;
  touch(now);
  auto &s = *session;
  if (s.page == Session::Page::refused) {
    s.page = Session::Page::confirm;
    s.cursor = 0;
    status = Notice::none;
  } else if (s.page == Session::Page::confirm) {
    s.action = {};
    s.page = s.returnPage;
    s.cursor = 0;
    status = Notice::none;
  } else if (s.page == Session::Page::list &&
             s.target.scope == Scope::devices) {
    s.page = Session::Page::menu;
    s.cursor = 1;
    status = Notice::none;
  } else
    close(Notice::none);
}
void Form::render(FormView &out) {
  out = {};
  out.notice = status;
  if (!session)
    return;
  auto &s = *session;
  out.id = s.target.id;
  out.kind = s.target.scope == Scope::groups ? Kind::group : Kind::shade;
  out.mode = Mode::fields;
  out.selected = s.cursor;
  if (s.target.scope == Scope::repeaters)
    strcpy(out.title, "Repeaters");
  else if (s.page == Session::Page::picker)
    strcpy(out.title, s.target.scope == Scope::devices ? "Advanced devices"
                                                       : "Advanced groups");
  else {
    Choice label;
    if (!backend.targetLabel(s.target, s.selectedSlot, label) ||
        label.value != s.target.id ||
        !memchr(label.label, 0, sizeof(label.label))) {
      close(Notice::changed);
      out = {};
      out.notice = status;
      return;
    }
    snprintf(out.title, sizeof(out.title), "%s", label.label);
  }
  strcpy(out.hint, "Press: choose   Back: cancel");
  if (s.page == Session::Page::refused) {
    out.mode = Mode::validation;
    out.total = out.rowCount = 2;
    strcpy(out.rows[0].label, "Back to review");
    strcpy(out.rows[1].label, "Close");
    snprintf(out.hint, sizeof(out.hint), "%s", s.refusal);
  } else if (s.page == Session::Page::confirm) {
    out.mode = Mode::confirm;
    out.total = out.rowCount = 2;
    strcpy(out.rows[0].label, "Cancel");
    if (s.action.kind == ActionKind::setPaired) {
      strcpy(out.rows[1].label,
             s.action.paired ? "Mark paired" : "Mark unpaired");
      strcpy(out.hint, "Marker only. Motor pairing is unchanged.");
    } else {
      snprintf(out.rows[1].label, sizeof(out.rows[1].label), "Remove #%lu",
               static_cast<unsigned long>(s.action.key));
      strcpy(out.hint, s.action.kind == ActionKind::removeMissingMember
                           ? "Remove all stale links. No physical unpair."
                       : s.action.kind == ActionKind::unlinkRepeater
                           ? "Stop future repeats. No physical unpair."
                           : "Remove association. No physical unpair.");
    }
  } else if (s.page == Session::Page::result) {
    out.mode = Mode::result;
    out.total = out.rowCount = 2;
    strcpy(out.rows[0].label, "Retry model save");
    strcpy(out.rows[1].label, "Close");
    strcpy(out.hint, "Applied already. Retry saves the model file only.");
  } else if (s.page == Session::Page::menu) {
    out.total = out.rowCount = 2;
    strcpy(out.rows[0].label, "Paired marker");
    strcpy(out.rows[0].value, s.paired ? "Paired" : "Unpaired");
    strcpy(out.rows[1].label, "Linked remotes");
  } else {
    out.total =
        s.page == Session::Page::picker ? s.count : backend.itemCount(s.target);
    out.offset = s.cursor / 3 * 3;
    if (!out.total) {
      out.total = out.rowCount = 1;
      strcpy(out.rows[0].label, "Close (empty list)");
    } else
      for (uint8_t i = out.offset; i < out.total && out.rowCount < 3; i++) {
        auto &row = out.rows[out.rowCount++];
        if (s.page == Session::Page::picker) {
          Choice label;
          if (!backend.targetLabel({s.target.scope, s.ids[i]}, s.slots[i],
                                   label) ||
              label.value != s.ids[i] ||
              !memchr(label.label, 0, sizeof(label.label))) {
            close(Notice::changed);
            out = {};
            out.notice = status;
            return;
          }
          snprintf(row.label, sizeof(row.label), "%s", label.label);
        } else {
          Item item;
          if (!backend.itemAt(s.target, i, item) ||
              !memchr(item.label, 0, sizeof(item.label))) {
            close(Notice::changed);
            out = {};
            out.notice = status;
            return;
          }
          snprintf(row.label, sizeof(row.label), "%s", item.label);
          if (s.target.scope == Scope::devices)
            snprintf(row.value, sizeof(row.value), "Code %u",
                     unsigned(item.rollingCode));
          if (s.target.scope == Scope::groups)
            strcpy(row.value, item.missing ? "Missing: repair" : "View only");
        }
      }
  }
  for (uint8_t i = 0; i < out.rowCount; i++)
    out.rows[i].selected = out.offset + i == out.selected;
}
} // namespace Advanced
} // namespace LocalUi
