#if defined(ESPSOMFY_LOCAL_UI) || defined(LOCAL_UI_HOST_PREVIEW)
#include "LocalUiRenderer.h"
#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeSansBold12pt7b.h>
#include <stdio.h>
#include <string.h>
namespace LocalUi {
namespace {
constexpr uint16_t rgb(unsigned c) { return ((c >> 8) & 0xf800) | ((c >> 5) & 0x07e0) | ((c >> 3) & 0x001f); }
constexpr uint16_t bg = rgb(0x111719), white = rgb(0xeaece8), muted = rgb(0x929d9b), teal = rgb(0x66c6b5),
                   amber = rgb(0xe5bc67), selected = rgb(0x21433f), line = rgb(0x3a4848);
const char *notices[] = {"",
                         "Preview only - no commands",
                         "Command queued",
                         "Radio unavailable / refused",
                         "STOP unavailable while idle",
                         "Target changed - reopen",
                         "Wait for screen, then press",
                         "Display settings saved",
                         "Display settings save failed"};
} // namespace
void Canvas::drawPixel(int16_t x, int16_t y, uint16_t color) {
  if(pixels && x >= 0 && x < screenWidth && y >= 0 && y < screenHeight)
    pixels[y * screenWidth + x] = color;
}
void Canvas::fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
  int x1 = x + w, y1 = y + h;
  if(x < 0)
    x = 0;
  if(y < 0)
    y = 0;
  if(x1 > screenWidth)
    x1 = screenWidth;
  if(y1 > screenHeight)
    y1 = screenHeight;
  if(!pixels)
    return;
  for(int yy = y; yy < y1; ++yy)
    for(int xx = x; xx < x1; ++xx)
      pixels[yy * screenWidth + xx] = color;
}
void Canvas::text(int x, int y, const char *s, bool title, uint16_t color, int maxWidth) {
  char label[48];
  snprintf(label, sizeof(label), "%.47s", s);
  setFont(title ? &FreeSansBold12pt7b : &FreeSans9pt7b);
  setTextSize(1);
  setTextColor(color);
  setTextWrap(false);
  int16_t bx, by;
  uint16_t w, h;
  getTextBounds(label, 0, 0, &bx, &by, &w, &h);
  if(w > maxWidth) {
    size_t n = strlen(label);
    do {
      if(n <= 3) {
        label[0] = 0;
        break;
      }
      --n;
      label[n - 3] = '.';
      label[n - 2] = '.';
      label[n - 1] = '.';
      label[n] = 0;
      getTextBounds(label, 0, 0, &bx, &by, &w, &h);
    } while(w > maxWidth);
  }
  setCursor(x, y);
  print(label);
}
void Canvas::shutter(int x, int y, int open, uint16_t color) {
  drawRoundRect(x, y, 28, 32, 2, color);
  if(open < 0) {
    drawLine(x + 8, y + 8, x + 20, y + 24, muted);
    drawLine(x + 20, y + 8, x + 8, y + 24, muted);
    return;
  }
  for(int yy = 4; yy < 28 * (100 - open) / 100; yy += 4)
    drawFastHLine(x + 4, y + yy, 20, color);
}
void Canvas::paint(const Model &m, const View &v, const StatusDisplay::Snapshot &s) {
  fillScreen(bg);
  char label[48];
  if(v.screen == Screen::details && s.running) {
    StatusDisplay::Page page;
    StatusDisplay::format(s, page);
    text(12, 28, "Details", true, white);
    drawFastHLine(12, 38, 296, line);
    const unsigned last = detailsRowCount - detailsVisibleRows;
    const unsigned offset = v.detailsOffset > last ? last : v.detailsOffset;
    for(unsigned row = 0; row < detailsVisibleRows; ++row) {
      const unsigned index = offset + row;
      if(index == 0)
        snprintf(label, sizeof(label), "Firmware: %s", s.firmwareVersion[0] ? s.firmwareVersion : "unavailable");
      else if(index == 1)
        snprintf(label, sizeof(label), "Application: %s", s.applicationVersion[0] ? s.applicationVersion : "unavailable");
      else
        snprintf(label, sizeof(label), "%s", page.lines[index - 2]);
      text(12, 58 + row * 18, label, false, index < 2 ? white : muted);
    }
    snprintf(label, sizeof(label), "Turn: scroll   Back   %u-%u/%u", offset + 1,
             offset + detailsVisibleRows, unsigned(detailsRowCount));
    text(12, 166, label, false, teal);
    return;
  }
  if(!s.running) {
    StatusDisplay::Page page;
    StatusDisplay::format(s, page);
    for(int i = 0; i < 9; i++)
      text(8, 16 + i * 18, page.lines[i], false, i == 0 ? teal : muted);
    return;
  }
  if(v.screen == Screen::status) {
    text(12, 29, "Shutters", true, white);
    drawFastHLine(12, 40, 296, line);
    // Simple radio aerial and Wi-Fi bars; no unsupported battery/USB glyph.
    drawLine(22, 57, 22, 77, teal);
    drawLine(16, 79, 22, 66, teal);
    drawLine(28, 79, 22, 66, teal);
    drawCircle(22, 57, 3, teal);
    const bool radio = s.radio == StatusDisplay::Radio::ready;
    text(40, 67, radio ? "Radio ready" : "Radio offline", false, radio ? white : amber, 132);
    snprintf(label, sizeof(label), "%.2f MHz", s.frequency);
    text(40, 88, label, false, muted, 132);
    const char *wifi = s.link == StatusDisplay::Link::connected     ? "Connected"
                       : s.link == StatusDisplay::Link::accessPoint ? "Access point"
                       : s.link == StatusDisplay::Link::connecting  ? "Connecting"
                                                                    : "Disconnected";
    text(194, 63, "Wi-Fi", false, muted, 110);
    text(194, 86, wifi, false, s.link == StatusDisplay::Link::disconnected ? amber : white, 110);
    for(int i = 0; i < 3; i++)
      fillRect(282 + i * 7, 27 - i * 5, 4, 5 + i * 5,
               s.link == StatusDisplay::Link::connected ? teal : muted);
    drawFastHLine(12, 103, 296, line);
    const bool address =
        s.link == StatusDisplay::Link::connected || s.link == StatusDisplay::Link::accessPoint;
    text(12, 125, address && s.address[0] ? s.address : "No IP address", false, white, 190);
    snprintf(label, sizeof(label), "Up %luh%02um", (unsigned long)(s.uptimeSeconds / 3600 % 10000),
             (unsigned)(s.uptimeSeconds / 60 % 60));
    text(207, 125, label, false, muted, 102);
    text(12, 146,
         s.web == StatusDisplay::Web::listening  ? "HTTP listening"
         : s.web == StatusDisplay::Web::starting ? "HTTP starting"
                                                 : "HTTP error",
         false, s.web == StatusDisplay::Web::error ? amber : muted, 170);
    if(s.battery.state == BatteryGauge::State::ready) {
      snprintf(label, sizeof(label), "Battery %u%%", unsigned(s.battery.percent));
      text(194, 146, label, false, s.battery.percent <= 15 ? amber : muted, 114);
    }
    text(12, 164, "Turn: choose   Hold: details", false, muted);
  } else if(v.screen == Screen::choose) {
    text(12, 28, "Choose shutters", true, white);
    const int first = v.selected >= 3 ? v.selected - 2 : 0;
    for(int row = 0; row < 3; row++) {
      int i = first + row, y = 40 + row * 34;
      bool active = i == v.selected;
      fillRoundRect(10, y, 300, 30, 5, active ? selected : bg);
      drawRoundRect(10, y, 300, 30, 5, active ? teal : line);
      if(active)
        fillRoundRect(10, y, 4, 30, 2, teal);
      if(i < 3) {
        const auto &t = m.targets[i];
        uint16_t color = i == 2 ? amber : muted;
        drawRect(21, y + 7, 14, 16, color);
        for(int yy = 11; yy < 22; yy += 4)
          drawFastHLine(24, y + yy, 8, color);
        if(i == 2)
          drawRect(38, y + 7, 14, 16, amber);
        text(i == 2 ? 62 : 48, y + 21, t.name[0] ? t.name : "Not configured", false, t.valid ? white : muted,
             i == 2 ? 236 : 250);
      } else
        text(48, y + 21, i == 3 ? "Status" : "Display settings", false, white);
    }
    text(12, 164, v.notice == Notice::none ? "Turn: select   Press: open" : notices[int(v.notice)], false,
         v.notice == Notice::none ? muted : teal);
  } else if(v.screen == Screen::brightness) {
    text(12, 28, "Display settings", true, white);
    const char *labels[] = {"Brightness", "Dim after", "Sleep after"};
    for(int i = 0; i < 3; i++) {
      const int y = 42 + i * 34;
      const bool active = v.setting == i;
      fillRoundRect(10, y, 300, 30, 5, active ? selected : bg);
      drawRoundRect(10, y, 300, 30, 5, active ? teal : line);
      text(20, y + 21, labels[i], false, white, 160);
      if(i == 0)
        snprintf(label, sizeof(label), "%u%%", unsigned((v.level * 100 + 127) / 255));
      else {
        const auto seconds = i == 1 ? v.dimSeconds : v.offSeconds;
        if(!seconds)
          snprintf(label, sizeof(label), "%s", i == 1 ? "No dim" : "No sleep");
        else
          snprintf(label, sizeof(label), "%us", unsigned(seconds));
      }
      text(216, y + 21, label, false, active ? teal : muted, 86);
    }
    text(12, 164, v.setting == 2 ? "Press: save   Back: save" : "Press: next   Back: save", false, muted);
  } else {
    const auto &t = m.targets[v.selected];
    text(12, 28, t.name[0] ? t.name : "Not configured", true, white);
    if(v.selected == 2) {
      for(int i = 0; i < 2; i++) {
        text(12, 52 + i * 21, m.targets[i].name[0] ? m.targets[i].name : "Not configured", false, muted, 145);
        if(t.open[i] < 0)
          snprintf(label, sizeof(label), "--");
        else
          snprintf(label, sizeof(label), "%d%%", t.open[i]);
        text(168, 52 + i * 21, label, false, white, 52);
      }
      text(228, 63, "Estimated", false, amber, 80);
    } else {
      shutter(14, 42, t.open[0], teal);
      if(t.open[0] < 0)
        snprintf(label, sizeof(label), "Position unavailable");
      else
        snprintf(label, sizeof(label), "Estimated %d%% open", t.open[0]);
      text(56, 64, label, false, muted, 252);
    }
    const char *actions[] = {"UP", "STOP", "DOWN"};
    for(int i = 0; i < 3; i++) {
      int x = 12 + i * 102;
      bool active = i == int(v.action);
      bool enabled = t.valid && (i != 1 || t.moving);
      fillRoundRect(x, 85, 94, 52, 6, active ? selected : bg);
      drawRoundRect(x, 85, 94, 52, 6, active ? teal : line);
      uint16_t color = enabled ? white : muted;
      if(i == 1)
        drawRect(x + 42, 94, 10, 10, color);
      else {
        int dir = i == 0 ? -1 : 1;
        drawLine(x + 37, 99 - dir * 4, x + 47, 99 + dir * 4, color);
        drawLine(x + 47, 99 + dir * 4, x + 57, 99 - dir * 4, color);
      }
      text(x + (i == 2 ? 19 : i == 1 ? 23 : 32), 127, actions[i], false, color, 90);
    }
    if(v.notice != Notice::none)
      text(12, 163, notices[int(v.notice)], false, amber);
    else if(!m.controlsEnabled)
      text(12, 163, "Preview: commands locked", false, amber);
    else
      text(12, 163, "Press: send   Back: return", false, muted);
  }
}
} // namespace LocalUi
#endif
