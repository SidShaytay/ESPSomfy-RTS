#include "LocalEntityView.h"
#include "LocalUiRenderer.h"
#include <assert.h>
#include <fstream>
#include <string.h>
#include <vector>
using namespace LocalUi;
static void save(Canvas &canvas, uint16_t *pixels,
                 const EntityAdmin::FormView &view, const char *out,
                 const char *name) {
  canvas.paintEntity(view);
  std::ofstream file(std::string(out) + "/" + name + ".ppm", std::ios::binary);
  file << "P6\n320 170\n255\n";
  for (unsigned i = 0; i < 320 * 170; i++) {
    unsigned c = pixels[i];
    char rgb[3] = {char((c >> 11) * 255 / 31), char(((c >> 5) & 63) * 255 / 63),
                   char((c & 31) * 255 / 31)};
    file.write(rgb, 3);
  }
}
int main(int argc, char **argv) {
  assert(argc == 2);
  Canvas canvas;
  std::vector<uint16_t> pixels(320 * 170);
  canvas.attach(pixels.data());
  EntityAdmin::FormView view;
  view.mode = EntityAdmin::Mode::fields;
  view.id = 4;
  strcpy(view.title, "Edit Room 4");
  view.total = 5;
  view.rowCount = 3;
  strcpy(view.rows[0].label, "Name");
  strcpy(view.rows[0].value, "Office / \xe2\x98\x83");
  view.rows[0].selected = true;
  strcpy(view.rows[1].label, "Sort order");
  strcpy(view.rows[1].value, "2");
  strcpy(view.rows[2].label, "Save");
  strcpy(view.hint, "Apply fields to draft, then Save. Back discards.");
  save(canvas, pixels.data(), view, argv[1], "entity-room-fields");
  view.mode = EntityAdmin::Mode::confirm;
  view.rowCount = 2;
  strcpy(view.rows[0].label, "Cancel");
  view.rows[0].value[0] = 0;
  strcpy(view.rows[1].label, "Delete");
  view.rows[1].value[0] = 0;
  strcpy(view.hint, "Delete Room 4? Confirm this exact ID.");
  save(canvas, pixels.data(), view, argv[1], "entity-room-confirm");
  const auto before = pixels;
  view.notice = EntityAdmin::Notice::busy;
  save(canvas, pixels.data(), view, argv[1],
       "entity-room-confirm-stale-notice");
  assert(before == pixels); // The action-specific warning always wins.
  view.mode = EntityAdmin::Mode::replace;
  strcpy(view.title, "Room 9: Sort order");
  strcpy(view.rows[1].label, "Replace invalid value");
  strcpy(view.hint, "Original kept until Apply; Save persists replacement.");
  save(canvas, pixels.data(), view, argv[1], "entity-room-replace-invalid");
  const auto replacement = pixels;
  view.notice = EntityAdmin::Notice::failed;
  save(canvas, pixels.data(), view, argv[1],
       "entity-room-replace-stale-notice");
  assert(replacement == pixels);
  view.mode = EntityAdmin::Mode::result;
  view.notice = EntityAdmin::Notice::appliedNotSaved;
  strcpy(view.title, "Room 9 file result");
  strcpy(view.rows[0].label, "Close");
  strcpy(view.rows[1].label, "Retry model file save");
  strcpy(view.hint, "Room 9 applied. Retry saves current model file only.");
  save(canvas, pixels.data(), view, argv[1], "entity-room-file-failed");
  view.notice = EntityAdmin::Notice::uncertain;
  save(canvas, pixels.data(), view, argv[1], "entity-room-file-uncertain");
  view.mode = EntityAdmin::Mode::text;
  strcpy(view.title,"Edit Room 9");
  view.notice = EntityAdmin::Notice::none;
  view.rowCount = 3;
  strcpy(view.rows[0].label, "Name");
  strcpy(view.rows[0].value, "12345678901234567END");
  strcpy(view.rows[1].label, "Character");
  strcpy(view.rows[1].value, "Apply to draft");
  view.rows[2] = {};
  strcpy(view.hint, "20/20 bytes. Apply to draft; Back cancels.");
  save(canvas, pixels.data(), view, argv[1], "entity-room-text");
  // A legal 20-byte name can still exceed the viewport in the real font.
  strcpy(view.rows[0].value, "WWWWWWWWWWWWWWWWWEND");
  assert(strlen(view.rows[0].value) == 20);
  save(canvas, pixels.data(), view, argv[1], "entity-room-wide-tail");
  const auto wide = pixels;
  char expected[48];
  const char *start = view.rows[0].value;
  int16_t x, y;
  uint16_t width, height;
  canvas.getTextBounds(start, 0, 0, &x, &y, &width, &height);
  assert(width > 296);
  do {
    ++start;
    snprintf(expected, sizeof(expected), "...%s", start);
    canvas.getTextBounds(expected, 0, 0, &x, &y, &width, &height);
  } while (width > 296);
  assert(width <= 296 && !strcmp(expected + strlen(expected) - 3, "END"));
  strcpy(view.rows[0].value, expected);
  save(canvas, pixels.data(), view, argv[1], "entity-room-wide-tail-expected");
  assert(wide ==
         pixels); // Compare the complete actual render, not source text.
  strcpy(view.rows[0].value, "WWWWWWWWWWWWWWWWWXYZ");
  save(canvas, pixels.data(), view, argv[1], "entity-room-wide-tail-alternate");
  bool lastChanged = false;
  for (unsigned row = 65; row < 86; row++)
    for (unsigned col = 235; col < 309; col++)
      lastChanged |= pixels[row * 320 + col] != wide[row * 320 + col];
  assert(lastChanged); // The newest suffix reaches the visible right edge.
  view = {};
  view.kind = EntityAdmin::Kind::shade;
  view.id = 4;
  view.mode = EntityAdmin::Mode::validation;
  view.rowCount = view.total = 2;
  strcpy(view.title, "Shade 4: Up GPIO");
  strcpy(view.rows[0].label, "Revise field"); view.rows[0].selected = true;
  strcpy(view.rows[1].label, "Cancel form");
  strcpy(view.hint, "Revert inactive GPIO draft or select GPIO protocol.");
  save(canvas, pixels.data(), view, argv[1], "entity-device-refused");
  const auto refusal = pixels;
  view.notice = EntityAdmin::Notice::busy;
  save(canvas, pixels.data(), view, argv[1], "entity-device-refused-stale-notice");
  assert(refusal == pixels); // Never hide the current actionable reason.
  strcpy(view.hint, "Revert inactive GPIO draft or select radio protocol.");
  save(canvas, pixels.data(), view, argv[1], "entity-device-refused-alternate");
  bool reasonChanged = false;
  for (unsigned row = 128; row < 154; row++)
    for (unsigned col = 12; col < 308; col++)
      reasonChanged |= pixels[row * 320 + col] != refusal[row * 320 + col];
  assert(reasonChanged); // The last words reach the actual second display line.
  view.mode = EntityAdmin::Mode::draft;
  view.notice = EntityAdmin::Notice::none;
  view.rowCount = view.total = 3;
  strcpy(view.rows[0].label, "Cancel");
  strcpy(view.rows[1].label, "Edit draft");
  strcpy(view.rows[2].label, "Revert field draft");
  strcpy(view.hint, "Revert keeps other drafts.");
  save(canvas, pixels.data(), view, argv[1], "entity-device-draft");
  view.mode = EntityAdmin::Mode::confirm;
  view.rowCount = view.total = 2;
  strcpy(view.title, "Save Shade 4");
  strcpy(view.rows[1].label, "Save");
  strcpy(view.hint, "May change live GPIO, motor timing and RF identity.");
  save(canvas, pixels.data(), view, argv[1], "entity-device-save-confirm");
  puts("Production entity renderer: fields, identity confirmation, complete "
       "applied/file-only failure and text states passed.");
}
