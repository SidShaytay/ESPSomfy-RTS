#pragma once
#include "LocalSettingsView.h"
#include "LocalValueEditor.h"
#include "SettingsApply.h"
#include "SettingsRead.h"
#include <stddef.h>
#include <stdint.h>

namespace LocalUi {
namespace Settings {
struct Authorization {
  bool allowed = false;
  uint32_t generation = 0, configurationGeneration = 0;
};
// Synchronous loop-owned boundary. Reads expose presence only for secrets.
// apply must recheck expected auth/history immediately before shared apply;
// request is validated and valid only for the duration of this call.
class Backend {
public:
  virtual ~Backend() = default;
  virtual Authorization authorization() = 0;
  virtual uint64_t revision(settings_section_t section) = 0;
  virtual bool read(settings_section_t section, SettingsSnapshotSink &sink) = 0;
  virtual settings_apply_result_t apply(settings_section_t section,
                                        JsonObject request,
                                        Authorization expected,
                                        uint64_t expectedRevision) = 0;
  virtual bool hasEthernet() const = 0;
  virtual bool fixedRadioPins() const = 0;
  virtual void activity() = 0;
};
// One changed-only draft document. Allocator must outlive the form and also
// admit a transient expanded validator document; allocation failure
// closes/wipes.
class Form {
  Backend &backend;
  ArduinoJson::Allocator &allocator;
  JsonDocument changes;
  ValueEditor editor;
  FormView frame;
  Authorization admitted;
  uint64_t admittedRevision = 0, lastActivity = 0, lastObserved = 0;
  settings_section_t section = settings_section_t::general;
  const settings_field_t *editingField = nullptr;
  uint8_t selected = 0, fieldIndex = 0, stepPower = 0;
  int32_t scale = 1;
  bool guard(uint64_t now);
  void close(Notice notice);
  void render();
  enum class FieldStart : uint8_t { existing, replaceSecret, replaceInvalid };
  void beginField(uint64_t now, FieldStart start = FieldStart::existing);
  void applyField(uint64_t now);
  void save(uint64_t now);
  bool readValue(const settings_field_t &, settings_value_t &);
  bool readOnly(const settings_field_t &);
  uint8_t count() const;
  void touch(uint64_t now);

public:
  static constexpr uint64_t timeoutMs = 120000;
  Form(Backend &backend, ArduinoJson::Allocator &allocator);
  ~Form();
  Form(const Form &) = delete;
  Form &operator=(const Form &) = delete;
  bool open(uint64_t now);
  bool openSection(settings_section_t section, uint64_t now);
  // Scan handoff stages SSID only; nullptr selects manual entry with no draft.
  bool openNetworkSSID(const char *ssid, uint64_t now, Authorization expected,
                       uint64_t expectedNetworkRevision);
  bool openRadioFrequency(float frequency,uint64_t now,Authorization expected,uint64_t expectedRadioRevision);
  void tick(uint64_t now);
  void turn(int direction, uint64_t now);
  void activate(uint64_t now);
  void back(uint64_t now);
  // Out-of-band teardown: erase every draft without consulting auth or time.
  void discard() { close(Notice::none); }
  const FormView &view() const { return frame; }
  bool closed() const { return frame.mode == Mode::closed; }
};
static_assert(sizeof(Form) <= 1024, "Keep local form state bounded");
} // namespace Settings
} // namespace LocalUi
