#include "LocalSettingsForm.h"
#include "BoardProfile.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
namespace LocalUi {
namespace Settings {
namespace {
void copy(char *to, size_t size, const char *from) {
  snprintf(to, size, "%s", from ? from : "");
}
void wipe(void *p, size_t n) {
  auto *b = static_cast<volatile unsigned char *>(p);
  while (n--)
    *b++ = 0;
}
struct ValueSink : SettingsSnapshotSink {
  proto_field_t wanted;
  settings_value_t &value;
  bool found = false;
  ValueSink(proto_field_t f, settings_value_t &v) : wanted(f), value(v) {}
  void field(proto_field_t f, const settings_value_t &v) override {
    if (f == wanted) {
      value = v;
      found = true;
    }
  }
};
const char *label(proto_field_t f) {
  switch (f) {
  case proto_field_t::hostname:
    return "Hostname";
  case proto_field_t::ssdpBroadcast:
    return "Discovery broadcast";
  case proto_field_t::ntpServer:
    return "Time server";
  case proto_field_t::posixZone:
    return "Timezone (POSIX)";
  case proto_field_t::connMode:
    return "Connection";
  case proto_field_t::ssid:
    return "WiFi name";
  case proto_field_t::passphrase:
    return "WiFi password";
  case proto_field_t::securityType:
    return "Authentication";
  case proto_field_t::username:
    return "Username";
  case proto_field_t::password:
    return "Password";
  case proto_field_t::pin:
    return "PIN";
  case proto_field_t::enabled:
    return "Enabled";
  case proto_field_t::port:
    return "Port";
  case proto_field_t::frequency:
    return "Frequency (MHz)";
  case proto_field_t::rxBandwidth:
    return "Bandwidth (kHz)";
  case proto_field_t::deviation:
    return "Deviation (kHz)";
  case proto_field_t::txPower:
    return "TX power (dBm)";
  case proto_field_t::proto:
    return "Radio protocol";
  case proto_field_t::bitLength:
    return "Frame bits";
  case proto_field_t::roaming:
    return "WiFi roaming";
  case proto_field_t::hidden:
    return "Hidden WiFi";
  case proto_field_t::boardType:
    return "Ethernet board";
  case proto_field_t::phyType:
    return "Ethernet PHY";
  case proto_field_t::CLKMode:
    return "Ethernet clock mode";
  case proto_field_t::phyAddress:
    return "PHY address";
  case proto_field_t::PWRPin:
    return "Ethernet power GPIO";
  case proto_field_t::MDCPin:
    return "Ethernet MDC GPIO";
  case proto_field_t::MDIOPin:
    return "Ethernet MDIO GPIO";
  case proto_field_t::dhcp:
    return "Automatic IP (DHCP)";
  case proto_field_t::ip:
    return "IP address";
  case proto_field_t::gateway:
    return "Gateway";
  case proto_field_t::subnet:
    return "Subnet mask";
  case proto_field_t::dns1:
    return "Primary DNS";
  case proto_field_t::dns2:
    return "Secondary DNS";
  case proto_field_t::pubDisco:
    return "Publish discovery";
  case proto_field_t::protocol:
    return "MQTT protocol";
  case proto_field_t::rootTopic:
    return "Root topic";
  case proto_field_t::discoTopic:
    return "Discovery topic";
  case proto_field_t::TXPin:
    return "Radio TX GPIO";
  case proto_field_t::RXPin:
    return "Radio RX GPIO";
  case proto_field_t::SCKPin:
    return "Radio SCK GPIO";
  case proto_field_t::MOSIPin:
    return "Radio MOSI GPIO";
  case proto_field_t::MISOPin:
    return "Radio MISO GPIO";
  case proto_field_t::CSNPin:
    return "Radio CSN GPIO";
  case proto_field_t::noiseDetection:
    return "Noise detection";
  default:
    return protoField(f).name;
  }
}
const char *title(settings_section_t s) {
  switch (s) {
  case settings_section_t::general:
    return "General";
  case settings_section_t::network:
    return "Network";
  case settings_section_t::mqtt:
    return "MQTT";
  case settings_section_t::security:
    return "Security";
  case settings_section_t::radio:
    return "Radio";
  }
  return "Settings";
}
int32_t fieldScale(const settings_field_t &f) {
  return f.schema().kind == 'n'
             ? (f.field == proto_field_t::frequency ? 1000 : 100)
             : 1;
}
int64_t fieldBound(const settings_field_t &f, int32_t bound) {
  return int64_t(bound) * (fieldScale(f) == 1000 ? 10 : 1);
}
bool numericOriginal(const settings_field_t &f, const settings_value_t &v,
                     int64_t &candidate) {
  const int64_t low = fieldBound(f, f.lo), high = fieldBound(f, f.hi);
  if (v.kind == 'n') {
    if(f.field==proto_field_t::frequency && !BoardProfile::radioFrequencyAllowed(v.number))return false;
    const long double scaled = static_cast<long double>(v.number) * fieldScale(f);
    if (!isfinite(scaled))
      return false;
    // Round to the displayed resolution, then bound-check before conversion.
    const long double rounded = roundl(scaled);
    if (rounded < static_cast<long double>(low) ||
        rounded > static_cast<long double>(high))
      return false;
    candidate = int64_t(rounded);
  } else {
    candidate = v.integer;
    if (candidate < low || candidate > high)
      return false;
  }
  if (f.choiceCount) {
    for (uint8_t i = 0; i < f.choiceCount; ++i)
      if (f.choices[i] == candidate)
        return true;
    return false;
  }
  return true;
}
void format(const settings_field_t &f, const settings_value_t &v, char *out,
            size_t size) {
  if (f.secret()) {
    copy(out, size, v.boolean ? "Set" : "Unset");
    return;
  }
  if (v.kind == 'i' || v.kind == 'n') {
    int64_t candidate = 0;
    if (!numericOriginal(f, v, candidate)) {
      if (v.kind == 'n')
        snprintf(out, size, "Invalid (%g)", double(v.number));
      else
        snprintf(out, size, "Invalid (%ld)", long(v.integer));
      return;
    }
  }
  if (f.field == proto_field_t::securityType) {
    copy(out, size,
         v.integer == 0   ? "None"
         : v.integer == 1 ? "PIN"
                          : "Password");
    return;
  }
  if (f.field == proto_field_t::connMode) {
    copy(out, size,
         v.integer == 1   ? "WiFi"
         : v.integer == 2 ? "Ethernet"
                          : "Ethernet + WiFi");
    return;
  }
  if (f.field == proto_field_t::proto) {
    copy(out, size,
         v.integer == 0   ? "RTS"
         : v.integer == 1 ? "RTW"
         : v.integer == 2 ? "RTV"
         : v.integer == 8 ? "GP Relay"
                          : "GP Remote");
    return;
  }
  if (v.kind == 's')
    copy(out, size, v.text);
  else if (v.kind == 'b')
    copy(out, size, v.boolean ? "On" : "Off");
  else if (v.kind == 'n')
    snprintf(out, size, f.field == proto_field_t::frequency ? "%.3f" : "%.2f",
             double(v.number));
  else
    snprintf(out, size, "%ld", long(v.integer));
}
bool same(const Authorization &a, const Authorization &b) {
  return a.allowed && b.allowed && a.generation == b.generation &&
         a.configurationGeneration == b.configurationGeneration;
}
} // namespace
Form::Form(Backend &b, ArduinoJson::Allocator &a)
    : backend(b), allocator(a), changes(&a) {}
Form::~Form() { close(Notice::none); }
uint8_t Form::count() const {
  return settingsFieldCount(section, backend.hasEthernet());
}
void Form::close(Notice notice) {
  changes.clear();
  editor.cancel();
  editingField = nullptr;
  admitted = {};
  admittedRevision = 0;
  frame = {};
  frame.notice = notice;
  selected = fieldIndex = stepPower = 0;
}
void Form::touch(uint64_t now) {
  lastActivity = lastObserved = now;
  backend.activity();
}
bool Form::guard(uint64_t now) {
  if (closed())
    return false;
  if (!same(admitted, backend.authorization())) {
    close(Notice::locked);
    return false;
  }
  if (frame.mode != Mode::sections &&
      admittedRevision != backend.revision(section)) {
    close(Notice::changed);
    return false;
  }
  if (now < lastObserved || now < lastActivity ||
      now - lastActivity >= timeoutMs) {
    close(Notice::expired);
    return false;
  }
  lastObserved = now;
  editor.tick(now);
  if (editor.outcome() == EditOutcome::expired) {
    close(Notice::expired);
    return false;
  }
  return true;
}
bool Form::open(uint64_t now) {
  close(Notice::none);
  admitted = backend.authorization();
  if (!admitted.allowed) {
    close(Notice::locked);
    return false;
  }
  frame.mode = Mode::sections;
  lastActivity = lastObserved = now;
  render();
  return true;
}
bool Form::openSection(settings_section_t s, uint64_t now) {
  if (static_cast<uint8_t>(s) > 4) {
    close(Notice::invalid);
    return false;
  }
  if (!open(now))
    return false;
  section = s;
  selected = 0;
  admittedRevision = backend.revision(s);
  frame.mode = Mode::fields;
  render();
  return !closed();
}
bool Form::openRadioFrequency(float frequency,uint64_t now,Authorization expected,uint64_t revision) {
  discard();const auto current=backend.authorization();
  if(!current.allowed||!expected.allowed||!expected.generation||!expected.configurationGeneration||
     current.generation!=expected.generation||current.configurationGeneration!=expected.configurationGeneration||
     !revision||backend.revision(settings_section_t::radio)!=revision){close(Notice::changed);return false;}
  if(!isfinite(frequency)||frequency<433||frequency>434){close(Notice::invalid);return false;}
  if(!openSection(settings_section_t::radio,now))return false;
  if(admitted.generation!=expected.generation||admitted.configurationGeneration!=expected.configurationGeneration||admittedRevision!=revision){close(Notice::changed);return false;}
  const settings_field_t *frequencyField=nullptr;
  for(uint8_t i=0;i<count();++i){const auto *field=settingsFieldAt(section,i,backend.hasEthernet());if(field->field==proto_field_t::frequency){selected=i;frequencyField=field;}}
  if(!frequencyField){close(Notice::changed);return false;}
  settings_value_t original;const bool known=readValue(*frequencyField,original)&&original.kind=='n'&&isfinite(original.number);
  const auto scaled=lround(double(frequency)*1000);
  const bool changed=!known||original.number<300||original.number>928||lround(double(original.number)*1000)!=scaled;
  wipe(&original,sizeof(original));
  if(changed){
    changes[protoField(proto_field_t::frequency).key]=double(scaled)/1000;
    changes[protoField(proto_field_t::section).key]=settingsSectionName(section);
    JsonDocument expanded(&allocator);proto_error_buffer_t why={};settings_section_t parsed;
    const char *error=expandSettingsPayload(changes.as<JsonObjectConst>(),backend.hasEthernet(),parsed,expanded,why);
    const bool noMemory=changes.overflowed()||expanded.overflowed();changes.remove(protoField(proto_field_t::section).key);
    if(noMemory||error){close(noMemory?Notice::noMemory:Notice::invalid);return false;}
  }
  if(!guard(now))return false;render();return !closed();
}
bool Form::openNetworkSSID(const char *ssid, uint64_t now,
                           Authorization expected, uint64_t revision) {
  discard();
  const auto current = backend.authorization();
  if (!current.allowed || !expected.allowed || !expected.generation || !expected.configurationGeneration ||
      current.generation != expected.generation ||
      current.configurationGeneration != expected.configurationGeneration ||
      !revision || backend.revision(settings_section_t::network) != revision) {
    close(Notice::changed); return false;
  }
  if (ssid && (!ssid[0] || strnlen(ssid,33)>32)) {
    close(Notice::invalid); return false;
  }
  if (!openSection(settings_section_t::network, now)) return false;
  if (admitted.generation != expected.generation ||
      admitted.configurationGeneration != expected.configurationGeneration ||
      admittedRevision != revision) { close(Notice::changed); return false; }
  const settings_field_t *ssidField=nullptr;
  for (uint8_t i=0;i<count();++i) {
    const auto *field=settingsFieldAt(section,i,backend.hasEthernet());
    if (field->field==proto_field_t::ssid) {selected=i;ssidField=field;}
  }
  if (!ssidField) {close(Notice::changed);return false;}
  bool changed=false;
  if (ssid) {
    settings_value_t original;
    const bool valid=readValue(*ssidField,original) && original.kind=='s' &&
        memchr(original.text,0,sizeof(original.text));
    changed=valid && strcmp(original.text,ssid)!=0;
    wipe(&original,sizeof(original));
    if (!valid) {close(Notice::changed);return false;}
  }
  if (changed) {
    changes[protoField(proto_field_t::ssid).key]=ssid;
    changes[protoField(proto_field_t::section).key]=settingsSectionName(section);
    JsonDocument expanded(&allocator); proto_error_buffer_t why={};
    settings_section_t parsed;
    const char *error=expandSettingsPayload(changes.as<JsonObjectConst>(),
        backend.hasEthernet(),parsed,expanded,why);
    const bool noMemory=changes.overflowed() || expanded.overflowed();
    changes.remove(protoField(proto_field_t::section).key);
    if (noMemory || error) {close(noMemory?Notice::noMemory:Notice::invalid);return false;}
  }
  if (!guard(now)) return false;
  render(); return !closed();
}
bool Form::readOnly(const settings_field_t &f) {
  if (f.field == proto_field_t::connMode && !backend.hasEthernet()) {
    settings_value_t value;
    int64_t candidate = 0;
    const bool fixed = !readValue(f, value) || numericOriginal(f, value, candidate);
    wipe(&value, sizeof(value));
    return fixed;
  }
  if (!backend.fixedRadioPins())
    return false;
  return f.field == proto_field_t::TXPin || f.field == proto_field_t::RXPin ||
         f.field == proto_field_t::SCKPin ||
         f.field == proto_field_t::MOSIPin ||
         f.field == proto_field_t::MISOPin || f.field == proto_field_t::CSNPin;
}
bool Form::readValue(const settings_field_t &f, settings_value_t &v) {
  v = {};
  v.kind = f.schema().kind;
  const char *key = f.schema().key;
  if (changes[key].isUnbound() == false && !changes[key].isNull()) {
    auto d = changes[key];
    if (f.secret()) {
      v.kind = 'b';
      v.boolean = true;
    } else if (v.kind == 's')
      copy(v.text, sizeof(v.text), d.as<const char *>());
    else if (v.kind == 'b')
      v.boolean = d.as<int>() != 0;
    else if (v.kind == 'n')
      v.number = d.as<float>();
    else
      v.integer = d.as<int32_t>();
    return true;
  }
  ValueSink sink(f.secret() ? f.presenceField() : f.field, v);
  return backend.read(section, sink) && sink.found;
}
void Form::render() {
  const auto mode = frame.mode;
  const auto notice = frame.notice;
  frame = {};
  frame.mode = mode;
  frame.notice = notice;
  frame.dirty = changes.size() != 0;
  copy(frame.title, sizeof(frame.title),
       mode == Mode::sections ? "Settings" : title(section));
  const char *menus[3] = {};
  if (mode == Mode::sections)
    frame.total = 5;
  else if (mode == Mode::fields)
    frame.total = count() + 2;
  else if (mode == Mode::secret) {
    frame.total = 2;
    menus[0] = "Keep";
    menus[1] = "Replace";
    copy(frame.hint, sizeof(frame.hint),
         "Blank retains saved secret; identity changes can clear it.");
  } else if (mode == Mode::replace) {
    copy(frame.title, sizeof(frame.title), label(editingField->field));
    frame.total = 2;
    menus[0] = "Cancel";
    menus[1] = "Replace invalid value";
    copy(frame.hint, sizeof(frame.hint),
         "Original kept until Apply; Save persists replacement.");
  } else if (mode == Mode::actions) {
    const bool numeric = editor.kind() == ValueKind::integer ||
                         editor.kind() == ValueKind::fixed2;
    frame.total = numeric ? 3 : 2;
    menus[0] = "Edit value";
    menus[1] = numeric ? "Step" : "Apply to draft";
    menus[2] = "Apply to draft";
  } else if (mode == Mode::discard) {
    frame.total=2;menus[0]="Keep editing";menus[1]="Discard changes";
    copy(frame.hint,sizeof(frame.hint),"Unsaved changes will be lost.");
  } else if (mode == Mode::confirm) {
    frame.total = 2;
    menus[0] = "Cancel";
    menus[1] = "Save settings";
    copy(frame.hint, sizeof(frame.hint),
         section == settings_section_t::security
             ? "Changes authentication; may close sessions."
         : section == settings_section_t::radio
             ? "Changes radio operation; may restart."
         : section == settings_section_t::mqtt
             ? "Changes MQTT connection/discovery."
             : "Network can disconnect; may restart.");
  } else if (mode == Mode::adjust || mode == Mode::text) {
    frame.total = frame.rowCount = 3;
    copy(frame.rows[0].label, sizeof(frame.rows[0].label),
         label(editingField->field));
    editor.preview(frame.rows[0].value, sizeof(frame.rows[0].value));
    if (mode == Mode::adjust) {
      if (editingField->schema().kind != 'b') {
        char raw[48];
        editor.preview(raw, sizeof(raw));
        settings_value_t value;
        value.kind = editingField->schema().kind;
        if (value.kind == 'n')
          value.number = float(strtod(raw, nullptr) / scale);
        else {
          value.integer = int32_t(strtol(raw, nullptr, 10));
          if (editingField->choiceCount)
            value.integer = editingField->choices[value.integer];
        }
        format(*editingField, value, frame.rows[0].value,
               sizeof(frame.rows[0].value));
      }
      copy(frame.rows[1].label, sizeof(frame.rows[1].label), "Turn to adjust");
      copy(frame.rows[2].label, sizeof(frame.rows[2].label),
           "Press for Apply / Step");
    } else {
      frame.total=0;
      editor.previewTail(frame.rows[0].value, sizeof(frame.rows[0].value));
      snprintf(frame.hint, sizeof(frame.hint),
               "%u/%u bytes. Done or Back keeps draft.", editor.length(),
               editor.maximumBytes());
      copy(frame.title, sizeof(frame.title), label(editingField->field));
      frame.keyboard=editor.keyboardView();
      frame.textLength=editor.length();frame.textLimit=editor.maximumBytes();

    }
    return;
  } else
    return;
  const bool consequenceHint = mode == Mode::secret || mode == Mode::confirm ||
                               mode == Mode::replace;
  if (notice == Notice::partial && !consequenceHint)
    copy(frame.hint, sizeof(frame.hint),
         "May be partly stored; running unchanged. Retry before restart.");
  else if (notice == Notice::failed && !consequenceHint)
    copy(frame.hint, sizeof(frame.hint),
         "Not saved; running unchanged. Retry or Cancel.");
  else if (notice == Notice::busy && !consequenceHint)
    copy(frame.hint, sizeof(frame.hint),
         "Actions queued. Retry Save when clear, or Cancel.");
  else if (notice == Notice::invalid && !consequenceHint)
    copy(frame.hint, sizeof(frame.hint),
         "Value refused. Revise the draft or Cancel.");
  frame.selected = selected;
  frame.offset = selected < 3 ? 0 : selected - 2;
  frame.rowCount = frame.total - frame.offset;
  if (frame.rowCount > 3)
    frame.rowCount = 3;
  for (uint8_t row = 0; row < frame.rowCount; row++) {
    const auto index = uint8_t(frame.offset + row);
    auto &out = frame.rows[row];
    out.selected = index == selected;
    if (mode == Mode::sections)
      copy(out.label, sizeof(out.label),
           title(static_cast<settings_section_t>(index)));
    else if (mode == Mode::fields) {
      if (index >= count()) {
        copy(out.label, sizeof(out.label),
             index == count() ? "Save" : "Cancel");
        continue;
      }
      const auto &f = *settingsFieldAt(section, index, backend.hasEthernet());
      copy(out.label, sizeof(out.label), label(f.field));
      out.readOnly = readOnly(f);
      out.dirty = !changes[f.schema().key].isNull();
      settings_value_t v;
      if (!readValue(f, v)) {
        wipe(&v, sizeof(v));
        close(Notice::unavailable);
        return;
      }
      format(f, v, out.value, sizeof(out.value));
      if (f.secret() && out.dirty)
        copy(out.value, sizeof(out.value),
             changes[f.schema().key].as<const char *>()[0] ? "Replace pending"
                                                           : "Blank (retain*)");
      wipe(&v, sizeof(v));
    } else {
      copy(out.label, sizeof(out.label), menus[index]);
      if (mode == Mode::actions && frame.total == 3 && index == 1) {
        int64_t step = 1;
        for (uint8_t i = 0; i < stepPower; i++)
          step *= 10;
        snprintf(out.value, sizeof(out.value), "%g", double(step) / scale);
      }
    }
  }
}
void Form::beginField(uint64_t now, FieldStart start) {
  editingField = settingsFieldAt(section, fieldIndex, backend.hasEthernet());
  if (!editingField)
    return;
  if (readOnly(*editingField)) {
    frame.notice = Notice::readOnly;
    render();
    return;
  }
  selected = 0;
  stepPower = 0;
  scale = 1;
  if (editingField->secret() && start != FieldStart::replaceSecret) {
    frame.mode = Mode::secret;
    render();
    return;
  }
  settings_value_t v;
  if (!readValue(*editingField, v)) {
    close(Notice::unavailable);
    return;
  }
  const auto &f = *editingField;
  scale = fieldScale(f);
  const int64_t low = fieldBound(f, f.lo), high = fieldBound(f, f.hi);
  int64_t candidate = 0;
  if (f.schema().kind == 'i' || f.schema().kind == 'n') {
    if (start == FieldStart::replaceInvalid) {
      // Seeding is explicit and changes no draft until Apply.
      candidate = f.field==proto_field_t::frequency ? 433420 :
        f.choiceCount ? f.choices[0] : (low <= 0 && high >= 0 ? 0 : low);
    } else if (!numericOriginal(f, v, candidate)) {
      wipe(&v, sizeof(v));
      editor.cancel();
      frame.mode = Mode::replace;
      frame.notice = Notice::invalid;
      render();
      return;
    }
  }
  bool ok = false;
  if (f.schema().kind == 's') {
    ok = editor.beginText(f.secret() ? "" : v.text, f.hi, TextEncoding::bytes,
                          f.secret(), now);
    frame.mode = Mode::text;
  } else if (f.schema().kind == 'b') {
    ok = editor.beginBoolean(v.boolean, now);
    frame.mode = Mode::adjust;
  } else if (f.choiceCount) {
    uint8_t index = 0;
    while (index < f.choiceCount && f.choices[index] != candidate)
      ++index;
    ok = index < f.choiceCount && editor.beginChoice(index, f.choiceCount, now);
    frame.mode = Mode::adjust;
  } else {
    ok = editor.beginInteger(candidate, low, high, 1, now);
    frame.mode = Mode::adjust;
  }
  wipe(&v, sizeof(v));
  if (!ok) {
    editor.cancel();
    editingField = nullptr;
    selected = fieldIndex;
    frame.mode = Mode::fields;
    frame.notice = Notice::invalid;
  }
  render();
}
void Form::applyField(uint64_t now) {
  const auto &f = *editingField;
  settings_value_t previous;
  previous.kind = f.schema().kind;
  const char *key = f.schema().key;
  const bool had = !changes[key].isNull();
  if (had) {
    auto old = changes[key];
    if (previous.kind == 's')
      copy(previous.text, sizeof(previous.text), old.as<const char *>());
    else if (previous.kind == 'n')
      previous.number = old.as<float>();
    else
      previous.integer = old.as<int32_t>();
  }
  bool took = false;
  if (f.schema().kind == 's') {
    char text[65] = {};
    took = editor.takeText(text, sizeof(text));
    if (took)
      changes[key].set(text);
    wipe(text, sizeof(text));
  } else if (f.choiceCount || f.schema().kind == 'b') {
    uint8_t choice = 0;
    took = editor.takeChoice(choice);
    if (took)
      changes[key] = f.choiceCount ? int(f.choices[choice]) : int(choice);
  } else {
    int64_t number = 0;
    took = editor.takeNumber(number);
    if (took) {
      if (f.schema().kind == 'n')
        changes[key] = double(number) / scale;
      else
        changes[key] = int32_t(number);
    }
  }
  if (!took) {
    wipe(&previous, sizeof(previous));
    frame.notice = Notice::invalid;
    render();
    return;
  }
  if (changes.overflowed()) {
    wipe(&previous, sizeof(previous));
    close(Notice::noMemory);
    return;
  }
  // Validation input uses a shallow temporary section key in the same
  // changed-only doc.
  changes[protoField(proto_field_t::section).key] =
      settingsSectionName(section);
  JsonDocument expanded(&allocator);
  proto_error_buffer_t why = {};
  settings_section_t parsed;
  const char *error =
      expandSettingsPayload(changes.as<JsonObjectConst>(),
                            backend.hasEthernet(), parsed, expanded, why);
  const bool noMemory = changes.overflowed() || expanded.overflowed();
  changes.remove(protoField(proto_field_t::section).key);
  if (noMemory) {
    wipe(&previous, sizeof(previous));
    close(Notice::noMemory);
    return;
  }
  if (error) {
    if (!had)
      changes.remove(key);
    else if (previous.kind == 's')
      changes[key].set(previous.text);
    else if (previous.kind == 'n')
      changes[key] = previous.number;
    else
      changes[key] = previous.integer;
    frame.notice = Notice::invalid;
  } else
    frame.notice = Notice::none;
  // Reinstating an older, longer string can itself need storage. Treat this
  // admission failure exactly like failure while building the candidate.
  if (changes.overflowed()) {
    wipe(&previous, sizeof(previous));
    close(Notice::noMemory);
    return;
  }
  wipe(&previous, sizeof(previous));
  frame.mode = Mode::fields;
  selected = fieldIndex;
  editingField = nullptr;
  render();
}
void Form::save(uint64_t now) {
  if (!guard(now))
    return;
  changes[protoField(proto_field_t::section).key] =
      settingsSectionName(section);
  JsonDocument expanded(&allocator);
  proto_error_buffer_t why = {};
  settings_section_t parsed;
  const char *error =
      expandSettingsPayload(changes.as<JsonObjectConst>(),
                            backend.hasEthernet(), parsed, expanded, why);
  const bool noMemory = changes.overflowed() || expanded.overflowed();
  changes.remove(protoField(proto_field_t::section).key);
  if (noMemory) {
    close(Notice::noMemory);
    return;
  }
  if (error) {
    frame.mode = Mode::fields;
    selected = count();
    frame.notice = Notice::invalid;
    render();
    return;
  }
  if (!guard(now))
    return;
  const auto result = backend.apply(section, expanded.as<JsonObject>(),
                                    admitted, admittedRevision);
  if (result.saved()) {
    close(result.restartBlocked ? Notice::savedRestartBlocked : Notice::saved);
    if (result.restartBlocked)
      frame.restartReason = result.restartReason;
    return;
  }
  if(result.persistence.radioCleanupFault) {close(Notice::cleanupFault);return;}
  if(result.persistence.liveApplied) {
    close(Notice::appliedUncertain);
    return;
  }
  if (result.refusal == settings_apply_refusal_t::unauthorized) {
    close(Notice::locked);
    return;
  }
  if (result.refusal == settings_apply_refusal_t::stale) {
    close(Notice::changed);
    return;
  }
  frame.mode = Mode::fields;
  selected = count();
  frame.notice = result.refusal == settings_apply_refusal_t::busy
                     ? Notice::busy
                 : result.refusal != settings_apply_refusal_t::none
                     ? Notice::invalid
                 : result.persistence.state == settings_save_state_t::partial
                     ? Notice::partial
                     : Notice::failed;
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
  else {
    const auto total = frame.total;
    if (direction > 0 && selected + 1 < total)
      ++selected;
    else if (direction < 0 && selected)
      --selected;
  }
  render();
}
void Form::activate(uint64_t now) {
  if (!guard(now))
    return;
  touch(now);
  switch (frame.mode) {
  case Mode::sections:
    section = static_cast<settings_section_t>(selected);
    admittedRevision = backend.revision(section);
    selected = 0;
    frame.mode = Mode::fields;
    break;
  case Mode::fields:
    if (selected < count()) {
      fieldIndex = selected;
      beginField(now);
      return;
    }
    if (selected > count()) {
      if(changes.size()){frame.mode=Mode::discard;selected=0;break;}
      close(Notice::none);return;
    }
    if (!changes.size()) {
      close(Notice::unchanged);
      return;
    }
    if (section == settings_section_t::general) {
      save(now);
      return;
    }
    selected = 0;
    frame.mode = Mode::confirm;
    break;
  case Mode::secret:
    if (selected == 0) {
      changes.remove(editingField->schema().key);
      frame.mode = Mode::fields;
      selected = fieldIndex;
      frame.notice = Notice::kept;
      editingField = nullptr;
    } else {
      beginField(now, FieldStart::replaceSecret);
      return;
    }
    break;
  case Mode::replace:
    if (selected) {
      beginField(now, FieldStart::replaceInvalid);
      return;
    }
    frame.mode = Mode::fields;
    selected = fieldIndex;
    editingField = nullptr;
    break;
  case Mode::adjust:
    frame.mode = Mode::actions;
    selected = 0;
    break;
  case Mode::text: {
    const auto result = editor.keyboardPress(now);
    if (result == EditOutcome::applied) {
      applyField(now);
      return;
    }
    if (result == EditOutcome::cancelled) {
      frame.mode = Mode::fields;
      selected = fieldIndex;
    } else if (result == EditOutcome::invalidUtf8 ||
               result == EditOutcome::full)
      frame.notice = Notice::invalid;
    break;
  }
  case Mode::actions:
    if (selected == 0)
      frame.mode = Mode::adjust;
    else if (selected == 1 && frame.total == 3) {
      int64_t step = 1;
      for (uint8_t i = 0; i < stepPower; i++)
        step *= 10;
      const int64_t range = int64_t(editingField->hi - editingField->lo) *
                            (scale == 1000 ? 10 : 1);
      if (step <= range / 10) {
        ++stepPower;
        step *= 10;
      } else {
        stepPower = 0;
        step = 1;
      }
      if (!editor.setStep(step, now))
        frame.notice = Notice::readOnly;
    } else {
      editor.apply(now);
      applyField(now);
      return;
    }
    break;
  case Mode::discard:
    if(selected){close(Notice::none);return;}
    frame.mode=Mode::fields;selected=fieldIndex;break;
  case Mode::confirm:
    if (selected) {
      save(now);
      return;
    }
    frame.mode = Mode::fields;
    selected = count();
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
      frame.mode == Mode::actions || frame.mode == Mode::secret ||
      frame.mode == Mode::replace) {
    editor.cancel();
    frame.mode = Mode::fields;
    selected = fieldIndex;
    editingField = nullptr;
    render();
  } else if (frame.mode == Mode::confirm) {
    frame.mode = Mode::fields;
    selected = count();
    render();
  } else if(frame.mode==Mode::discard) {
    frame.mode=Mode::fields;selected=fieldIndex;render();
  } else if(frame.mode==Mode::fields && changes.size()) {
    frame.mode=Mode::discard;selected=0;render();
  } else
    close(Notice::none);
}
} // namespace Settings
} // namespace LocalUi
