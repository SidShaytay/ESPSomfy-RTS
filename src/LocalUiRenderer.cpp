#if defined(ESPSOMFY_LOCAL_UI) || defined(LOCAL_UI_HOST_PREVIEW)
#include "LocalUiRenderer.h"
#include "LocalText.h"
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
                         "Display settings save failed",
                         "Unlock required", "Invalid credentials", "Too many attempts - wait",
                         "Local unlock unavailable", "Unlocked - select action again",
                         "Command accepted", "Partially accepted - review",
                         "Busy - retry", "Pending action cancelled", "Estimate unavailable - no draft", "Queued action expired - retry", "Cancelled; STOP not queued"};
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
  lcdLabel(s,label,sizeof(label));
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
void Canvas::paintRecovery(BootRecoveryNotice notice) {
  if(notice==BootRecoveryNotice::none)return;
  fillRect(0,151,screenWidth,19,bg);
  text(12,166,bootRecoveryText(notice),false,amber,296);
}
void Canvas::paint(const Model &m, const View &v, const StatusDisplay::Snapshot &s) {
  fillScreen(bg);
  char label[48];
  if(v.screen == Screen::unlock && s.running) {
    text(12,28,"Local unlock",true,white);
    text(12,54,v.credentialUser ? "Username (masked)" :
         v.credentialMaximum==4 ? "PIN (masked)" : "Password (masked)",false,muted);
    char masked[33]={};
    for(uint8_t i=0;i<v.credentialLength && i<32;i++) masked[i]='*';
    text(12,78,masked,false,white,180);
    snprintf(label,sizeof(label),"%u/%u bytes",unsigned(v.credentialLength),unsigned(v.credentialMaximum));
    text(200,78,label,false,muted,108);
    if(v.credentialByte) snprintf(label,sizeof(label),"Add byte %02X (01-FF)",unsigned(v.credentialChoice));
    else if(v.credentialChoice<95) {
      if(v.credentialChoice==0) snprintf(label,sizeof(label),"Add space");
      else snprintf(label,sizeof(label),"Add character: %c",char(v.credentialChoice+32));
    } else snprintf(label,sizeof(label),"%s",v.credentialChoice==95?"Backspace":
                    v.credentialChoice==96?"Byte entry (UTF-8 bytes)":
                    v.credentialChoice==97?(v.credentialUser?"Next: password":"Unlock"):"Cancel");
    fillRoundRect(10,90,300,30,5,selected);
    text(20,112,label,false,teal);
    if(v.retryAfterMs) snprintf(label,sizeof(label),"Wait %lus (device-wide limit)",
      (unsigned long)(v.retryAfterMs/1000+(v.retryAfterMs%1000!=0)));
    else snprintf(label,sizeof(label),"%s",v.notice==Notice::unlockWrong?"Invalid credentials":
                  v.notice==Notice::unlockUnavailable?"Local unlock unavailable":"Turn selects; press adds/applies");
    text(12,142,label,false,amber);
    text(12,164,"Back: cancel   Idle clears entry",false,muted);
    return;
  }
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
      else if(index<10)
        snprintf(label, sizeof(label), "%s", page.lines[index - 2]);
      else
        StatusDisplay::formatHealthLine(s.health,index-10,label,sizeof(label));
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
    text(12, 29, "Controller", true, white);
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
    text(12, 164, "Turn: Home   Hold: details", false, muted);
  } else if(v.screen==Screen::home || v.screen==Screen::roomKinds || v.screen==Screen::catalogue || v.screen==Screen::settings) {
    const char *home[]={"Rooms","Devices","Groups","Settings","Display settings","Status","Administration","System"};
    static_assert(sizeof(home)/sizeof(*home)==homeRows,"Every Home entry needs a label");
    const bool list=v.screen==Screen::catalogue;
    const bool roomKinds=v.screen==Screen::roomKinds;
    const bool settings=v.screen==Screen::settings;
    const unsigned count=list?m.page.total:roomKinds?2:settings?1:homeRows;
    const unsigned cursor=list?m.page.cursor:v.selected<count?v.selected:0;
    const unsigned offset=list?m.page.offset:cursor/catalogueRows*catalogueRows;
    const char *title=v.screen==Screen::home?"Home":roomKinds?v.roomName:settings?"Settings":
      v.catalogue==CatalogueKind::rooms?"Rooms":v.catalogue==CatalogueKind::groups?"Groups":"Devices";
    text(12,28,title,true,white,230);
    snprintf(label,sizeof(label),"%u/%u",count?cursor+1:0,count);text(250,27,label,false,muted,58);
    if(list && (!m.pageValid || !count)) text(12,79,m.pageValid?"Nothing configured":"List changed - go Back",false,amber);
    for(unsigned row=0;row<catalogueRows && offset+row<count;row++) {
      const unsigned index=offset+row;
      if(list && row>=m.page.count) break;
      const int y=40+row*34;
      const bool active=index==cursor;
      fillRoundRect(10,y,300,30,5,active?selected:bg);
      drawRoundRect(10,y,300,30,5,active?teal:line);
      if(list) {
        const auto &entry=m.page.rows[row];
        if(v.catalogue==CatalogueKind::rooms && (entry.id==0 || entry.id==allRooms))
          snprintf(label,sizeof(label),"%s",entry.name);
        else snprintf(label,sizeof(label),"#%u %s",unsigned(entry.id),entry.name);
        text(18,y+21,label,false,entry.available || v.catalogue==CatalogueKind::rooms?white:muted,210);
        if(v.catalogue==CatalogueKind::devices) {
          if(!entry.available) snprintf(label,sizeof(label),"Setup");
          else if(entry.position<0) snprintf(label,sizeof(label),"--");
          else snprintf(label,sizeof(label),"%d%%",entry.position);
          text(245,y+21,label,false,entry.moving?amber:muted,60);
        } else if(v.catalogue==CatalogueKind::groups) text(247,y+21,"Group",false,muted,59);
      } else text(22,y+21,settings?"Settings unavailable":roomKinds?(index?"Groups":"Devices"):home[index],false,white,280);
    }
    const char *footer=v.notice!=Notice::none?notices[int(v.notice)]:list?"Press: open   Estimates only":"Turn: select   Press: open";
    text(12,164,footer,false,v.notice==Notice::none?muted:amber);
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
    text(12, 164, v.setting == 2 ? "Press: save   Back: cancel" : "Press: next   Back: cancel", false, muted);
  } else if(v.screen==Screen::actions || v.screen==Screen::repeatActions) {
    snprintf(label,sizeof(label),"#%s%u %s",v.entity.kind==EntityKind::group?"G":"",unsigned(v.entity.id),m.target.name);
    text(12,28,label,true,white);
    snprintf(label,sizeof(label),v.screen==Screen::repeatActions?"Repeat action %u/%u":"Actions %u/%u",unsigned(v.selected+1),unsigned((v.screen==Screen::repeatActions?repeatMenu(m.target.descriptor):controlMenu(m.target.capabilities,false,repeatableControls(m.target.descriptor)!=0)).count));
    text(12,52,label,false,muted);
    const auto menu=(v.screen==Screen::repeatActions?repeatMenu(m.target.descriptor):controlMenu(m.target.capabilities,false,repeatableControls(m.target.descriptor)!=0));
    const unsigned offset=v.selected<3?0:v.selected-2;
    for(unsigned row=0;row<3 && row+offset<menu.count;row++) {
      const unsigned index=offset+row;const bool active=index==v.selected;
      const auto action=menu.actions[index];const int y=62+row*29;
      fillRoundRect(10,y,300,26,4,active?selected:bg);
      text(18,y+20,controlLabel(action),false,active?teal:white,218);
      if(action==Action::position || action==Action::tiltPosition) {
        const int value=action==Action::position?m.target.position:m.target.tiltPosition;
        if(value>=0) {snprintf(label,sizeof(label),"%d%%",value);text(258,y+20,label,false,muted,48);}
      }
    }
    if(m.controlPending) text(12,164,"Queued action; Back cancels",false,amber);
    else if(v.notice!=Notice::none) text(12,164,notices[int(v.notice)],false,amber);
    else text(12,164,"Turn: choose   Back: STOP page",false,muted);
  } else if(v.screen==Screen::favouriteObserve || v.screen==Screen::favouriteConfirm) {
    snprintf(label,sizeof(label),"#%u %s",unsigned(v.entity.id),m.target.name);text(12,28,label,true,white);
    const bool review=v.screen==Screen::favouriteConfirm,clear=v.action==Action::clearFavouriteHere;
    text(12,52,review?(clear?"Clear favourite?":"Set favourite here?"):"First move, then observe stop",false,amber);
    snprintf(label,sizeof(label),"Estimate: %d%%   Tilt: %d%%",int(m.target.position),int(m.target.tiltPosition));text(12,75,label,false,muted);
    const char *choices[]={review?"Cancel":"Move with normal controls",review?(clear?"Send Clear favourite":"Send Set favourite"):"I observed it stopped"};
    for(unsigned i=0;i<2;++i) {
      const int y=89+i*30;const bool active=v.valueChoice==i;
      fillRoundRect(10,y,300,27,4,active?selected:bg);text(18,y+20,choices[i],false,active?teal:white,284);
    }
    text(12,164,v.notice!=Notice::none?notices[int(v.notice)]:review?"Programs motor; observe result.":clear&&!m.target.favouriteClearReady?"Clear: reach old favourite first.":"Back: controls   No command yet",false,amber);
  } else if(v.screen==Screen::value || v.screen==Screen::repeatValue) {
    snprintf(label,sizeof(label),"#%s%u %s",v.entity.kind==EntityKind::group?"G":"",unsigned(v.entity.id),m.target.name);
    text(12,28,label,true,white);
    if(v.screen==Screen::repeatValue && controlIsStep(v.action)) {
      snprintf(label,sizeof(label),"%s: %u steps",controlLabel(v.action),unsigned(v.repeatStep));
      text(12,52,label,false,muted);
    } else text(12,52,controlLabel(v.action),false,muted);
    fillRoundRect(10,64,300,42,5,v.valueChoice?bg:selected);
    snprintf(label,sizeof(label),v.screen==Screen::repeatValue?"Repeats: %d":controlIsStep(v.action)?"%d steps":"%d%%",int(v.draftValue));
    text(22,94,label,true,white);
    if(v.valueChoice) {
      const char *choices[]={v.repeating && v.screen==Screen::value?"Next":"Apply","Cancel"};
      for(unsigned i=0;i<2;i++) {
        const int x=12+i*152;const bool active=v.valueChoice==i+1;
        fillRoundRect(x,114,144,30,4,active?selected:bg);
        drawRoundRect(x,114,144,30,4,active?teal:line);
        text(x+35,136,choices[i],false,active?teal:white,108);
      }
    } else text(12,135,"Turn: adjust   Push: review",false,teal);
    text(12,164,m.controlPending?"Queued action; Back cancels both":v.notice!=Notice::none?notices[int(v.notice)]:v.repeating && v.screen==Screen::value?"Next: choose repeats; Back cancels":"Back: cancel   Apply sends",false,m.controlPending||v.notice!=Notice::none?amber:muted);
  } else {
    const auto &t=m.target;
    snprintf(label,sizeof(label),"#%s%u %s",v.entity.kind==EntityKind::group?"G":"",unsigned(v.entity.id),t.exists?t.name:"Not configured");
    text(12,28,label,true,white);
    static const char *types[]={"Roller","Blind","Left curtain","Awning","Shutter","One-button garage","Garage",
      "Right curtain","Centre curtain","Dry contact","Two-button contact","Left gate","Centre gate","Right gate",
      "One-button left gate","One-button centre gate","One-button right gate"};
    if(v.entity.kind==EntityKind::group) snprintf(label,sizeof(label),"Group: %u members, %u moving",unsigned(t.descriptor.memberCount),unsigned(t.movingCount));
    else snprintf(label,sizeof(label),"%s | %s %ubit",uint8_t(t.descriptor.shadeType)<17?types[uint8_t(t.descriptor.shadeType)]:"Unknown type",
      t.descriptor.proto==radio_proto::RTS?"RTS":t.descriptor.proto==radio_proto::RTW?"RTW":t.descriptor.proto==radio_proto::RTV?"RTV":
      t.descriptor.proto==radio_proto::GP_Relay?"GPIO relay":"GPIO remote",unsigned(t.descriptor.bitLength));
    text(12,52,label,false,muted);
    if(!t.exists || !v.openedToken || t.token!=v.openedToken) snprintf(label,sizeof(label),"Target changed - reopen");
    else if(!t.valid) snprintf(label,sizeof(label),"Unavailable / setup required");
    else if(v.entity.kind==EntityKind::group) snprintf(label,sizeof(label),"%s",t.moving?"Estimated motion":"Estimated idle");
    else if(t.position<0) snprintf(label,sizeof(label),"Position estimate unavailable");
    else snprintf(label,sizeof(label),"Estimated position %d%%",t.position);
    text(12,74,label,false,t.valid?muted:amber);
    const auto menu=controlMenu(t.capabilities,true,repeatableControls(t.descriptor)!=0);
    for(unsigned i=0;i<menu.count;i++) {
      const int width=(296-int(menu.count-1)*8)/menu.count,x=12+i*(width+8);
      const auto action=menu.actions[i];const bool active=action==v.action;
      const bool enabled=t.valid && (action==Action::count || action!=Action::stop || t.moving || m.controlPending);
      fillRoundRect(x,85,width,52,6,active?selected:bg);drawRoundRect(x,85,width,52,6,active?teal:line);
      const auto color=enabled?white:muted;
      if(action==Action::stop) drawRect(x+width/2-5,94,10,10,color);
      else if(action==Action::up || action==Action::down) {
        const int direction=action==Action::up?-1:1,mid=x+width/2;
        drawLine(mid-10,99-direction*4,mid,99+direction*4,color);drawLine(mid,99+direction*4,mid+10,99-direction*4,color);
      }
      text(x+8,127,controlLabel(action),false,color,width-12);
    }
    if(v.notice==Notice::cancelled || v.notice==Notice::stopNotQueued) text(12,163,notices[int(v.notice)],false,amber);
    else if(m.controlPending) text(12,163,v.notice==Notice::busy?"Busy; Back cancels queued action":"Queued - Back cancels",false,amber);
    else if(v.notice!=Notice::none) text(12,163,notices[int(v.notice)],false,amber);
    else if(!m.authorized) text(12,163,t.stopLease?"STOP only; unlock other actions":"Press: unlock   Back: return",false,amber);
    else text(12,163,t.lightUnavailable?(t.pendingRelayTilt?"Tilt unavailable; Light unavailable":"Light unavailable"):
      t.pendingRelayTilt?"Separate relay tilt unavailable":"Press: select   Back: return",false,t.pendingRelayTilt||t.lightUnavailable?amber:muted);
  }
}
} // namespace LocalUi
#endif
