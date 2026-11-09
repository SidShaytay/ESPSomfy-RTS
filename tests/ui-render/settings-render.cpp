#include "LocalUiRenderer.h"
#include "LocalSettingsView.h"
#include <Fonts/FreeSans9pt7b.h>
#include "../../src/LocalSettingsRenderer.cpp"
#include <assert.h>
#include <fstream>
#include <string>
#include <vector>
static void save(const std::string &name,const std::vector<uint16_t> &pixels) {
  std::ofstream out(name,std::ios::binary);
  out<<"P6\n320 170\n255\n";
  for(auto p:pixels) {
    const char rgb[]={char(((p>>11)&31)*255/31),char(((p>>5)&63)*255/63),char((p&31)*255/31)};
    out.write(rgb,3);
  }
}
int main(int argc,char **argv) {
  assert(argc==2); using namespace LocalUi;
  std::vector<uint16_t> pixels(screenWidth*screenHeight);Canvas canvas;canvas.attach(pixels.data());
  canvas.setFont(&FreeSans9pt7b);
  char full[48],visible[48];memset(full,'W',44);strcpy(full+44,"END");
  tail(canvas,full,visible,296);
  assert(strlen(visible)>=3 && strcmp(visible+strlen(visible)-3,"END")==0);
  int16_t x,y;uint16_t w,h;canvas.getTextBounds(visible,0,0,&x,&y,&w,&h);assert(w<=296);
  char warningFirst[48],warningSecond[48];
  warningLines(canvas,"Blank retains saved secret; identity changes can clear it.",warningFirst,warningSecond);
  std::string joined=std::string(warningFirst)+" "+warningSecond;
  assert(joined=="Blank retains saved secret; identity changes can clear it.");
  Settings::FormView view;strcpy(view.title,"Network");strcpy(view.hint,"Press to edit   Back: cancel");
  view.total=12;view.selected=0;view.rowCount=3;view.mode=Settings::Mode::fields;
  strcpy(view.rows[0].label,"WiFi network");strcpy(view.rows[0].value,"Home network");view.rows[0].selected=true;
  strcpy(view.rows[1].label,"WiFi password");strcpy(view.rows[1].value,"Set (kept)");
  strcpy(view.rows[2].label,"Connection");strcpy(view.rows[2].value,"WiFi");view.rows[2].readOnly=true;
  canvas.paintSettings(view);save(std::string(argv[1])+"/settings-network.ppm",pixels);
  const auto first=pixels;
  view.rows[0].selected=false;view.rows[1].selected=true;view.selected=1;view.rows[0].dirty=true;
  canvas.paintSettings(view);assert(pixels!=first);save(std::string(argv[1])+"/settings-secret.ppm",pixels);
  view.notice=Settings::Notice::partial;strcpy(view.hint,"May be partly stored; running unchanged. Retry before restart.");canvas.paintSettings(view);save(std::string(argv[1])+"/settings-partial.ppm",pixels);
  view.notice=Settings::Notice::busy;canvas.paintSettings(view);save(std::string(argv[1])+"/settings-busy.ppm",pixels);
  Settings::FormView blocked;blocked.notice=Settings::Notice::savedRestartBlocked;
  const RestartReason causes[]={RestartReason::none,RestartReason::settingsUncertain,RestartReason::modelFailed,
    RestartReason::modelUncertain,RestartReason::queuedOperations,RestartReason::pending,RestartReason::stale,
    RestartReason::cancelled,RestartReason::exhausted,RestartReason::clock,RestartReason::invalidRequest,
    RestartReason::notReady,RestartReason::persistenceBusy,RestartReason::counterUncertain,RestartReason::activeMovement,RestartReason::unknownGroup,RestartReason::modelUnavailable,RestartReason::securityUnavailable,RestartReason(255)};
  std::vector<uint16_t> queued;
  for(const auto cause:causes) {
    blocked.restartReason=cause;
    const char *text=restartBlockText(cause);
    assert(strlen(text)<64);assert(strstr(text,"Save settings")==nullptr);
    warningLines(canvas,text,warningFirst,warningSecond);
    const auto complete=std::string(warningFirst)+(warningSecond[0]?" ":"")+warningSecond;
    assert(complete==text);
    canvas.getTextBounds(warningFirst,0,0,&x,&y,&w,&h);assert(w<=296);
    canvas.getTextBounds(warningSecond,0,0,&x,&y,&w,&h);assert(w<=296);
    canvas.paintSettings(blocked);
    if(cause==RestartReason::queuedOperations){queued=pixels;save(std::string(argv[1])+"/settings-restart-queued.ppm",pixels);assert(strstr(text,"uncertain")==nullptr);}
    if(cause==RestartReason::pending){save(std::string(argv[1])+"/settings-restart-pending.ppm",pixels);assert(strstr(text,"pending")!=nullptr);}
    if(cause==RestartReason::counterUncertain){assert(pixels!=queued);save(std::string(argv[1])+"/settings-restart-counter.ppm",pixels);}
    if(cause==RestartReason::modelUnavailable){assert(strstr(text,"verified full model restore")!=nullptr);save(std::string(argv[1])+"/settings-restart-model-unavailable.ppm",pixels);}
    if(cause==RestartReason::securityUnavailable){assert(strstr(text,"Authentication is unavailable")!=nullptr);save(std::string(argv[1])+"/settings-restart-security-unavailable.ppm",pixels);}
    if(cause==RestartReason::unknownGroup){assert(strstr(text,"Group")!=nullptr);save(std::string(argv[1])+"/settings-restart-group.ppm",pixels);}
    if(cause==RestartReason::modelFailed)save(std::string(argv[1])+"/settings-restart-model.ppm",pixels);
    if(cause==RestartReason(255)){assert(strstr(text,"uncertain")==nullptr);save(std::string(argv[1])+"/settings-restart-unknown.ppm",pixels);}
    assert(blocked.protectedFrame);
  }
  assert(view.protectedFrame);
  view.notice=Settings::Notice::none;view.mode=Settings::Mode::confirm;view.rowCount=2;
  strcpy(view.hint,"Changes authentication; may close sessions.");
  for(const char *hint:{"Changes authentication; may close sessions.","Changes radio operation; may restart.","Changes MQTT connection/discovery.","Network can disconnect; may restart.","Blank retains saved secret; identity changes can clear it.","May be partly stored; running unchanged. Retry before restart."}) {
    warningLines(canvas,hint,warningFirst,warningSecond);
    canvas.getTextBounds(warningFirst,0,0,&x,&y,&w,&h);assert(w<=296);
    canvas.getTextBounds(warningSecond,0,0,&x,&y,&w,&h);assert(w<=296);
  }
  strcpy(view.rows[0].label,"Cancel");view.rows[0].value[0]=0;view.rows[0].selected=true;
  strcpy(view.rows[1].label,"Save settings");view.rows[1].value[0]=0;view.rows[1].selected=false;
  canvas.paintSettings(view);save(std::string(argv[1])+"/settings-confirm.ppm",pixels);
  const auto confirmation=pixels;
  view.notice=Settings::Notice::kept;canvas.paintSettings(view);assert(pixels==confirmation);
  view.notice=Settings::Notice::partial;canvas.paintSettings(view);assert(pixels==confirmation);
  view.notice=Settings::Notice::none;
  view.mode=Settings::Mode::secret;strcpy(view.hint,"Blank retains saved secret; identity changes can clear it.");
  canvas.paintSettings(view);const auto secret=pixels;
  view.notice=Settings::Notice::invalid;canvas.paintSettings(view);assert(pixels==secret);
  view.mode=Settings::Mode::replace;view.notice=Settings::Notice::invalid;
  view.total=2;view.selected=0;view.rows[0].dirty=false;
  strcpy(view.title,"Frequency (MHz)");
  strcpy(view.rows[1].label,"Replace invalid value");
  strcpy(view.hint,"Original kept until Apply; Save persists replacement.");
  warningLines(canvas,view.hint,warningFirst,warningSecond);
  assert(std::string(warningFirst)+" "+warningSecond==view.hint);
  canvas.getTextBounds(warningFirst,0,0,&x,&y,&w,&h);assert(w<=296);
  canvas.getTextBounds(warningSecond,0,0,&x,&y,&w,&h);assert(w<=296);
  canvas.paintSettings(view);const auto replacement=pixels;
  save(std::string(argv[1])+"/settings-replace.ppm",pixels);
  for(auto stale:{Settings::Notice::none,Settings::Notice::partial,Settings::Notice::failed,Settings::Notice::kept}) {
    view.notice=stale;canvas.paintSettings(view);assert(pixels==replacement);
  }
  view.notice=Settings::Notice::none;
  view.mode=Settings::Mode::text;view.rowCount=3;
  strcpy(view.rows[0].label,"WiFi network");strcpy(view.rows[0].value,full);
  strcpy(view.rows[1].label,"Character");strcpy(view.rows[1].value,"'D' (68)");
  strcpy(view.rows[2].label,"Back cancels edit");strcpy(view.hint,"47/64 bytes; press adds, Back cancels");
  canvas.paintSettings(view);save(std::string(argv[1])+"/settings-edit-tail.ppm",pixels);
}
