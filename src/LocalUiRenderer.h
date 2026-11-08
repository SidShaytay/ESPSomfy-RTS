#pragma once
#if defined(ESPSOMFY_LOCAL_UI) || defined(LOCAL_UI_HOST_PREVIEW)
#include <Adafruit_GFX.h>
#include "LocalUi.h"
#include "StatusDisplay.h"
namespace LocalUi {
namespace Settings { struct FormView; }
namespace EntityAdmin { struct FormView; }
constexpr int screenWidth = 320, screenHeight = 170, bandHeight = 4;
constexpr size_t framebufferBytes = screenWidth * screenHeight * sizeof(uint16_t);
class Canvas : public Adafruit_GFX {
  uint16_t *pixels = nullptr;

public:
  Canvas() : Adafruit_GFX(screenWidth, screenHeight) {}
  void attach(uint16_t *p) { pixels = p; }
  void drawPixel(int16_t x, int16_t y, uint16_t color) override;
  void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) override;
  void drawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t c) override { fillRect(x, y, w, 1, c); }
  void drawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t c) override { fillRect(x, y, 1, h, c); }
  void paint(const Model &, const View &, const StatusDisplay::Snapshot &);
  void paintSettings(const Settings::FormView &);
  void paintEntity(const EntityAdmin::FormView &);

private:
  void text(int x, int baseline, const char *, bool title = false, uint16_t color = 0xef7d,
            int maxWidth = 296);
  void shutter(int x, int y, int open, uint16_t color);
};
} // namespace LocalUi
#endif
