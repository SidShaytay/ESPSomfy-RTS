#pragma once
#if defined(ESPSOMFY_LOCAL_UI) || defined(LOCAL_UI_HOST_PREVIEW)
#include <Adafruit_GFX.h>
#include <stdio.h>
#include "LocalUi.h"
#include "StatusDisplay.h"
#include "LocalKeyboard.h"
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
  void paintRecovery(BootRecoveryNotice);
  // Shared text-entry layout. The caller supplies a masked preview for secrets.
  void keyboard(const KeyboardView &view,const char *preview,uint8_t length,uint8_t limit) {
    constexpr uint16_t fg=0xef7d, muted=0x8c71, accent=0x6e9b, selected=0x19e7;
    text(12,51,preview,false,fg,246);
    char progress[12];snprintf(progress,sizeof(progress),"%u/%u",length,limit);
    text(263,51,progress,false,muted,48);
    if(view.bank==3) {
      const unsigned offset=(view.selected/27)*27;
      for(unsigned i=0;i<27 && offset+i<255;i++) {
        const int x=12+(i%9)*33,y=61+(i/9)*23;
        const bool active=view.selected==offset+i;
        if(active)fillRoundRect(x-2,y,30,22,3,selected);
        char label[4];snprintf(label,sizeof(label),"%02X",offset+i+1);
        text(x+2,y+17,label,false,active?accent:fg,30);
      }
      text(12,155,"Byte: press adds; Back returns",false,muted,296);return;
    }
    const auto letters=RotaryKeyboard::characterCount(view);
    for(uint8_t i=0;i<letters;i++) {
      const int x=12+(i%9)*33,y=61+(i/9)*23;
      if(view.selected==i)fillRoundRect(x-2,y,30,22,3,selected);
      char label[2]={RotaryKeyboard::character(view,i),0};
      text(x+7,y+17,label,false,view.selected==i?accent:fg,24);
    }
    if(view.bank==2) {
      const int x=12+(letters%9)*33,y=61+(letters/9)*23;
      if(view.selected==letters)fillRoundRect(x-2,y,30,22,3,selected);
      text(x+2,y+17,"0x",false,view.selected==letters?accent:muted,28);
    }
    const int cell=50;
    for(uint8_t i=0;i<6;i++) {
      const int x=10+i*cell;const bool active=view.selected==RotaryKeyboard::actionOffset(view)+i;
      if(active)fillRoundRect(x,136,cell-1,27,3,selected);
      const char *label=RotaryKeyboard::actionLabel(i);
      text(0,0,"",false,fg); // Select the body font before measuring.
      int16_t left,top;uint16_t width,height;getTextBounds(label,0,0,&left,&top,&width,&height);
      text(x+(cell-1-int(width))/2-left,155,label,false,active||(i==0&&view.shift)?accent:muted,cell-1);
    }
  }


private:
  void text(int x, int baseline, const char *, bool title = false, uint16_t color = 0xef7d,
            int maxWidth = 296);
  void shutter(int x, int y, int open, uint16_t color);
};
} // namespace LocalUi
#endif
