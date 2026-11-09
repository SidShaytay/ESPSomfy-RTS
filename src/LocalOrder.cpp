#include "LocalOrder.h"
#include <stdio.h>
#include <string.h>
namespace LocalUi {
namespace Ordering {
using namespace EntityAdmin;
namespace {
const char *title(Kind kind) {
  return kind == Kind::room    ? "rooms"
         : kind == Kind::shade ? "devices"
                               : "groups";
}
Notice fileNotice(ModelCommitState state) {
  return state == ModelCommitState::saved       ? Notice::saved
         : state == ModelCommitState::uncertain ? Notice::uncertain
                                                : Notice::appliedNotSaved;
}
} // namespace
void Form::close(Notice value) {
  memset(ids, 0, sizeof(ids));
  memset(rollback, 0, sizeof(rollback));
  memset(slots, 0, sizeof(slots));
  admitted = {};
  revision = activityAt = observedAt = 0;
  count = cursor = start = 0;
  dirty = normalization = false;
  state = State::closed;
  status = value;
}
bool Form::capture() {
  if (!backend.readOrder(kind, ids, slots, count, normalization) || count > 32)
    return false;
  const uint8_t capacity = kind == Kind::shade  ? SOMFY_MAX_SHADES
                           : kind == Kind::room ? SOMFY_MAX_ROOMS
                                                : SOMFY_MAX_GROUPS;
  if (count > capacity)
    return false;
  for (uint8_t i = 0; i < count; i++) {
    if (!ids[i] || ids[i] == 255 || slots[i] >= capacity)
      return false;
    for (uint8_t j = 0; j < i; j++)
      if (ids[j] == ids[i] || slots[j] == slots[i])
        return false;
  }
  return true;
}
bool Form::open(Kind value, uint64_t now) {
  discard();
  kind = value;
  if (kind != Kind::room && kind != Kind::shade && kind != Kind::group)
    return false;
  admitted = backend.authorization();
  revision = backend.revision();
  activityAt = observedAt = now;
  if (!admitted.allowed || !admitted.generation ||
      !admitted.configurationGeneration) {
    close(Notice::locked);
    return false;
  }
  if (!revision || !capture()) {
    close(Notice::changed);
    return false;
  }
  state = State::browse;
  return guard(now);
}
bool Form::guard(uint64_t now) {
  if (closed())
    return false;
  if (now < observedAt || now < activityAt || now - activityAt >= timeoutMs) {
    close(Notice::expired);
    return false;
  }
  observedAt = now;
  const auto current = backend.authorization();
  if (!current.allowed) {
    close(Notice::locked);
    return false;
  }
  if (!current.generation || !current.configurationGeneration ||
      current.generation != admitted.generation ||
      current.configurationGeneration != admitted.configurationGeneration ||
      !revision || backend.revision() != revision) {
    close(Notice::changed);
    return false;
  }
  return true;
}
void Form::touch(uint64_t now) {
  activityAt = now;
  backend.activity();
}
void Form::tick(uint64_t now) { guard(now); }
void Form::exchange(uint8_t first, uint8_t second) {
  const uint8_t id = ids[first], slot = slots[first];
  ids[first] = ids[second];
  slots[first] = slots[second];
  ids[second] = id;
  slots[second] = slot;
}
bool Form::refreshDirty() {
  OrderIds live{}, physical{};
  uint8_t total = 0;
  bool normalize = false;
  if (!backend.readOrder(kind, live, physical, total, normalize) ||
      total != count)
    return false;
  dirty = false;
  for (uint8_t i = 0; i < count; i++) {
    bool found = false;
    for (uint8_t j = 0; j < count; j++)
      if (ids[i] == live[j] && slots[i] == physical[j])
        found = true;
    if (!found)
      return false;
    dirty |= ids[i] != live[i];
  }
  normalization = normalize;
  return true;
}
void Form::turn(int direction, uint64_t now) {
  if (!direction || !guard(now))
    return;
  touch(now);
  if (state == State::browse) {
    if (direction > 0 && cursor < count + 1)
      ++cursor;
    if (direction < 0 && cursor)
      --cursor;
    status = Notice::none;
  } else if (state == State::move) {
    if (direction > 0 && cursor + 1 < count) {
      exchange(cursor, cursor + 1);
      ++cursor;
    }
    if (direction < 0 && cursor) {
      exchange(cursor, cursor - 1);
      --cursor;
    }
  } else if (state == State::confirm || state == State::result) {
    if (direction < 0)
      cursor = 0;
    if (direction > 0 && (state == State::confirm || status != Notice::saved))
      cursor = 1;
  }
}
void Form::refused(Admission admission) {
  if (admission == Admission::busy) {
    // A blocked COMMIT retains the applied result; it must never return to
    // a now-empty mutation draft and offer Save as though nothing was applied.
    if (state != State::result) {
      state = State::browse;
      cursor = count;
    } else
      cursor = 0;
    status = Notice::busy;
  } else if (admission == Admission::locked)
    close(Notice::locked);
  else if (admission == Admission::noMemory)
    close(Notice::noMemory);
  else
    close(Notice::changed);
}
void Form::save() {
  const auto result =
      backend.applyOrder(kind, ids, count, allocator, admitted, revision);
  if (result.admission != Admission::accepted) {
    refused(result.admission);
    return;
  }
  if (!result.edit.applied) {
    state = State::browse;
    cursor = count;
    status = Notice::failed;
    return;
  }
  // Discard all mutation intent after application, including failed file save.
  // Subsequent Retry is solely a checked COMMIT of the current live model.
  memset(ids, 0, sizeof(ids));
  memset(rollback, 0, sizeof(rollback));
  memset(slots, 0, sizeof(slots));
  count = cursor = start = 0;
  dirty = normalization = false;
  revision = backend.revision();
  if (!revision) {
    close(Notice::changed);
    return;
  }
  state = State::result;
  status = fileNotice(result.edit.persistence.state);
}
void Form::activate(uint64_t now) {
  if (!guard(now))
    return;
  touch(now);
  if (state == State::browse) {
    if (cursor < count) {
      memcpy(rollback, ids, sizeof(ids));
      start = cursor;
      state = State::move;
    } else if (cursor == count) {
      if (!dirty && !normalization) {
        close(Notice::unchanged);
        return;
      }
      state = State::confirm;
      cursor = 0;
      status = Notice::none;
    } else
      close(Notice::none);
  } else if (state == State::move) {
    if (!refreshDirty()) {
      close(Notice::changed);
      return;
    }
    memset(rollback, 0, sizeof(rollback));
    state = State::browse;
    status = Notice::none;
  } else if (state == State::confirm) {
    if (!cursor) {
      state = State::browse;
      cursor = count;
    } else
      save();
  } else if (state == State::result) {
    if (!cursor) {
      close(Notice::none);
      return;
    }
    const auto result = backend.retryOrderFile(admitted, revision);
    if (result.admission != Admission::accepted) {
      refused(result.admission);
      return;
    }
    status = fileNotice(result.file.state);
    cursor = 0;
  }
}
void Form::back(uint64_t now) {
  if (!guard(now))
    return;
  touch(now);
  if (state == State::move) {
    for (uint8_t i = 0; i < count; i++) {
      uint8_t found = i;
      while (found < count && ids[found] != rollback[i])
        ++found;
      if (found == count) {
        close(Notice::changed);
        return;
      }
      exchange(i, found);
    }
    memset(rollback, 0, sizeof(rollback));
    cursor = start;
    state = State::browse;
  } else if (state == State::confirm) {
    state = State::browse;
    cursor = count;
  } else
    close(Notice::none);
}
void Form::render(FormView &out) {
  out = {};
  out.kind = kind;
  out.notice = status;
  snprintf(out.title, sizeof(out.title), "Reorder %s", title(kind));
  if (closed()) {
    out.mode = Mode::closed;
    return;
  }
  out.dirty =
      dirty || normalization || (state == State::move && cursor != start);
  if (state == State::confirm) {
    out.mode = Mode::confirm;
    out.rowCount = out.total = 2;
    out.selected = cursor;
    strcpy(out.rows[0].label, "Cancel");
    strcpy(out.rows[1].label, "Save order");
    out.rows[cursor].selected = true;
    strcpy(out.hint, "Reassigns list positions only. No motor command.");
    return;
  }
  if (state == State::result) {
    out.mode = Mode::result;
    out.rowCount = out.total = status == Notice::saved ? 1 : 2;
    out.selected = cursor;
    strcpy(out.rows[0].label, "Close");
    if (out.rowCount == 2)
      strcpy(out.rows[1].label, "Retry model file");
    out.rows[cursor].selected = true;
    strcpy(out.hint, status == Notice::saved
                         ? "Order saved to model file."
                         : "Applied. Retry saves file only.");
    return;
  }
  out.mode = Mode::fields;
  out.total = count + (state == State::browse ? 2 : 0);
  out.selected = cursor;
  out.offset = cursor / 3 * 3;
  if (state == State::move)
    snprintf(out.title, sizeof(out.title), "Move #%u in %s",
             unsigned(ids[cursor]), title(kind));
  strcpy(out.hint, state == State::move ? "Turn: move Press: apply Back: undo"
                   : normalization      ? "Save numbers from 0. Back: discard"
                                        : "Press: pick/Save Back: discard");
  for (uint8_t at = out.offset; at < out.total && out.rowCount < 3; at++) {
    auto &row = out.rows[out.rowCount++];
    row.selected = at == cursor;
    if (at < count) {
      Value label;
      if (!backend.readOrderLabel(kind, slots[at], ids[at], label) ||
          label.kind != 's' || !memchr(label.text, 0, sizeof(label.text))) {
        close(Notice::changed);
        out = {};
        out.kind = kind;
        out.mode = Mode::closed;
        out.notice = status;
        return;
      }
      snprintf(row.label, sizeof(row.label), "#%u %s", unsigned(ids[at]),
               label.text);
      if (row.selected)
        out.id = ids[at];
    } else
      strcpy(row.label, at == count ? "Save order" : "Cancel");
  }
}
} // namespace Ordering
} // namespace LocalUi
