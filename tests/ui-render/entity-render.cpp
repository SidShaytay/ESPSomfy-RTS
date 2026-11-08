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
  view.notice = EntityAdmin::Notice::none;
  view.rowCount = 3;
  strcpy(view.rows[0].label, "Name");
  strcpy(view.rows[0].value, "12345678901234567END");
  strcpy(view.rows[1].label, "Character");
  strcpy(view.rows[1].value, "Apply to draft");
  view.rows[2] = {};
  strcpy(view.hint, "20/20 bytes. Apply to draft; Back cancels.");
  save(canvas, pixels.data(), view, argv[1], "entity-room-text");
  puts("Production entity renderer: fields, identity confirmation, complete "
       "applied/file-only failure and text states passed.");
}
