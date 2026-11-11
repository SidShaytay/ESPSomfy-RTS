#if defined(ESPSOMFY_LOCAL_UI) || defined(LOCAL_UI_HOST_PREVIEW)
#include "LocalEntityView.h"
#include "LocalText.h"
#include "LocalUiRenderer.h"
#include <stdio.h>
#include <string.h>
namespace LocalUi {
namespace {
constexpr uint16_t rgb(unsigned c) {
  return ((c >> 8) & 0xf800) | ((c >> 5) & 0x07e0) | ((c >> 3) & 0x001f);
}
constexpr uint16_t background = rgb(0x111719), foreground = rgb(0xeaece8),
                   muted = rgb(0x929d9b), accent = rgb(0x66c6b5),
                   warning = rgb(0xe5bc67), selection = rgb(0x21433f),
                   divider = rgb(0x3a4848);
const char *notice(EntityAdmin::Notice n) {
  using EntityAdmin::Notice;
  switch (n) {
  case Notice::none:
    return "";
  case Notice::saved:
    return "Model file saved";
  case Notice::appliedNotSaved:
    return "Applied; model file not saved";
  case Notice::uncertain:
    return "Applied; model file save uncertain";
  case Notice::invalid:
    return "Invalid value - original retained";
  case Notice::changed:
    return "Configuration changed - reopen form";
  case Notice::locked:
    return "Unlock required";
  case Notice::expired:
    return "Edit expired - draft discarded";
  case Notice::noMemory:
    return "No draft memory - draft discarded";
  case Notice::unchanged:
    return "No changes to save";
  case Notice::failed:
    return "Edit refused - changes not applied";
  case Notice::busy:
    return "Busy - wait and try again";
  case Notice::capacity:
    return "No free slots. Delete an entry.";
  }
  return "Entity unavailable";
}
// Use the body font selected by the preceding text call. Pixel width, not
// byte count, decides whether a prefix must be removed. lcdLabel emits ASCII.
void tail(Adafruit_GFX &canvas, const char *input, char (&out)[48],
          unsigned limit) {
  char clean[48];
  lcdLabel(input, clean, sizeof(clean));
  for (const char *start = clean;; ++start) {
    if (start != clean && strlen(start) > sizeof(out) - 4)
      continue;
    snprintf(out, sizeof(out), "%s%s", start == clean ? "" : "...", start);
    int16_t x, y;
    uint16_t width, height;
    canvas.getTextBounds(out, 0, 0, &x, &y, &width, &height);
    if (width <= limit || !*start)
      return;
  }
}
void lines(Adafruit_GFX &canvas, const char *source, char (&first)[48],
           char (&second)[48]) {
  char clean[64];
  lcdLabel(source, clean, sizeof(clean));
  size_t count = strlen(clean);
  if (count > 47)
    count = 47;
  int16_t x, y;
  uint16_t w, h;
  while (count) {
    memcpy(first, clean, count);
    first[count] = 0;
    canvas.getTextBounds(first, 0, 0, &x, &y, &w, &h);
    if (w <= 296)
      break;
    --count;
  }
  if (clean[count]) {
    size_t word = count;
    while (word && clean[word] != ' ')
      --word;
    if (word)
      count = word;
  }
  memcpy(first, clean, count);
  first[count] = 0;
  const char *rest = clean + count;
  while (*rest == ' ')
    ++rest;
  snprintf(second, sizeof(second), "%s", rest);
}
} // namespace
void Canvas::paintEntity(const EntityAdmin::FormView &view) {
  using EntityAdmin::Mode;
  using EntityAdmin::Notice;
  fillScreen(background);
  const bool numbered = (view.mode == Mode::fields || view.mode == Mode::stackedRows) && view.total > 3;
  text(12, 24, view.title[0] ? view.title : "Entity administration", true,
       accent, numbered || view.mode==Mode::wifiList ? 230 : 296);
  if (numbered) {
    char progress[12];
    snprintf(progress, sizeof(progress), "%u/%u", unsigned(view.selected) + 1, unsigned(view.total));
    text(256, 24, progress, false, muted, 52);
  }
  drawFastHLine(12, 33, 296, divider);
  if (view.mode == Mode::closed) {
    char first[48], second[48];
    lines(*this, notice(view.notice), first, second);
    text(12, 78, first, false, warning, 296);
    text(12, 104, second, false, warning, 296);
    text(12, 161, "Press or Back: return", false, muted, 296);
    return;
  }
  if(view.mode==Mode::updateStatus) {
    // Version and label share one line. Keep the lower two lines for a complete
    // update failure/status hint rather than crowding the next selected action.
    for(uint8_t i=0;i<view.rowCount && i<3;++i) {
      const auto &row=view.rows[i];const int top=38+i*28;
      if(row.selected)fillRoundRect(8,top,304,26,4,selection);
      text(14,top+19,row.label,false,foreground,row.value[0]?176:284);
      text(200,top+19,row.value,false,row.selected?accent:muted,106);
    }
    char first[48],second[48];lines(*this,view.hint,first,second);
    text(12,143,first,false,muted,296);text(12,166,second,false,muted,296);return;
  }
  if(view.mode==Mode::wifiList) {
    char progress[12];snprintf(progress,sizeof(progress),"%u/%u",unsigned(view.selected)+1,unsigned(view.total));
    text(256,24,progress,false,muted,52);
    for(uint8_t i=0;i<view.rowCount && i<4;++i) {
      const auto &row=view.rows[i];const int top=36+i*32;
      if(row.selected)fillRoundRect(8,top,304,30,4,selection);
      if(!row.value[0]) {text(14,top+21,row.label,false,foreground,284);continue;}
      const char *security=strrchr(row.label,' ');
      char signal[16]={};
      const size_t length=security?size_t(security-row.label):strlen(row.label);
      const size_t bounded=length<sizeof(signal)-1?length:sizeof(signal)-1;
      memcpy(signal,row.label,bounded);
      text(12,top+21,"",false,foreground,296); // Select body font for measurement.
      int16_t x,y;uint16_t width,height;getTextBounds(signal,0,0,&x,&y,&width,&height);
      const int signalX=292-width;
      text(14,top+21,row.value,false,foreground,signalX-22);
      text(signalX,top+21,signal,false,row.selected?accent:muted,width+2);
      if(security && !strcmp(security+1,"Secured")) {
        drawRoundRect(298,top+13,10,9,1,muted);
        drawRoundRect(300,top+7,6,10,3,muted);
      } else if(security && !strcmp(security+1,"Unknown"))text(298,top+21,"?",false,muted,10);
    }
    return;
  }
  if (view.mode == Mode::stackedRows) {
    for(uint8_t i=0;i<view.rowCount && i<3;i++) {
      const int top=36+i*29;
      if(view.rows[i].selected)fillRoundRect(8,top,304,28,4,selection);
      text(14,top+14,view.rows[i].label,false,foreground,284);
      text(14,top+28,view.rows[i].value,false,muted,284);
    }
    char first[48],second[48];lines(*this,view.hint,first,second);
    text(12,143,first,false,muted,296);text(12,166,second,false,muted,296);return;
  }
  if (view.mode == Mode::wifiReview) {
    text(12,47,"",false,foreground,296); // Select body font before measuring both SSID lines.
    char first[48],second[48];lines(*this,view.rows[2].value,first,second);
    text(12,47,first,false,foreground,296);text(12,64,second,false,foreground,296);
    text(12,81,view.rows[2].label,false,muted,296);
    text(12,98,view.rows[0].value,false,muted,296);
    for(uint8_t i=0;i<2;i++) {
      const int top=103+i*17;if(view.rows[i].selected)fillRoundRect(8,top,304,17,4,selection);
      text(14,top+12,view.rows[i].label,false,foreground,284);
    }
    lines(*this,view.hint,first,second);text(12,149,first,false,warning,296);text(12,166,second,false,warning,296);
    return;
  }
  if(view.mode==Mode::membershipReview) {
    text(12,51,view.rows[0].value,false,muted,296);
    text(12,69,view.rows[1].value,false,muted,296);
    text(12,87,view.rows[2].label,false,foreground,296);
    for(uint8_t i=0;i<2;++i){const int top=95+i*20;
      if(view.rows[i].selected)fillRoundRect(8,top,304,20,4,selection);
      text(14,top+15,view.rows[i].label,false,foreground,284);
    }
    char first[48],second[48];lines(*this,view.hint,first,second);
    text(12,148,first,false,warning,296);text(12,166,second,false,warning,296);return;
  }
  if(view.mode==Mode::learnReview || view.mode==Mode::counterReview || view.mode==Mode::programmingReview || view.mode==Mode::groupProgrammingReview || view.mode==Mode::sensorReview || view.mode==Mode::updateReview) {
    // The complete target ID/title and received address/code/format are part
    // of the same protected frame identity as the default-Cancel actions.
    text(12,53,view.rows[2].label,false,foreground,296);
    char first[48],second[48];lines(*this,view.rows[2].value,first,second);
    text(12,71,first,false,muted,296);text(12,89,second,false,muted,296);
    for(uint8_t i=0;i<2;i++) {
      const int top=95+i*20;if(view.rows[i].selected)fillRoundRect(8,top,304,20,4,selection);
      text(14,top+15,view.rows[i].label,false,foreground,284);
    }
    lines(*this,view.hint,first,second);
    text(12,148,first,false,warning,296);text(12,166,second,false,warning,296);return;
  }
  if (view.mode == Mode::result) {
    text(12, 53, notice(view.notice), false,
         view.notice == Notice::saved ? accent : warning, 296);
    char first[48], second[48];
    lines(*this, view.hint, first, second);
    text(12, 74, first, false, muted, 296);
    text(12, 94, second, false, muted, 296);
    for (uint8_t i = 0; i < view.rowCount && i < 2; i++) {
      const auto &r = view.rows[i];
      int top = 104 + i * 31;
      if (r.selected)
        fillRoundRect(8, top, 304, 29, 4, selection);
      text(14, top + 20, r.label, false, foreground, 284);
    }
    return;
  }
  if(view.mode==Mode::text) {
    char visible[48];tail(*this,view.rows[0].value,visible,246);
    keyboard(view.keyboard,visible,view.textLength,view.textLimit);return;
  }
  const bool editor = view.mode == Mode::text || view.mode == Mode::adjust;
  if (editor) {
    text(12, 55, view.rows[0].label, false, muted, 296);
    char visible[48];
    if (view.mode == Mode::text)
      tail(*this, view.rows[0].value, visible, 296);
    else
      snprintf(visible, sizeof(visible), "%s", view.rows[0].value);
    text(12, 84, visible, false, foreground, 296);
    text(12, 115, view.rows[1].label, false, muted, 128);
    text(146, 115, view.rows[1].value, false, accent, 160);
    text(12, 137, view.rows[2].label, false, muted, 296);
  } else
    for (uint8_t i = 0; i < view.rowCount && i < 3; i++) {
      const auto &r = view.rows[i];
      int top = 40 + i * 33;
      if (r.selected)
        fillRoundRect(8, top - 2, 304, 31, 4, selection);
      text(14, top + 18, r.label, false, foreground, r.value[0] ? 169 : 284);
      text(186, top + 18, r.value, false, r.selected ? accent : muted, 113);
      if (r.dirty)
        fillCircle(304, top + 13, 2, warning);
    }
  if (view.mode == Mode::confirm || view.mode == Mode::replace || view.mode == Mode::validation || view.mode == Mode::discard) {
    char first[48], second[48];
    lines(*this, view.hint, first, second);
    text(12, 126, first, false, warning, 296);
    text(12, 150, second, false, warning, 296);
    return;
  }
  drawFastHLine(12, 142, 296, divider);
  text(12, 161,
       editor                        ? view.hint
       : view.notice == Notice::none ? view.hint
                                     : notice(view.notice),
       false, view.notice == Notice::none ? muted : warning, 296);
}
} // namespace LocalUi
#endif
