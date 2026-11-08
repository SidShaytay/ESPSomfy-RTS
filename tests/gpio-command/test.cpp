#include "Model.h"
#include "ShadeGpioCommands.h"
#include <assert.h>
#include <initializer_list>
unsigned storageReads=0,storageWrites=0,behaviourCalls=0;
uint8_t radioBitLength=56;
SomfyShadeController somfy;
static unsigned gpioWrites=0;
static uint8_t levels[256];
void pinMode(uint8_t,uint8_t){}
void digitalWrite(uint8_t pin,uint8_t level){assert(pin!=255);++gpioWrites;levels[pin]=level;}
void encodeRepeatJob(somfy_tx_job_t &,somfy_frame_t &,uint8_t){}
void noteRepeatQueued(somfy_frame_t &,const somfy_tx_job_t &){}
int main(){
 SomfyShade s;s.proto=radio_proto::GP_Remote;s.bitLength=56;s.lastFrame.cmd=somfy_commands::Up;
 assert(!s.sendCommandAfter(somfy_commands::Up,1,0,0).sent);
 assert(!storageReads&&!storageWrites&&!gpioWrites&&!somfy.processed&&s.lastRollingCode==0);
 assert(!s.repeatFrame(1));assert(!s.sendSensorCommand(1,1,1));assert(!somfy.frames);
 s.gpioUp=17;
 assert(s.sendCommandAfter(somfy_commands::Up,1,0,0).sent);assert(storageReads==1&&storageWrites==1&&gpioWrites==1);
 const auto writes=storageWrites; const auto frame=s.lastFrame;const auto code=s.lastRollingCode;
 s.conflict=1;assert(!s.sendCommandAfter(somfy_commands::Up,1,0,0).sent);
 assert(storageWrites==writes&&s.lastFrame.cmd==frame.cmd&&s.lastRollingCode==code);
 s.conflict=0;assert(!s.sendCommandAfter(somfy_commands::StepUp,1,0,0).sent);
 s.proto=radio_proto::RTS;s.conflict=1;assert(s.sendCommandAfter(somfy_commands::Up,1,0,0).sent);assert(somfy.frames==1);
 SomfyRemote base;base.proto=radio_proto::GP_Relay;assert(!base.sendCommand(somfy_commands::Up));
 // Wrapper admission must happen before any Behaviour call or default bit-length mutation.
 s.proto=radio_proto::GP_Remote;s.bitLength=0;s.conflict=1;
 assert(!s.sendCommand(somfy_commands::Up));assert(!s.sendTiltCommand(somfy_commands::Up));
 assert(!s.moveToTarget(20));assert(!s.moveToTiltTarget(20));assert(!s.setMyPosition(20));assert(!s.moveToMyPosition());
 assert(!behaviourCalls&&s.bitLength==0);
 // A rejected tilt-only seek/favorite must not normalize the live lift to100.
 s.tiltType=tilt_types::tiltonly;s.behaviour.restore(25,30,45,60);
 const auto original=s.state();
 assert(!s.moveToTarget(100,80));assert(!s.moveToMyPosition());assert(!s.setMyPosition(25,60));
 assert(s.state().currentPos==original.currentPos&&s.state().target==original.target&&s.state().myPos==original.myPos);
 assert(!behaviourCalls);
 s.conflict=0;s.tiltType=tilt_types::none;s.gpioUp=17;s.gpioDown=40;s.gpioMy=255;
 const auto directionBefore=s.state().direction;
 assert(!s.moveToTarget(60));assert(s.state().direction==directionBefore&&s.state().target==original.target);
 // Supported direct commands still travel through real Behaviour and driver.
 assert(s.sendCommand(somfy_commands::Down));assert(s.state().target==100);


 // Asserted-pad table agrees with the actual driver for every category/opcode.
 // All three outputs are present here; sparse assignment is checked separately.
 for(unsigned type=0;type<=16;++type) for(unsigned cmd=0;cmd<=255;++cmd) {
   SomfyShade driver;driver.proto=radio_proto::GP_Remote;driver.shadeType=shade_types(type);
   driver.gpioUp=17;driver.gpioDown=40;driver.gpioMy=42;
   levels[17]=levels[40]=levels[42]=0;
   somfy_frame_t pulse{};pulse.cmd=somfy_commands(cmd);driver.triggerGPIOs(pulse);
   const unsigned actual=(levels[17]?1:0)|(levels[40]?2:0)|(levels[42]?4:0);
   assert(actual==ShadeGpioPolicy::commandMask(driver.proto,driver.shadeType,pulse.cmd));
 }
 // Every partial assignment and polarity keeps available direct commands. A
 // composite seek requires the full direction/stop path before any state edit.
 for(unsigned mask=0;mask<8;++mask) for(unsigned flip=0;flip<2;++flip) {
   SomfyShade partial;partial.proto=radio_proto::GP_Remote;partial.flipCommands=flip;
   partial.gpioUp=mask&1?17:255;partial.gpioDown=mask&2?40:255;partial.gpioMy=mask&4?42:255;
   assert(partial.canGpioCommand(somfy_commands::Up)==bool(mask&(flip?2:1)));
   assert(partial.canGpioCommand(somfy_commands::Down)==bool(mask&(flip?1:2)));
   assert(partial.canGpioCommand(somfy_commands::My)==bool(mask&4));
   assert(partial.canGpioSeek()==(mask==7));
   partial.proto=radio_proto::GP_Relay;
   assert(partial.canGpioCommand(somfy_commands::Up)==bool(mask&1));
   assert(partial.canGpioCommand(somfy_commands::Down)==bool(mask&2));
   assert(partial.canGpioCommand(somfy_commands::My)==bool(mask&3));
   assert(partial.canGpioSeek()==((mask&3)==3));
   for(unsigned tilt=0;tilt<=4;++tilt) {
     partial.tiltType=tilt_types(tilt);
     assert(partial.canGpioTilt(somfy_commands::Up)==(bool(mask&1)&&(tilt==2||tilt==3)));
   }
 }
 SomfyShade single;single.proto=radio_proto::GP_Remote;single.shadeType=shade_types::garage1;single.gpioDown=40;
 for(auto bits:{56,80})for(auto low:{0,1}) {
   single.bitLength=bits;single.gpioFlags=low;const auto before=gpioWrites;
   assert(single.send(somfy_commands::My,2,0,0,false).sent);
   assert(gpioWrites==before+1&&levels[40]==(low?LOW:HIGH));
 }
 // Unconfigured/unsafe/suppressed commands never touch the production driver.
 single.gpioDown=255;const auto before=gpioWrites;const auto reads=storageReads;const auto stored=storageWrites;
 assert(!single.send(somfy_commands::My,2,0,0,false).sent);
 assert(gpioWrites==before&&storageReads==reads&&storageWrites==stored);
 puts("GPIO command boundaries and all category/opcode mappings passed");
}
