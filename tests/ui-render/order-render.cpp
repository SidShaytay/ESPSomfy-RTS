#include "LocalEntityView.h"
#include "LocalUiRenderer.h"
#include <assert.h>
#include <fstream>
#include <stdio.h>
#include <string.h>
#include <vector>
using namespace LocalUi;
using namespace LocalUi::EntityAdmin;
static void save(Canvas &canvas, const std::vector<uint16_t> &pixels,
                 const FormView &view, const char *out, const char *name) {
  canvas.paintEntity(view);
  std::ofstream file(std::string(out) + "/" + name + ".ppm", std::ios::binary);
  file << "P6\n320 170\n255\n";
  for (auto color : pixels) {
    const char rgb[] = {char((color >> 11) * 255 / 31),
                        char(((color >> 5) & 63) * 255 / 63),
                        char((color & 31) * 255 / 31)};
    file.write(rgb, 3);
  }
}
int main(int argc, char **argv) {
  assert(argc == 2);
  Canvas canvas;
  std::vector<uint16_t> pixels(320 * 170);
  canvas.attach(pixels.data());
  FormView view;
  view.kind = Kind::shade;
  view.mode = Mode::fields;
  view.total = 34;
  view.selected = 31;
  view.offset = 30;
  view.rowCount = 3;
  view.id = 32;
  view.dirty = true;
  strcpy(view.title, "Reorder devices");
  strcpy(view.rows[0].label, "#31 Duplicate name");
  strcpy(view.rows[1].label, "#32 WWWWWWWWWWWWWWWWWEND");
  view.rows[1].selected = true;
  strcpy(view.rows[2].label, "Save order");
  strcpy(view.hint, "Save numbers from 0. Back: discard");
  save(canvas, pixels, view, argv[1], "order-full-list");
  const char *footers[] = {"Turn: move Press: apply Back: undo",
                           "Save numbers from 0. Back: discard",
                           "Press: pick/Save Back: discard"};
  for (auto *text : footers) {
    int16_t x, y;
    uint16_t width, height;
    canvas.getTextBounds(text, 0, 0, &x, &y, &width, &height);
    printf("%u px: %s\n", unsigned(width), text);
    fflush(stdout);
    assert(width <= 296);
  }
  strcpy(view.title, "Move #32 in devices");
  view.total = 32;
  view.rowCount = 2;
  view.selected = 30;
  view.rows[0].selected = true;
  strcpy(view.rows[0].label, "#32 Duplicate name");
  view.rows[1].selected = false;
  strcpy(view.rows[1].label, "#31 Duplicate name");
  strcpy(view.hint, footers[0]);
  save(canvas, pixels, view, argv[1], "order-move-draft");
  view = {};
  view.kind = Kind::shade;
  view.mode = Mode::confirm;
  view.total = view.rowCount = 2;
  strcpy(view.title, "Reorder devices");
  strcpy(view.rows[0].label, "Cancel");
  view.rows[0].selected = true;
  strcpy(view.rows[1].label, "Save order");
  strcpy(view.hint, "Reassigns list positions only. No motor command.");
  save(canvas, pixels, view, argv[1], "order-save-confirm");
  const auto confirm = pixels;
  view.notice = EntityAdmin::Notice::busy;
  canvas.paintEntity(view);
  assert(confirm == pixels);
  view.mode = Mode::result;
  view.notice = EntityAdmin::Notice::appliedNotSaved;
  strcpy(view.rows[0].label, "Close");
  strcpy(view.rows[1].label, "Retry model file");
  strcpy(view.hint, "Applied. Retry saves file only.");
  save(canvas, pixels, view, argv[1], "order-file-retry");
}
