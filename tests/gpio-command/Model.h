#include "ModelLoadTrust.h"
#pragma once
#define SOMFY_H
#include "ShadeSettings.h"
#include "ShadeBehaviour.h"
#include "TxBuffer.h"
#include "RadioScheduler.h"
#include "Preferences.h"
#include <stdio.h>
#include <string.h>
extern unsigned behaviourCalls;
constexpr int HIGH=1,LOW=0,OUTPUT=3;
void pinMode(uint8_t,uint8_t);void digitalWrite(uint8_t,uint8_t);
enum class gpio_flags_t:byte{LowLevelTrigger=1};
extern uint8_t radioBitLength;
struct SomfyRemote:RemoteSettings {
 char m_remotePrefId[12]="";uint32_t m_remoteAddress=1;bool m_rollingCodeSynced=false;
 radio_proto proto=radio_proto::RTS;uint8_t gpioUp=255,gpioDown=255,gpioMy=255;uint16_t lastRollingCode=0;
 somfy_frame_t lastFrame{};
 virtual ~SomfyRemote()=default;
 virtual bool gpioCommandAllowed(somfy_commands){return false;}
 virtual void triggerGPIOs(somfy_frame_t &);
 virtual uint8_t getFlags(){return 0;}
 virtual uint16_t p_lastRollingCode(uint16_t);
 void setRemoteAddress(uint32_t);uint32_t getRemoteAddress();somfy_commands transformCommand(somfy_commands);
 bool sendSensorCommand(int8_t,int8_t,uint8_t);bool sendCommand(somfy_commands);bool sendCommand(somfy_commands,uint8_t,uint8_t=0);
 send_result_t sendCommandAfter(somfy_commands,uint8_t,uint8_t,uint16_t,bool=false,bool=false);
 bool isLastCommand(somfy_commands);bool repeatFrame(uint8_t);uint16_t peekNextRollingCode();void commitRollingCode(uint16_t);uint16_t setRollingCode(uint16_t);
};
struct SomfyLinkedRemote:SomfyRemote{SomfyLinkedRemote();};
struct SomfyShade:SomfyRemote,ShadeSettings,MotorLink,ShadeEvents {
 uint8_t id=1;uint8_t &shadeId=id;
 SomfyLinkedRemote linkedRemotes[7];
 ShadeBehaviour behaviour;
 SomfyShade():behaviour(*this,*this,lastFrame,*this,*this,id,"Fake GPIO"){}
 int8_t conflict=0;
 void onChanged(uint16_t)override{++behaviourCalls;}
 void onCommand(somfy_commands,const char *,uint32_t)override{}
 void onSaveWanted()override{++behaviourCalls;}
 void onSensorFlags()override{}
 uint8_t gpioFlags=0;int8_t gpioDir=0;uint32_t gpioRelease=0;
 const ShadeBehaviour::State &state(){return behaviour.state();}
 uint8_t getShadeId(){return 1;}
 SomfyRemote *findRemote(uint32_t);void processFrame(somfy_frame_t &,bool=false);
 void configureGPIOs();void setGPIOs();bool usesPin(uint8_t);
 send_result_t send(somfy_commands,uint8_t,uint8_t,uint16_t,bool);
 int8_t validateGPIOs(radio_proto,uint8_t,uint8_t,uint8_t){return conflict;}
 bool isIdle(){return behaviour.isIdle();}
 bool isToggle(){return isToggleShade(shadeType);}
 bool gpioCommandAllowed(somfy_commands)override;
 bool canGpioCommand(somfy_commands);bool canGpioSeek(bool=false);bool canGpioTilt(somfy_commands);
 void triggerGPIOs(somfy_frame_t &)override;
 bool sendCommand(somfy_commands);bool sendCommand(somfy_commands,uint8_t,uint8_t=0);
 bool sendTiltCommand(somfy_commands);bool moveToTarget(float,float=-1);bool moveToTiltTarget(float);bool setMyPosition(int8_t,int8_t=-1);bool moveToMyPosition();
};
struct SomfyShadeController {
  ModelLoadTrust modelLoad{ModelLoadStatus::loaded};
 SomfyShade shades[2];void configureGPIOs();
 unsigned frames=0,processed=0;
 struct Transceiver {struct Config{uint8_t type=56;}config;uint32_t blocked=0;bool counterBlocksIdentity(uint32_t address){return blocked&&address==blocked;} bool submitTx(somfy_tx_job_t &){return true;}}transceiver;
 uint32_t sendFrame(somfy_frame_t &,uint8_t,uint16_t=0,bool=false){++frames;return 1;}
 void processFrame(somfy_frame_t &,bool,const SomfyRemote * =nullptr){++processed;}
 void markTxPending(uint32_t,uint32_t,const SomfyRemote *){}
};
extern SomfyShadeController somfy;
