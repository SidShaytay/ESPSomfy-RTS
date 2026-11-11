#include "LocalEntityForm.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
namespace LocalUi {
namespace EntityAdmin {
namespace {
void copy(char *to, size_t size, const char *text) {
  snprintf(to, size, "%s", text ? text : "");
}
const char *title(Kind kind) {
  return kind == Kind::room ? "Room" : kind == Kind::shade ? "Shade" : "Group";
}
const char *label(proto_field_t field) { return entityFieldLabel(field); }
bool validStored(const entity_field_t &f, const Value &value) {
  if (value.kind != f.schema().kind)
    return false;
  if (value.kind == 's')
    return strlen(value.text) <= uint64_t(f.hi) &&
           ValueEditor::validUtf8(value.text);
  if (value.kind == 'b')
    return true;
  if (value.integer < f.lo || value.integer > f.hi)
    return false;
  if (!f.choiceCount)
    return true;
  for (uint8_t i = 0; i < f.choiceCount; i++)
    if (f.choices[i].value == value.integer)
      return true;
  return false;
}
void format(const entity_field_t &f, const Value &value, char *to,
            size_t size) {
  if (value.kind == 's')
    copy(to, size, validStored(f, value) ? value.text : "Invalid UTF-8");
  else if (value.kind == 'b')
    copy(to, size, value.boolean ? "On" : "Off");
  else
    snprintf(to, size, validStored(f, value) ? "%lld" : "Invalid (%lld)",
             (long long)value.integer);
}
Notice admissionNotice(Admission a) {
  return a == Admission::locked  ? Notice::locked
         : a == Admission::stale ? Notice::changed
                                 : Notice::busy;
}
const entity_field_t *choiceField(Kind kind,uint8_t source,proto_field_t field) {
  switch(kind) {
  case Kind::room:return entityField(source?model_edit_type_t::save_room:model_edit_type_t::add_room,field);
  case Kind::shade:return entityField(source?model_edit_type_t::save_shade:model_edit_type_t::add_shade,field);
  case Kind::group:return entityField(source?model_edit_type_t::save_group:model_edit_type_t::add_group,field);
  }
  return nullptr;
}
} // namespace
uint8_t Backend::choiceCount(Kind kind, uint8_t source, proto_field_t field) {
  const auto *f = choiceField(kind,source,field);
  return f ? f->choiceCount : 0;
}
bool Backend::choiceAt(Kind kind, uint8_t source, proto_field_t field, uint8_t index, Choice &out) {
  out = {};
  const auto *f = choiceField(kind,source,field);
  if (!f || index >= f->choiceCount) return false;
  out.value = f->choices[index].value;
  snprintf(out.label, sizeof(out.label), "%s", f->choices[index].label);
  return true;
}
bool Form::choice(const entity_field_t &f, uint8_t index, Choice &out) {
  out = {};
  return backend.choiceAt(kind, sourceId, f.field, index, out) &&
         memchr(out.label, 0, sizeof(out.label)) && out.value >= f.lo && out.value <= f.hi;
}
bool Form::validOriginal(const entity_field_t &f, const Value &v) {
  if (!validStored(f, v)) return false;
  const auto count = backend.choiceCount(kind, sourceId, f.field);
  if (!count || v.kind != 'i') return true;
  for (uint8_t i = 0; i < count; i++) {
    Choice item;
    if (!choice(f, i, item)) { close(Notice::changed); return false; }
    if (item.value == v.integer) return true;
  }
  return false;
}
void Form::formatValue(const entity_field_t &f, const Value &v, char *out, size_t size) {
  format(f, v, out, size);
  const auto count = backend.choiceCount(kind, sourceId, f.field);
  if (!count || v.kind != 'i' || !validStored(f, v)) return;
  for (uint8_t i = 0; i < count; i++) {
    Choice item;
    if (!choice(f, i, item)) { close(Notice::changed); return; }
    if (item.value == v.integer) { copy(out, size, item.label); return; }
  }
  snprintf(out, size, "Invalid (%lld)", (long long)v.integer);
}
Form::Form(Backend &b, ArduinoJson::Allocator &a)
    : backend(b), allocator(a), changes(&a) {}
Form::~Form() { discard(); }
model_edit_type_t Form::type(bool remove) const {
  if (kind == Kind::room)
    return remove     ? model_edit_type_t::delete_room
           : sourceId ? model_edit_type_t::save_room
                      : model_edit_type_t::add_room;
  if (kind == Kind::shade)
    return remove     ? model_edit_type_t::delete_shade
           : sourceId ? model_edit_type_t::save_shade
                      : model_edit_type_t::add_shade;
  return remove     ? model_edit_type_t::delete_group
         : sourceId ? model_edit_type_t::save_group
                    : model_edit_type_t::add_group;
}
const edit_form_t *Form::metadata() const { return editForm(type()); }
void Form::close(Notice n) {
  changes.clear();
  editor.cancel();
  admitted = {};
  admittedRevision = activityAt = observedAt = 0;
  sourceId = id = selected = editing = stepPower = 0;
  deleting = validationField = false;
  memset(validationReason, 0, sizeof(validationReason));
  frame = {};
  frame.notice = n;
}
void Form::touch(uint64_t now) {
  activityAt = observedAt = now;
  backend.activity();
}
bool Form::guard(uint64_t now) {
  if (closed())
    return false;
  const auto auth = backend.authorization();
  if (!auth.allowed) {
    close(Notice::locked);
    return false;
  }
  if (auth.generation != admitted.generation ||
      auth.configurationGeneration != admitted.configurationGeneration ||
      backend.revision() != admittedRevision) {
    close(Notice::changed);
    return false;
  }
  if (now < observedAt || now < activityAt || now - activityAt >= timeoutMs) {
    close(Notice::expired);
    return false;
  }
  observedAt = now;
  if (editor.active()) {
    editor.tick(now);
    if (!editor.active()) {
      close(Notice::expired);
      return false;
    }
  }
  return true;
}
bool Form::open(Kind k, uint8_t existingId, uint64_t now) {
  discard();
  if (k != Kind::room && k != Kind::shade && k != Kind::group) {
    close(Notice::invalid);
    return false;
  }
  kind = k;
  sourceId = existingId;
  const auto *fields = metadata();
  if (!fields) {
    close(Notice::invalid);
    return false;
  }
  for (uint8_t i = 0; i < fields->count; i++)
    if (!entityField(type(),fields->fields[i])) {
      close(Notice::invalid);
      return false;
    }
  admitted = backend.authorization();
  admittedRevision = backend.revision();
  if (!admitted.allowed || !admitted.generation ||
      !admitted.configurationGeneration) {
    close(Notice::locked);
    return false;
  }
  if (!admittedRevision || !backend.prepare(k, existingId, id) || !id ||
      id == 255) {
    close(Notice::invalid);
    return false;
  }
  activityAt = observedAt = now;
  frame.mode = Mode::fields;
  if (!guard(now))
    return false;
  render();
  if (!guard(now))
    return false;
  return !closed();
}
bool Form::read(const entity_field_t &f, Value &v) {
  v = {};
  v.kind = f.schema().kind;
  const auto candidate = changes[f.schema().key];
  if (!candidate.isNull()) {
    if (v.kind == 's')
      copy(v.text, sizeof(v.text), candidate.as<const char *>());
    else if (v.kind == 'b')
      v.boolean = candidate.as<int>() != 0;
    else
      v.integer = candidate.as<int64_t>();
    return true;
  }
  if (!backend.read(kind, sourceId, f.field, v))
    return false;
  return v.kind == f.schema().kind &&
         (v.kind != 's' || memchr(v.text, 0, sizeof(v.text)));
}
bool Form::fieldActive(const entity_field_t &f) {
  if (kind != Kind::shade ||
      (f.field != proto_field_t::gpioUp && f.field != proto_field_t::gpioDown &&
       f.field != proto_field_t::gpioMy && f.field != proto_field_t::gpioLLTrigger))
    return true;
  const auto *protocol = entityField(type(), proto_field_t::proto);
  Value value;
  if (!protocol || !read(*protocol, value)) { close(Notice::changed); return false; }
  // Reject invalid legacy enum values before narrowing to radio_proto.
  return validStored(*protocol, value) && entityFieldActive(f, radio_proto(value.integer));
}
void Form::refuse(proto_field_t field, const char *reason) {
  editing = 0; validationField = false;
  for (uint8_t i = 0; i < metadata()->count; i++)
    if (metadata()->fields[i] == field) { editing = i; validationField = true; }
  copy(validationReason, sizeof(validationReason), reason);
  frame.mode = Mode::validation;
  frame.notice = Notice::invalid;
  selected = 0;
  render();
}
void Form::render() {
  const auto mode = frame.mode;
  const auto notice = frame.notice;
  frame = {};
  frame.mode = mode;
  frame.notice = notice;
  frame.id = id;
  frame.kind = kind;
  frame.dirty = changes.size() != 0;
  snprintf(frame.title, sizeof(frame.title), "%s %s %u",
           sourceId ? "Edit" : "Add", title(kind), id);
  const auto *fields = metadata();
  if (!fields)
    return;
  if (mode == Mode::fields) {
    frame.total = fields->count + 2 + (sourceId ? 1 : 0);
    frame.selected = selected;
    frame.offset = selected / 3 * 3;
    copy(frame.hint, sizeof(frame.hint),
         "Apply draft; Save. Back discards.");
    for (uint8_t i = frame.offset; i < frame.total && frame.rowCount < 3; i++) {
      auto &row = frame.rows[frame.rowCount++];
      row.selected = i == selected;
      if (i < fields->count) {
        const auto &f = *entityField(type(),fields->fields[i]);
        Value v;
        const bool activeField = fieldActive(f);
        if (closed()) return;
        snprintf(row.label, sizeof(row.label), "%s%s", activeField ? "" : "Inactive: ", label(f.field));
        if (!read(f, v)) {
          close(Notice::changed);
          return;
        }
        formatValue(f, v, row.value, sizeof(row.value));
        if (closed()) return;
        row.dirty = !changes[f.schema().key].isNull();
      } else
        copy(row.label, sizeof(row.label),
             i == fields->count       ? "Save"
             : i == fields->count + 1 ? "Cancel"
                                      : "Delete");
    }
  } else if (mode == Mode::adjust || mode == Mode::text) {
    const auto &f = *entityField(type(),fields->fields[editing]);
    frame.rowCount = 3;
    copy(frame.rows[0].label, sizeof(frame.rows[0].label), label(f.field));
    editor.previewTail(frame.rows[0].value, sizeof(frame.rows[0].value));
    if (mode == Mode::text) {
      copy(frame.title,sizeof(frame.title),label(f.field));
      frame.keyboard=editor.keyboardView();
      frame.textLength=editor.length();frame.textLimit=editor.maximumBytes();
      snprintf(frame.hint,sizeof(frame.hint),"%u/%u bytes. Done or Back keeps draft.",editor.length(),editor.maximumBytes());
    } else {
      if (editor.kind() == ValueKind::choice) {
        char raw[48]; editor.preview(raw, sizeof(raw));
        Choice item;
        if (!choice(f, uint8_t(strtoul(raw, nullptr, 10)), item)) { close(Notice::changed); return; }
        copy(frame.rows[0].value, sizeof(frame.rows[0].value), item.label);
      }
      copy(frame.rows[1].label, sizeof(frame.rows[1].label), "Turn to adjust");
      copy(frame.rows[2].label, sizeof(frame.rows[2].label),
           editor.kind() == ValueKind::integer ? "Press for Apply / Step" : "Press for Apply / Cancel");
      copy(frame.hint, sizeof(frame.hint), "Back cancels this field.");
    }
  } else if (mode == Mode::actions) {
    const bool numeric = editor.kind() == ValueKind::integer;
    frame.rowCount = frame.total = numeric ? 3 : 2;
    frame.selected = selected;
    const char *labels[] = {"Apply to draft", "Cancel field", "Change step"};
    for (unsigned i = 0; i < frame.total; i++) {
      copy(frame.rows[i].label, sizeof(frame.rows[i].label), labels[i]);
      frame.rows[i].selected = selected == i;
    }
    int64_t step = 1;
    for (uint8_t i = 0; i < stepPower; i++)
      step *= 10;
    snprintf(frame.rows[2].value, sizeof(frame.rows[2].value), "%lld",
             (long long)step);
    copy(frame.hint, sizeof(frame.hint), "Apply changes only the draft.");
  } else if (mode == Mode::replace) {
    const auto &f = *entityField(type(),fields->fields[editing]);
    snprintf(frame.title, sizeof(frame.title), "%s %u: %s", title(kind), id,
             label(f.field));
    frame.rowCount = frame.total = 2;
    frame.selected = selected;
    copy(frame.rows[0].label, sizeof(frame.rows[0].label), "Cancel");
    copy(frame.rows[1].label, sizeof(frame.rows[1].label),
         "Replace invalid value");
    frame.rows[selected ? 1 : 0].selected = true;
    copy(frame.hint, sizeof(frame.hint),
         "Original kept until Apply; Save persists replacement.");
  } else if (mode == Mode::draft || mode == Mode::validation) {
    const auto &f = *entityField(type(), fields->fields[editing]);
    if (mode == Mode::validation && !validationField)
      snprintf(frame.title, sizeof(frame.title), "%s %u", title(kind), id);
    else
      snprintf(frame.title, sizeof(frame.title), "%s %u: %s", title(kind), id, label(f.field));
    frame.selected = selected;
    if (mode == Mode::validation) {
      frame.rowCount = frame.total = 2;
      copy(frame.rows[0].label, sizeof(frame.rows[0].label), validationField ? "Revise field" : "Back to fields");
      copy(frame.rows[1].label, sizeof(frame.rows[1].label), "Cancel form");
      copy(frame.hint, sizeof(frame.hint), validationReason);
    } else {
      const bool activeField = fieldActive(f);
      if (closed()) return;
      frame.rowCount = frame.total = activeField ? 3 : 2;
      copy(frame.rows[0].label, sizeof(frame.rows[0].label), "Cancel");
      if (activeField) copy(frame.rows[1].label, sizeof(frame.rows[1].label), "Edit draft");
      copy(frame.rows[frame.total - 1].label, sizeof(frame.rows[0].label), "Revert field draft");
      copy(frame.hint, sizeof(frame.hint), activeField ? "Revert keeps other drafts." : "Inactive; Revert or change protocol.");
    }
    frame.rows[selected].selected = true;
  } else if(mode==Mode::discard) {
    frame.rowCount=frame.total=2;frame.selected=selected;
    copy(frame.rows[0].label,sizeof(frame.rows[0].label),"Keep editing");
    copy(frame.rows[1].label,sizeof(frame.rows[1].label),"Discard changes");
    frame.rows[selected].selected=true;
    copy(frame.hint,sizeof(frame.hint),"Unsaved changes will be lost.");
  } else if (mode == Mode::confirm) {
    frame.rowCount = frame.total = 2;
    frame.selected = selected;
    copy(frame.rows[0].label, sizeof(frame.rows[0].label), "Cancel");
    copy(frame.rows[1].label, sizeof(frame.rows[1].label),
         deleting ? "Delete" : "Save");
    frame.rows[selected ? 1 : 0].selected = true;
    snprintf(frame.title, sizeof(frame.title), "%s %s %u", deleting ? "Delete" : "Save", title(kind), id);
    if (!deleting && kind != Kind::room)
      copy(frame.hint, sizeof(frame.hint), kind == Kind::shade
        ? "May change live GPIO, motor timing and RF identity."
        : "Changes group identity and future commands; not motor pairing.");
    else
      snprintf(frame.hint, sizeof(frame.hint), "%s %s %u? %s", deleting ? "Delete" : "Save", title(kind), id,
        kind == Kind::room ? "Children become Unassigned." : "Motor pairing may remain.");
  } else if (mode == Mode::result) {
    snprintf(frame.title, sizeof(frame.title), "%s %u file result", title(kind),
             id);
    frame.rowCount = frame.total = notice == Notice::saved ? 1 : 2;
    frame.selected = selected;
    copy(frame.rows[0].label, sizeof(frame.rows[0].label), "Close");
    frame.rows[0].selected = !selected;
    if (frame.total == 2) {
      copy(frame.rows[1].label, sizeof(frame.rows[1].label),
           "Retry model file save");
      frame.rows[1].selected = selected;
    }
    snprintf(frame.hint, sizeof(frame.hint),
             "%s %u applied. Retry saves current model file only.", title(kind),
             id);
  }
}
void Form::beginField(uint64_t now, FieldStart start) {
  const bool replaceInvalid = start == FieldStart::replaceInvalid;
  if (start == FieldStart::selected) editing = selected;
  stepPower = 0;
  const auto &f = *entityField(type(),metadata()->fields[editing]);
  Value v;
  if (!read(f, v)) {
    close(Notice::changed);
    return;
  }
  const auto choices = backend.choiceCount(kind, sourceId, f.field);
  if (!replaceInvalid && !validOriginal(f, v)) {
    if (closed()) return;
    editor.cancel();
    frame.mode = Mode::replace;
    frame.notice = Notice::invalid;
    selected = 0;
    render();
    return;
  }
  if (replaceInvalid) {
    // Seed only after an explicit Replace action. Until Apply, neither this
    // bounded candidate nor cancellation changes the original or other drafts.
    v = {};
    v.kind = f.schema().kind;
    v.integer = f.lo <= 0 && f.hi >= 0 ? 0 : f.lo;
    if (choices) {
      Choice item;
      if (!choice(f, 0, item)) { close(Notice::changed); return; }
      v.integer = item.value;
    }
  }
  bool ok = false;
  if (v.kind == 's')
    ok = editor.beginText(v.text, f.hi, TextEncoding::utf8, false, now);
  else if (v.kind == 'b')
    ok = editor.beginBoolean(v.boolean, now);
  else if (choices) {
    for (uint8_t i = 0; i < choices; i++) {
      Choice item;
      if (!choice(f, i, item)) { close(Notice::changed); return; }
      if (item.value == v.integer) ok = editor.beginChoice(i, choices, now);
    }
  } else
    ok = editor.beginInteger(v.integer, f.lo, f.hi, 1, now);
  if (!ok) {
    frame.notice = Notice::invalid;
    frame.mode = Mode::fields;
    selected = editing;
    render();
    return;
  }
  frame.notice = Notice::none;
  frame.mode = v.kind == 's' ? Mode::text : Mode::adjust;
  render();
}
void Form::applyField(uint64_t) {
  const auto &f = *entityField(type(),metadata()->fields[editing]);
  const char *key = f.schema().key;
  bool ok = false;
  if (f.schema().kind == 's') {
    char text[21] = {};
    ok = editor.takeText(text, sizeof(text));
    if (ok)
      changes[key].set(text);
  } else if (f.schema().kind == 'b' || editor.kind() == ValueKind::choice) {
    uint8_t n = 0;
    ok = editor.takeChoice(n);
    if (ok) {
      if (f.schema().kind == 'b') changes[key] = int(n);
      else {
        Choice item;
        if (!choice(f, n, item)) { close(Notice::changed); return; }
        changes[key] = item.value;
      }
    }
  } else {
    int64_t n = 0;
    ok = editor.takeNumber(n);
    if (ok)
      changes[key] = n;
  }
  if (changes.overflowed()) {
    close(Notice::noMemory);
    return;
  }
  if (!ok || !validEntityValue(f, changes[key])) {
    close(Notice::invalid);
    return;
  }
  frame.mode = Mode::fields;
  selected = editing;
  frame.notice = Notice::none;
  render();
}
void Form::save(uint64_t now) {
  if (!guard(now))
    return;
  if (!deleting)
    for (uint8_t i = 0; i < metadata()->count; i++) {
      const auto &f = *entityField(type(), metadata()->fields[i]);
      if (changes[f.schema().key].isNull()) continue;
      if (!fieldActive(f)) {
        if (!closed()) refuse(f.field, "Revert inactive GPIO draft or select GPIO protocol.");
        return;
      }
      Value value;
      if (!read(f, value)) { close(Notice::changed); return; }
      if (!validOriginal(f, value)) {
        if (!closed()) refuse(f.field, "Value is no longer available; revise this field.");
        return;
      }
    }
  if (!sourceId && !deleting) {
    // An offered default can be invalid (e.g. next sort order after 127).
    // Never let an omitted field silently wrap in the domain's add default.
    for (uint8_t i = 0; i < metadata()->count; i++) {
      const auto &f = *entityField(type(),metadata()->fields[i]);
      Value value;
      if (!read(f, value)) {
        close(Notice::changed);
        return;
      }
      if (!validOriginal(f, value)) {
        if (closed()) return;
        frame.mode = Mode::fields;
        frame.notice = Notice::invalid;
        selected = i;
        render();
        return;
      }
    }
  }
  const auto editType = type(deleting);
  JsonDocument expanded(&allocator);
  proto_error_buffer_t error = {};
  // Expand the changed-only draft before adding the captured stable identity.
  const auto *why = expandEditPayload(editType, changes.as<JsonObjectConst>(),
                                      expanded, error);
  if (deleting) {
    expanded.clear();
    expanded.to<JsonObject>();
    why = nullptr;
  }
  if (sourceId)
    expanded[kind == Kind::room    ? "roomId"
             : kind == Kind::shade ? "shadeId"
                                   : "groupId"] = sourceId;
  if (changes.overflowed() || expanded.overflowed()) {
    close(Notice::noMemory);
    return;
  }
  if (why) {
    frame.notice = Notice::invalid;
    frame.mode = Mode::fields;
    render();
    return;
  }
  if (!guard(now))
    return;
  const auto outcome =
      backend.apply(editType, expanded, admitted, admittedRevision);
  if (outcome.admission != Admission::accepted) {
    if (outcome.admission == Admission::invalid) {
      if (!memchr(outcome.reason, 0, sizeof(outcome.reason))) { close(Notice::changed); return; }
      refuse(outcome.field, outcome.reason[0] ? outcome.reason : "Review selected field; changes were not applied.");
    } else if (outcome.admission == Admission::busy) {
      frame.mode = Mode::fields;
      frame.notice = Notice::busy;
      render();
    } else if (outcome.admission == Admission::noMemory)
      close(Notice::noMemory);
    else
      close(admissionNotice(outcome.admission));
    return;
  }
  const auto &result = outcome.edit;
  if (!result.applied) {
    frame.mode = Mode::fields;
    frame.notice = Notice::failed;
    render();
    return;
  }
  // Mutation is never replayable: retain its new ID and observed history only.
  id = result.id;
  admittedRevision = backend.revision();
  changes.clear();
  editor.cancel();
  selected = 0;
  frame.mode = Mode::result;
  frame.notice = result.persistence.saved() ? Notice::saved
                 : result.persistence.state == ModelCommitState::uncertain
                     ? Notice::uncertain
                     : Notice::appliedNotSaved;
  render();
}
void Form::tick(uint64_t now) { guard(now); }
void Form::turn(int direction, uint64_t now) {
  if (!direction || !guard(now))
    return;
  touch(now);
  if (frame.mode == Mode::text)
    editor.keyboardTurn(direction, now);
  else if (frame.mode == Mode::adjust)
    editor.rotate(direction, now);
  else if (direction > 0 && selected + 1 < frame.total)
    ++selected;
  else if (direction < 0 && selected)
    --selected;
  render();
}
void Form::activate(uint64_t now) {
  if (!guard(now))
    return;
  touch(now);
  switch (frame.mode) {
  case Mode::fields:
    if (selected < metadata()->count) {
      const auto &f = *entityField(type(), metadata()->fields[selected]);
      editing = selected;
      if (!changes[f.schema().key].isNull()) {
        frame.mode = Mode::draft; frame.notice = Notice::none; selected = 0; render();
      } else if (!fieldActive(f)) {
        if (!closed()) refuse(proto_field_t::proto, "Select GPIO protocol before editing this field.");
      } else beginField(now);
      return;
    }
    if (selected == metadata()->count + 1) {
      if(changes.size()){frame.mode=Mode::discard;selected=0;break;}
      discard();return;
    }
    deleting = selected > metadata()->count + 1;
    if (deleting) {
      frame.mode = Mode::confirm;
      selected = 0;
    } else if (!changes.size() && sourceId) {
      close(Notice::unchanged);
      return;
    } else if (kind != Kind::room) {
      frame.mode = Mode::confirm;
      selected = 0;
    } else {
      save(now);
      return;
    }
    break;
  case Mode::adjust:
    frame.mode = Mode::actions;
    selected = 1;
    break;
  case Mode::text: {
    const auto result = editor.keyboardPress(now);
    if (result == EditOutcome::applied) {
      applyField(now);
      return;
    }
    if (result == EditOutcome::cancelled) {
      frame.mode = Mode::fields;
      selected = editing;
    } else if (result == EditOutcome::invalidUtf8 ||
               result == EditOutcome::full)
      frame.notice = Notice::invalid;
    break;
  }
  case Mode::actions:
    if (selected == 1) {
      editor.cancel();
      frame.mode = Mode::fields;
      selected = editing;
    } else if (selected == 2) {
      const auto &f = *entityField(type(),metadata()->fields[editing]);
      int64_t step = 1;
      for (uint8_t i = 0; i < stepPower; i++)
        step *= 10;
      if (step <= (f.hi - f.lo) / 10) {
        ++stepPower;
        step *= 10;
      } else {
        stepPower = 0;
        step = 1;
      }
      editor.setStep(step, now);
      frame.mode = Mode::adjust;
    } else {
      editor.apply(now);
      applyField(now);
      return;
    }
    break;
  case Mode::draft: {
    const auto &f = *entityField(type(), metadata()->fields[editing]);
    if (!selected) {
      frame.mode = Mode::fields; selected = editing;
    } else if (selected == 1 && fieldActive(f)) {
      beginField(now, FieldStart::existing); return;
    } else {
      if (closed()) return;
      changes.remove(f.schema().key);
      frame.mode = Mode::fields; selected = editing;
    }
    frame.notice = Notice::none;
    break;
  }
  case Mode::validation:
    if (selected) { discard(); return; }
    frame.mode = Mode::fields; selected = editing; frame.notice = Notice::none;
    memset(validationReason, 0, sizeof(validationReason));
    break;
  case Mode::replace:
    if (selected) {
      beginField(now, FieldStart::replaceInvalid);
      return;
    }
    frame.mode = Mode::fields;
    selected = editing;
    break;
  case Mode::discard:
    if(selected){discard();return;}
    frame.mode=Mode::fields;selected=editing;break;
  case Mode::confirm:
    if (selected) {
      save(now);
      return;
    }
    frame.mode = Mode::fields;
    selected = metadata()->count + (deleting ? 2 : 0);
    deleting = false;
    break;
  case Mode::result:
    if (!selected) {
      discard();
      return;
    }
    {
      const auto result = backend.retryFileCommit(admitted, admittedRevision);
      if (result.admission == Admission::locked ||
          result.admission == Admission::stale) {
        close(admissionNotice(result.admission));
        return;
      }
      frame.notice = result.admission == Admission::busy ? Notice::busy
                     : result.file.saved()               ? Notice::saved
                     : result.file.state == ModelCommitState::uncertain
                         ? Notice::uncertain
                         : Notice::appliedNotSaved;
      if (result.file.saved() && result.admission == Admission::accepted)
        selected = 0;
    }
    break;
  default:
    break;
  }
  render();
}
void Form::back(uint64_t now) {
  if (!guard(now))
    return;
  touch(now);
  if(frame.mode==Mode::text) {
    if(editor.keyboardBack(now)){render();return;}
    editor.apply(now);applyField(now);return;
  }
  if (frame.mode == Mode::adjust ||
      frame.mode == Mode::actions || frame.mode == Mode::draft ||
      frame.mode == Mode::validation) {
    editor.cancel();
    frame.mode = Mode::fields;
    selected = editing;
    frame.notice = Notice::none;
    memset(validationReason, 0, sizeof(validationReason));
  } else if (frame.mode == Mode::replace) {
    frame.mode = Mode::fields;
    selected = editing;
  } else if (frame.mode == Mode::confirm) {
    frame.mode = Mode::fields;
    selected = metadata()->count + (deleting ? 2 : 0);
    deleting = false;
  } else if(frame.mode==Mode::discard) {
    frame.mode=Mode::fields;selected=editing;
  } else if(frame.mode==Mode::fields && changes.size()) {
    frame.mode=Mode::discard;selected=0;
  } else {
    discard();return;
  }
  render();
}
} // namespace EntityAdmin
} // namespace LocalUi
