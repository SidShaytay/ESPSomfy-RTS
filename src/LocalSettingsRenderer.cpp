#if defined(ESPSOMFY_LOCAL_UI) || defined(LOCAL_UI_HOST_PREVIEW)
#include "LocalUiRenderer.h"
#include "LocalSettingsView.h"
#include "LocalText.h"
#include <string.h>
#include <stdio.h>
namespace LocalUi {
namespace {
constexpr uint16_t rgb(unsigned c) { return ((c>>8)&0xf800)|((c>>5)&0x07e0)|((c>>3)&0x001f); }
constexpr uint16_t background=rgb(0x111719),foreground=rgb(0xeaece8),muted=rgb(0x929d9b),
                   accent=rgb(0x66c6b5),warning=rgb(0xe5bc67),selection=rgb(0x21433f),divider=rgb(0x3a4848);
const char *notice(Settings::Notice value) {
  switch(value) {
  case Settings::Notice::saved: return "Settings saved";
  case Settings::Notice::savedRestartBlocked: return "Saved; restart blocked by uncertain storage.";
  case Settings::Notice::unchanged: return "No changes to save";
  case Settings::Notice::busy: return "Busy - retry Save when actions finish";
  case Settings::Notice::failed: return "Save failed - changes not applied";
  case Settings::Notice::partial: return "Partial save: retry before restart";
  case Settings::Notice::invalid: return "Invalid value - check your entry";
  case Settings::Notice::kept: return "Stored value kept";
  case Settings::Notice::changed: return "Settings changed - reopen this form";
  case Settings::Notice::locked: return "Unlock required";
  case Settings::Notice::expired: return "Edit expired - draft discarded";
  case Settings::Notice::noMemory: return "No draft memory - changes discarded";
  case Settings::Notice::readOnly: return "Fixed by this board or connection";
  case Settings::Notice::none: return "";
  }
  return "Settings unavailable";
}
}
// Use the font selected by the preceding Canvas::text call. Keep the newest
// characters in the viewport when an appended entry exceeds the screen width.
static void tail(Adafruit_GFX &canvas,const char *input,char (&out)[48],int width) {
  char clean[48]; lcdLabel(input,clean,sizeof(clean));
  const char *start=clean; int16_t x,y;uint16_t w,h;
  for(;;) {
    if(start!=clean && strlen(start)>sizeof(out)-4) { ++start;continue; }
    snprintf(out,sizeof(out),"%s%s",start==clean?"":"...",start);
    canvas.getTextBounds(out,0,0,&x,&y,&w,&h);
    if(w<=width || !*start) return;
    ++start; // lcdLabel currently yields printable ASCII only.
  }
}
// Two lines are reserved for complete confirmation/secret-retention warnings.
static void warningLines(Adafruit_GFX &canvas,const char *input,char (&first)[48],char (&second)[48]) {
  char clean[64];lcdLabel(input,clean,sizeof(clean));
  size_t split=strlen(clean);if(split>47) split=47;
  int16_t x,y;uint16_t w,h;
  while(split) {
    memcpy(first,clean,split);first[split]=0;
    canvas.getTextBounds(first,0,0,&x,&y,&w,&h);
    if(w<=296) break;
    --split;
  }
  if(clean[split]) {
    size_t word=split;while(word && clean[word]!=' ') --word;
    if(word) split=word;
  }
  memcpy(first,clean,split);first[split]=0;
  const char *rest=clean+split;while(*rest==' ') ++rest;
  snprintf(second,sizeof(second),"%s",rest);
}
void Canvas::paintSettings(const Settings::FormView &view) {
  fillScreen(background);
  text(12,24,view.title[0]?view.title:"Settings",true,accent,246);
  if(view.total) {
    char position[12];
    snprintf(position,sizeof(position),"%u/%u",unsigned(view.selected)+1,unsigned(view.total));
    text(268,21,position,false,muted,44);
  }
  drawFastHLine(12,33,296,divider);
  if(view.mode==Settings::Mode::closed) {
    char first[48],second[48];warningLines(*this,notice(view.notice),first,second);
    text(12,78,first,false,view.notice==Settings::Notice::saved?accent:warning,296);
    text(12,104,second,false,warning,296);
    text(12,161,"Press or Back: return Home",false,muted,296);return;
  }
  const bool editing=view.mode==Settings::Mode::text || view.mode==Settings::Mode::adjust;
  const bool partial=view.notice==Settings::Notice::partial && view.mode==Settings::Mode::fields;
  if(editing) {
    text(12,55,view.rows[0].label,false,muted,296);
    char visible[48];tail(*this,view.rows[0].value,visible,296);
    text(12,84,visible,false,foreground,296);
    text(12,115,view.rows[1].label,false,muted,128);
    text(146,115,view.rows[1].value,false,accent,160);
    text(12,137,view.rows[2].label,false,muted,296);
  } else for(uint8_t i=0;i<view.rowCount && i<3;i++) {
    const auto &row=view.rows[i];
    const int top=40+i*(partial?27:33);
    if(row.selected) fillRoundRect(8,top-2,304,partial?25:31,4,selection);
    text(14,top+18,row.label,false,row.readOnly?muted:foreground,row.value[0]?169:284);
    text(186,top+18,row.value,false,row.selected?accent:muted,113);
    if(row.dirty) fillCircle(304,top+13,2,warning);
  }
  if((view.mode==Settings::Mode::confirm || view.mode==Settings::Mode::secret ||
      view.mode==Settings::Mode::replace)) {
    char first[48],second[48];warningLines(*this,view.hint,first,second);
    text(12,126,first,false,warning,296);text(12,150,second,false,warning,296);
    return;
  }
  if(partial) {
    char first[48],second[48];warningLines(*this,view.hint,first,second);
    drawFastHLine(12,119,296,divider);
    text(12,139,first,false,warning,296);text(12,161,second,false,warning,296);
    return;
  }
  drawFastHLine(12,142,296,divider);
  const char *footer=editing?(view.hint[0]?view.hint:"Press: actions   Back: cancel"):
                     view.notice==Settings::Notice::none?view.hint:notice(view.notice);
  text(12,161,footer,false,view.notice==Settings::Notice::none?muted:warning,296);
}
} // namespace LocalUi
#endif
