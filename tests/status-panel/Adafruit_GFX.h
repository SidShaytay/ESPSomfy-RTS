#pragma once
#include <stdint.h>
extern bool allocationOK;
// Only the text rasterizer is stubbed; the real SPI/lifecycle driver is compiled.
struct GFXcanvas1 {
  uint8_t buffer[720];
  GFXcanvas1(int, int) {}
  uint8_t *getBuffer() { return allocationOK ? buffer : nullptr; }
  void fillScreen(int) {}
  void setCursor(int, int) {}
  void setTextColor(int) {}
  void setTextSize(int) {}
  void setTextWrap(bool) {}
  void print(const char *) {}
  bool getPixel(int x, int y) { return (x + y) % 2; }
};
