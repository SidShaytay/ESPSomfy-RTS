#pragma once
#include <stdint.h>
#include "RestartTypes.h"
namespace LocalUi {
namespace Settings {
enum class Mode : uint8_t {
  closed,
  sections,
  fields,
  secret,
  adjust,
  text,
  actions,
  confirm,
  replace
};
enum class Notice : uint8_t {
  none,
  saved,
  failed,
  partial,
  invalid,
  kept,
  changed,
  locked,
  expired,
  noMemory,
  readOnly,
  unchanged,
  busy,
  savedRestartBlocked
};
struct FormRow {
  char label[32] = {}, value[48] = {};
  bool selected = false, readOnly = false, dirty = false;
};
struct FormView {
  char title[32] = {}, hint[64] = {};
  FormRow rows[3];
  uint8_t rowCount = 0, selected = 0, total = 0, offset = 0;
  Mode mode = Mode::closed;
  Notice notice = Notice::none;
  RestartReason restartReason = RestartReason::none;
  bool protectedFrame = true, dirty = false;
};
} // namespace Settings
} // namespace LocalUi
