#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "SomfyFrame.h"
#define TRANSCEIVER_H
// Only radio hardware is replaced. Model classes, allocation, free-ID/order
// helpers and reset/behaviour methods are production code.
struct Transceiver { struct {uint8_t type=56;radio_proto proto=radio_proto::RTS;} config; };
#include "Somfy.h"
static unsigned ioCalls=0;
SomfyShadeController::SomfyShadeController() : startingAddress(100) {modelLoad.bootResult(ModelLoadStatus::loaded);}
SomfyShadeController somfy;
bool SomfyRemote::isLastCommand(somfy_commands){return false;}
uint16_t SomfyRemote::setRollingCode(uint16_t){++ioCalls;return 0;}
bool SomfyRemote::sendCommand(somfy_commands){++ioCalls;return false;}
bool SomfyRemote::sendCommand(somfy_commands,uint8_t,uint8_t){++ioCalls;return false;}
void SomfyRemote::triggerGPIOs(somfy_frame_t &){++ioCalls;}
bool SomfyShade::sendCommand(somfy_commands){++ioCalls;return false;}
bool SomfyShade::sendCommand(somfy_commands,uint8_t,uint8_t){++ioCalls;return false;}
void SomfyShade::triggerGPIOs(somfy_frame_t &){++ioCalls;}
bool SomfyShade::gpioCommandAllowed(somfy_commands){++ioCalls;return false;}
send_result_t SomfyShade::send(somfy_commands,uint8_t,uint8_t,uint16_t,bool){++ioCalls;return {};}
void SomfyShade::onSaveWanted(){++ioCalls;}
void SomfyShade::onSensorFlags(){++ioCalls;}
void SomfyShade::emitCommand(somfy_commands,const char*,uint32_t){++ioCalls;}
bool SomfyGroup::sendCommand(somfy_commands){++ioCalls;return false;}
bool SomfyGroup::sendCommand(somfy_commands,uint8_t,uint8_t){++ioCalls;return false;}
#include "production-reset.inc"
#include "../../src/ModelAllocation.cpp"
static void seed(SomfyShadeController &model,unsigned slot,unsigned id) {
  model.rooms[slot].roomId=id;model.rooms[slot].sortOrder=7;strcpy(model.rooms[slot].name,"Preserved room");
  auto &shade=model.shades[slot];shade.setShadeId(id);shade.setRemoteAddress(1000+id);
  strcpy(shade.name,"Preserved shade");shade.sortOrder=8;shade.gpioUp=0;shade.gpioDown=4;shade.gpioMy=16;shade.gpioFlags=1;
  shade.lastRollingCode=65500;shade.linkedRemotes[0].setRemoteAddress(2000+id);shade.roomId=id;
  shade.restore(43,72,33,62);shade.paired=true;
  auto &group=model.groups[slot];group.setGroupId(id);group.setRemoteAddress(3000+id);
  strcpy(group.name,"Preserved group");group.sortOrder=9;group.linkedShades[0]=id;group.roomId=id;group.flags=67;
}
static void preserved(SomfyShadeController &model,unsigned slot,unsigned id) {
  const auto &room=model.rooms[slot];assert(room.roomId==id && room.sortOrder==7 && !strcmp(room.name,"Preserved room"));
  auto &shade=model.shades[slot];assert(shade.getShadeId()==id && shade.getRemoteAddress()==1000+id);
  assert(!strcmp(shade.name,"Preserved shade") && shade.sortOrder==8 && shade.lastRollingCode==65500 && shade.roomId==id && shade.paired);
  assert(shade.gpioUp==0 && shade.gpioDown==4 && shade.gpioMy==16 && shade.gpioFlags==1);
  assert(shade.state().currentPos==43 && shade.state().currentTiltPos==72 && shade.state().myPos==33 && shade.state().myTiltPos==62);
  assert(shade.linkedRemotes[0].getRemoteAddress()==2000+id);
  auto &group=model.groups[slot];assert(group.getGroupId()==id && group.getRemoteAddress()==3000+id);
  assert(!strcmp(group.name,"Preserved group") && group.sortOrder==9 && group.linkedShades[0]==id && group.roomId==id && group.flags==67);
}
int main() {
  { SomfyShadeController model;auto &shade=model.shades[0];
    assert(shade.idleForAdministration());
    shade.gpioRelease=1;assert(!shade.idleForAdministration());
    shade.gpioRelease=0;assert(shade.idleForAdministration());
    shade.lastFrame.await=1;assert(!shade.idleForAdministration());
    shade.lastFrame.processed=true;assert(shade.idleForAdministration());
    assert(!ioCalls);
  }

  // Exact packed-after-reboot regression: free ID1 is unrelated to occupied slot0.
  { SomfyShadeController model;seed(model,0,2);
    assert(model.addRoom()==&model.rooms[1]);assert(model.rooms[1].roomId==1);preserved(model,0,2);
    assert(model.addShade()==&model.shades[1]);assert(model.shades[1].getShadeId()==1);
    assert(model.shades[1].gpioUp==255 && model.shades[1].gpioDown==255 && model.shades[1].gpioMy==255);
    assert(model.shades[1].sortOrder==9);preserved(model,0,2);
    assert(model.addGroup()==&model.groups[1]);assert(model.groups[1].getGroupId()==1);preserved(model,0,2);
  }
  // Sparse storage and sparse IDs: take the FIRST free slot, independently of ID.
  { SomfyShadeController model;seed(model,0,4);seed(model,2,2);
    assert(model.addRoom()==&model.rooms[1]);assert(model.rooms[1].roomId==1);
    assert(model.addShade()==&model.shades[1]);assert(model.shades[1].getShadeId()==1);
    assert(model.addGroup()==&model.groups[1]);assert(model.groups[1].getGroupId()==1);
    preserved(model,0,4);preserved(model,2,2);
  }
  // Reused deleted shade slots get fresh disconnected pads; other legacy pads stay.
  { SomfyShadeController model;seed(model,0,2);seed(model,1,3);model.shades[1].clear();
    model.shades[1].gpioUp=17;model.shades[1].gpioDown=18;model.shades[1].gpioMy=19;model.shades[1].gpioFlags=1;
    auto *shade=model.addShade();assert(shade==&model.shades[1]);
    assert(shade->gpioUp==255 && shade->gpioDown==255 && shade->gpioMy==255 && shade->gpioFlags==0);
    assert(shade->roomId==0 && !shade->sunSensor && !shade->simMy && !shade->light);
    preserved(model,0,2);
  }
  // Reused unused groups discard deleted state without changing NVS counters.
  { SomfyShadeController model;seed(model,0,2);seed(model,1,3);model.groups[1].clear();
    auto &old=model.groups[1];old.flags=67;old.flipCommands=true;old.lastFrame.remoteAddress=999;
    old.lastFrame.cmd=somfy_commands::Down;old.lastRollingCode=65500;old.direction=1;
    auto *group=model.addGroup();assert(group==&model.groups[1] && group->getGroupId()==1);
    assert(group->flags==0 && !group->flipCommands && group->lastFrame.remoteAddress==0);
    assert(group->lastRollingCode==0 && group->direction==0);
    for(auto member:group->linkedShades) assert(member==0);
    assert(group->roomId==0 && !group->name[0]);preserved(model,0,2);
  }
  // Empty/new objects use the same defaults offered to a fresh Add draft.
  { SomfyShadeController model;
    assert(model.addRoom()==&model.rooms[0] && model.rooms[0].roomId==1 && model.rooms[0].sortOrder==0);
    assert(model.addShade()==&model.shades[0] && model.shades[0].getShadeId()==1 && model.shades[0].sortOrder==0);
    assert(model.addGroup()==&model.groups[0] && model.groups[0].getGroupId()==1 && model.groups[0].sortOrder==0);
  }
  // Full occupied arrays with unique IDs exhaust the stable ID range.
  { SomfyShadeController model;
    for(unsigned i=0;i<SOMFY_MAX_SHADES;++i) model.shades[i].setShadeId(i+1);
    for(unsigned i=0;i<SOMFY_MAX_GROUPS;++i) model.groups[i].setGroupId(i+1);
    for(unsigned i=0;i<SOMFY_MAX_ROOMS;++i) model.rooms[i].roomId=i+1;
    assert(!model.addRoom() && !model.addShade() && !model.addGroup() && !model.isDirty);
  }
  // Full occupied arrays with duplicate IDs still refuse even though an ID is free.
  { SomfyShadeController model;
    for(unsigned i=0;i<SOMFY_MAX_SHADES;++i) {model.shades[i].setShadeId(2);model.shades[i].lastRollingCode=100+i;}
    for(unsigned i=0;i<SOMFY_MAX_GROUPS;++i) {model.groups[i].setGroupId(2);model.groups[i].flags=100+i;}
    for(unsigned i=0;i<SOMFY_MAX_ROOMS;++i) {model.rooms[i].roomId=2;model.rooms[i].sortOrder=i;}
    assert(!model.addRoom() && !model.addShade() && !model.addGroup());assert(!model.isDirty);
    for(unsigned i=0;i<SOMFY_MAX_SHADES;++i) assert(model.shades[i].getShadeId()==2 && model.shades[i].lastRollingCode==100+i);
    for(unsigned i=0;i<SOMFY_MAX_GROUPS;++i) assert(model.groups[i].getGroupId()==2 && model.groups[i].flags==100+i);
    for(unsigned i=0;i<SOMFY_MAX_ROOMS;++i) assert(model.rooms[i].roomId==2 && model.rooms[i].sortOrder==i);
  }
  assert(!ioCalls);puts("Production allocation: packed/sparse IDs, free slots, capacity and preserved state passed.");
}
