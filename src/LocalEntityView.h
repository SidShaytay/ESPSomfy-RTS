#pragma once
#include <stdint.h>
namespace LocalUi {
namespace EntityAdmin {
enum class Kind : uint8_t { room, shade, group };
enum class Mode : uint8_t {
  closed,
  fields,
  adjust,
  text,
  actions,
  confirm,
  result,
  replace,
  draft,
  validation,
  wifiReview,
  stackedRows
};
enum class Notice : uint8_t {
  none,
  saved,
  appliedNotSaved,
  uncertain,
  invalid,
  changed,
  locked,
  expired,
  noMemory,
  unchanged,
  failed,
  busy,
  capacity
};
struct FormRow {
  char label[32] = {}, value[48] = {};
  bool selected = false, dirty = false;
};
struct FormView {
  char title[32] = {}, hint[64] = {};
  FormRow rows[3];
  uint8_t rowCount = 0, selected = 0, total = 0, offset = 0, id = 0;
  Kind kind = Kind::room;
  Mode mode = Mode::closed;
  Notice notice = Notice::none;
  bool protectedFrame = true, dirty = false;
};
} // namespace EntityAdmin
} // namespace LocalUi
