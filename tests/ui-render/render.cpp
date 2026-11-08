#include "LocalUiRenderer.h"
#include <vector>
#include <assert.h>
#include <stdio.h>
#include <string.h>
using namespace LocalUi;
static void save(const char *directory,const char *name,const std::vector<uint16_t> &buffer) {
  char path[512];snprintf(path,sizeof(path),"%s/%s.ppm",directory,name);FILE *file=fopen(path,"wb");assert(file);
  fprintf(file,"P6\n320 170\n255\n");
  for(size_t i=1;i<buffer.size()-1;i++) {
    const auto c=buffer[i];const uint8_t rgb[]={uint8_t((c>>11)*255/31),uint8_t(((c>>5)&63)*255/63),uint8_t((c&31)*255/31)};
    fwrite(rgb,1,3,file);
  }
  fclose(file);
}
int main(int argc,char **argv) {
  assert(argc==2);std::vector<uint16_t> buffer(screenWidth*screenHeight+2,0xdead);
  Canvas canvas;canvas.attach(buffer.data()+1);
  Model m;m.ready=m.controlsEnabled=m.authorized=m.pageValid=true;m.authMode=AuthMode::none;
  m.page.total=32;m.page.count=3;
  m.page.rows[0]={7,0,0,"Bedroom curtain",700,1,35,true,true};
  m.page.rows[1]={22,0,1,"Duplicate long name",2200,1,90,false,true};
  m.page.rows[2]={31,0,2,"Duplicate long name",3100,1,-1,false,false};
  auto &target=m.target;target.ref={EntityKind::device,7};target.exists=target.valid=target.moving=true;
  target.token=99;target.identity=700;target.revision=1;target.capabilities=7;target.position=35;
  strcpy(target.name,"Bedroom curtain");target.descriptor.shadeType=shade_types::cdrapery;target.descriptor.bitLength=56;
  StatusDisplay::Snapshot s;s.running=true;s.battery={BatteryGauge::State::ready,74,3920,-100};
  s.link=StatusDisplay::Link::connected;s.web=StatusDisplay::Web::listening;s.radio=StatusDisplay::Radio::ready;
  s.uptimeSeconds=4140;strcpy(s.address,"192.0.2.12");strcpy(s.build,"LILYGO shared");
  strcpy(s.firmwareVersion,"v4.0.1");strcpy(s.applicationVersion,"4.0.2");
  View v;v.entity=target.ref;v.openedToken=99;strcpy(v.roomName,"Bedroom");
  const Screen screens[]={Screen::status,Screen::home,Screen::catalogue,Screen::control,Screen::details,Screen::roomKinds,Screen::brightness,Screen::settings};
  const char *names[]={"status","home","devices","control","details","room-kinds","brightness","settings"};
  for(unsigned i=0;i<8;i++) {v.screen=screens[i];canvas.paint(m,v,s);save(argv[1],names[i],buffer);assert(buffer.front()==0xdead && buffer.back()==0xdead);}
  v.screen=Screen::details;canvas.paint(m,v,s);auto original=buffer;
  strcpy(s.applicationVersion,"4.0.3");canvas.paint(m,v,s);assert(buffer!=original);
  strcpy(s.applicationVersion,"4.0.2");v.detailsOffset=4;canvas.paint(m,v,s);assert(buffer!=original);save(argv[1],"details-scrolled",buffer);
  v.screen=Screen::home;v.selected=4;canvas.paint(m,v,s);save(argv[1],"home-settings",buffer);
  v.screen=Screen::catalogue;v.catalogue=CatalogueKind::devices;
  strcpy(m.page.rows[0].name,"\xc3\xa9\n");canvas.paint(m,v,s);const auto fallback=buffer;
  strcpy(m.page.rows[0].name,"??");canvas.paint(m,v,s);assert(buffer==fallback); // one glyph per codepoint; no newline layout effect
  strcpy(m.page.rows[0].name,"\xe5\xb8\x98\xc3\xa9 longue porte");canvas.paint(m,v,s);save(argv[1],"catalogue-fallback",buffer);
  m.page.count=m.page.total=0;canvas.paint(m,v,s);save(argv[1],"devices-empty",buffer);
  v.catalogue=CatalogueKind::rooms;m.page.total=18;m.page.count=3;m.page.offset=15;m.page.cursor=17;
  for(uint8_t i=0;i<3;i++)m.page.rows[i]={uint8_t(14+i),0,0,"Repeated room"};
  canvas.paint(m,v,s);save(argv[1],"rooms-last-page",buffer);
  v.screen=Screen::control;v.entity={EntityKind::group,11};target.ref=v.entity;target.descriptor.group=true;
  target.descriptor.memberCount=32;target.movingCount=17;strcpy(target.name,"Both rooms and more");
  canvas.paint(m,v,s);save(argv[1],"group",buffer);
  v.entity={EntityKind::device,7};target.ref=v.entity;target.descriptor.group=false;target.descriptor.memberCount=0;
  strcpy(target.name,"Bedroom curtain");target.capabilities=allControlActions&~controlActionBit(Action::toggle);target.tiltPosition=20;
  v.screen=Screen::control;v.action=Action::count;canvas.paint(m,v,s);save(argv[1],"control-more",buffer);
  v.screen=Screen::actions;v.selected=0;v.action=Action::position;canvas.paint(m,v,s);save(argv[1],"actions-position",buffer);
  v.selected=10;v.action=Action::sunOn;canvas.paint(m,v,s);save(argv[1],"actions-sun",buffer);
  v.screen=Screen::value;v.action=Action::position;v.draftValue=36;v.valueChoice=0;
  canvas.paint(m,v,s);save(argv[1],"position-draft",buffer);original=buffer;
  v.draftValue=37;canvas.paint(m,v,s);assert(buffer!=original);
  v.valueChoice=2;canvas.paint(m,v,s);save(argv[1],"position-cancel",buffer);original=buffer;
  v.valueChoice=1;canvas.paint(m,v,s);assert(buffer!=original);save(argv[1],"position-apply",buffer);
  v.action=Action::stepDown;v.draftValue=127;canvas.paint(m,v,s);save(argv[1],"step-apply",buffer);
  v.screen=Screen::control;v.action=Action::toggle;target.capabilities=controlActionBit(Action::toggle);
  canvas.paint(m,v,s);save(argv[1],"toggle",buffer);
  v.screen=Screen::control;v.action=Action::stop;target.capabilities=7;target.moving=false;m.controlPending=true;
  canvas.paint(m,v,s);save(argv[1],"pending-idle-stop",buffer);original=buffer;
  m.controlPending=false;canvas.paint(m,v,s);assert(buffer!=original);
  v.notice=Notice::stopNotQueued;canvas.paint(m,v,s);save(argv[1],"stop-not-queued",buffer);v.notice=Notice::none;
  v.screen=Screen::brightness;v.dimSeconds=v.offSeconds=0;v.setting=2;canvas.paint(m,v,s);save(argv[1],"no-timeouts",buffer);
  v.screen=Screen::unlock;v.credentialMaximum=4;v.credentialLength=4;v.credentialChoice=CredentialEditor::doneChoice;
  canvas.paint(m,v,s);save(argv[1],"unlock-pin",buffer);original=buffer;
  v.credentialMaximum=32;v.credentialLength=32;v.credentialUser=true;canvas.paint(m,v,s);save(argv[1],"unlock-username",buffer);assert(buffer!=original);
  v.credentialUser=false;v.credentialByte=true;v.credentialChoice=0xc3;canvas.paint(m,v,s);save(argv[1],"unlock-byte",buffer);
  v.retryAfterMs=30000;v.notice=Notice::unlockWait;canvas.paint(m,v,s);save(argv[1],"unlock-throttled",buffer);
  v.retryAfterMs=UINT32_MAX-998;canvas.paint(m,v,s);original=buffer;
  v.retryAfterMs=UINT32_MAX;canvas.paint(m,v,s);assert(buffer!=original);save(argv[1],"unlock-long-clock-wait",buffer);
  assert(buffer.front()==0xdead && buffer.back()==0xdead);
  puts("Production renderer: dynamic pages, rooms, duplicate/UTF-8 labels, controls and protected entry bounds passed.");
}
