#include "ModelLoadTrust.h"
#include "CounterEdit.h"
#pragma once
#define SOMFY_H
#include "ShadeSettings.h"
#include "ShadeRequests.h"
#include "ShadeBehaviour.h"
#include <string.h>
// Receipt transport is deliberately unavailable in this adapter-only model.
// Actual remote/group receipt semantics are compiled by test-owned-repeat.sh.
struct SomfyRemote : RemoteSettings {
 radio_proto proto=radio_proto::RTS;somfy_frame_t lastFrame;
 virtual uint32_t getRemoteAddress() const=0;
 somfy_commands transformCommand(somfy_commands cmd){return flipCommands?flipCommand(cmd):cmd;}
 bool isLastCommand(somfy_commands){return false;}
 send_result_t repeatFrameReceipt(uint8_t){return {};}
 send_result_t sendCommandReceipt(somfy_commands,uint8_t,uint8_t){return {};}
};
struct SomfyShade : ShadeSettings,SomfyRemote {
  uint8_t id=255,roomId=0,gpioUp=0,gpioDown=0,gpioMy=0,gpioFlags=0;
  int8_t sortOrder=0;
  char name[21]="Device";
  bool paired=true,idle=true,light=false;
  uint32_t address=100,gpioRelease=0;
  uint16_t lastRollingCode=1;
  uint64_t configurationRevision=1,observationRevision=1;
  struct Remote { uint16_t lastRollingCode=0;uint32_t address=0; uint32_t getRemoteAddress() const{return address;} } linkedRemotes[7];
  struct State { float currentPos=35,currentTiltPos=20,target=50,tiltTarget=30,myPos=-1,myTiltPos=-1; int8_t direction=0,tiltDirection=0; } value;
  uint8_t getShadeId() const{return id;} uint32_t getRemoteAddress() const{return address;}
  const State &state() const{return value;} bool isIdle() const{return idle;} bool idleForAdministration() const{return idle && !gpioRelease;}
  uint8_t getFlags()const{return statusFlags;}uint8_t statusFlags=0;
  uint64_t favouriteObservationRevision() const{return observationRevision;}
  bool isInGroup();
  bool isToggle(){return isToggleShade(shadeType);}
  somfy_commands transformCommand(somfy_commands cmd){return flipCommands?flipCommand(cmd):cmd;}
  int8_t gpioConflict=0;
  int8_t validateGPIOs(radio_proto,uint8_t,uint8_t,uint8_t){return gpioConflict;}
  bool gpioCommandAllowed(somfy_commands);
  bool canGpioCommand(somfy_commands);bool canGpioSeek(bool=false);bool canGpioTilt(somfy_commands);
  bool moveToTarget(float){idle=false;return true;} bool moveToTiltTarget(float){idle=false;return true;}
  bool sendCommand(somfy_commands,uint8_t,uint8_t){++lastRollingCode;return true;}
  bool sendTiltCommand(somfy_commands){return true;} bool isLastCommand(somfy_commands){return false;}
  bool repeatFrame(uint8_t){return true;} bool sendSensorCommand(int8_t,int8_t,uint8_t){return true;}
  // Fake motor boundary only; real programming semantics have native coverage.
  bool programMyPositionHere(bool clear){
    if(!idle)return false;
    value.myPos=clear||tiltType==tilt_types::tiltonly?-1:value.currentPos;
    value.myTiltPos=clear||tiltType==tilt_types::none?-1:value.currentTiltPos;return true;
  }
  bool setMyPosition(int8_t pos,int8_t tilt){value.myPos=pos;value.myTiltPos=tilt;return true;}
  void setPosition(float pos,float tilt){if(pos>=0)value.currentPos=pos;if(tilt>=0)value.currentTiltPos=tilt;}
  void emitState(){} bool save(){return true;} void configChanged(){++configurationRevision;}
  uint16_t setRollingCode(uint16_t code){auto old=lastRollingCode;lastRollingCode=code;return old;}
};
struct SomfyGroup : SomfyRemote {
  uint8_t id=255,roomId=0; int8_t sortOrder=0,direction=0;
  char name[21]="Group"; uint8_t linkedShades[32]={};
  uint32_t address=500;uint16_t lastRollingCode=0;
  uint64_t configurationRevision=1;
  uint8_t statusFlags=0;uint8_t getFlags()const{return statusFlags;}
  uint8_t getGroupId() const{return id;} uint32_t getRemoteAddress() const{return address;}
  bool sendCommand(somfy_commands,uint8_t,uint8_t){return true;}
  bool isLastCommand(somfy_commands){return false;} bool repeatFrame(uint8_t){return true;}
  bool sendSensorCommand(int8_t,int8_t,uint8_t){return true;} void emitState(){}
};
struct SomfyRoom {uint8_t roomId=0; int8_t sortOrder=0; char name[21]="Room";};
struct SomfyShadeController {
  ModelLoadTrust modelLoad{ModelLoadStatus::loaded};
  uint64_t configurationRevision=1,restoreRevision=1;
  SomfyShade shades[32]; SomfyGroup groups[16]; SomfyRoom rooms[16];uint32_t repeaters[7]{};
  struct Radio {
    uint64_t configurationRevision=1;
    struct Config {uint8_t type=56;} config;
    bool ready=true,suspended=false,room=true,quiescent=true,installPaused=false;
    uint32_t cancelledTicket=0;bool cancelTx(uint32_t ticket){cancelledTicket=ticket;return true;}
    bool restartQuiescent()const{return quiescent;}
    CounterQuarantine quarantine=CounterQuarantine::none;
    CounterQuarantine counterQuarantine()const{return quarantine;}
    uint32_t blockedIdentity=0;bool counterBlocksIdentity(uint32_t address)const{return address && address==blockedIdentity;}
    bool available() const{return ready;} bool txSuspended() const{return suspended;} bool txHasRoom() const{return room;}
  } transceiver;
  SomfyShade *getShadeById(uint8_t id){for(auto &s:shades)if(s.id==id)return &s;return nullptr;}
  SomfyGroup *getGroupById(uint8_t id){for(auto &g:groups)if(g.id==id)return &g;return nullptr;}
  SomfyRoom *getRoomById(uint8_t id){for(auto &r:rooms)if(r.roomId==id)return &r;return nullptr;}
  uint8_t getNextShadeId(){for(uint8_t id=1;id<=32;id++)if(!getShadeById(id))return id;return 255;}
  uint8_t getNextGroupId(){for(uint8_t id=1;id<=16;id++)if(!getGroupById(id))return id;return 255;}
  int8_t getMaxShadeOrder(){int8_t value=-1;for(const auto &s:shades)if(s.id!=255&&s.sortOrder>value)value=s.sortOrder;return value;}
  int8_t getMaxGroupOrder(){int8_t value=-1;for(const auto &g:groups)if(g.id!=255&&g.sortOrder>value)value=g.sortOrder;return value;}
  uint32_t getNextRemoteAddress(uint8_t id){
    uint32_t value=1000+id;bool used;
    do {used=false;for(const auto &s:shades)if(s.id!=255&&s.address==value)used=true;
      for(const auto &g:groups)if(g.id!=255&&g.address==value)used=true;
      if(used)++value;
    } while(used);
    return value;
  }
  uint8_t getNextRoomId(){for(uint8_t id=1;id<=16;id++)if(!getRoomById(id))return id;return 0;}
  int8_t getMaxRoomOrder(){int8_t value=-1;for(const auto &r:rooms)if(r.roomId&&r.sortOrder>value)value=r.sortOrder;return value;}
};

extern SomfyShadeController somfy;
inline bool radioInstallPaused(){return somfy.transceiver.installPaused;}
inline bool SomfyShade::isInGroup(){
  for(const auto &group:somfy.groups)if(group.id!=255)
    for(const auto member:group.linkedShades)if(member==id)return true;
  return false;
}
// Exercise the production GPIO query against this bounded fake model.
#include "../../src/ShadeGpioCommands.cpp"

#ifndef LOCAL_QUICK_EXTERNAL_IO
#include "LocalQuickAccessNvs.h"
namespace LocalUi {
inline QuickAccessLoad readQuickAccessPreferences(QuickAccess &out){out={};return QuickAccessLoad::missing;}
inline QuickPreferenceSave writeQuickAccessPreferences(const QuickAccess &,QuickPreferenceWitness &){return QuickPreferenceSave::failed;}
}
#endif
