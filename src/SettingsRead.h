#pragma once
#include "ProtoSettings.h"
class ConfigSettings;
struct transceiver_config_t;

// One bounded typed value. Secret fields are never emitted; only the existing
// hasPassphrase/hasPassword/hasPin boolean fields expose whether they are set.
// Defaults are included here; protocol serialization may omit schema defaults.
struct settings_value_t {
  char kind = 0;
  int32_t integer = 0;
  float number = 0;
  bool boolean = false;
  char text[65] = {};
};
static_assert(sizeof(settings_value_t) <= 80, "Keep one settings read value bounded");
class SettingsSnapshotSink {
public:
  virtual ~SettingsSnapshotSink() = default;
  virtual void field(proto_field_t field, const settings_value_t &value) = 0;
};
// loop() only: source settings/radio remain stable during this bounded read.
// No allocation, persistence, commands or secret copies. A sink must copy any
// values it retains; the supplied reference is valid only during field().
// False for an invalid section or a structurally unavailable required namespace,
// without emitting any fields. Readable invalid originals remain explicit.
bool readSettingsSnapshot(settings_section_t section, const ConfigSettings &settings,
                          const transceiver_config_t &radio, SettingsSnapshotSink &sink);
