#include "Model.h"
#include "ShadeGpioCommands.h"
#include <assert.h>
#include <nvs.h>
#include <initializer_list>
unsigned storageReads=0,storageWrites=0,behaviourCalls=0;
uint8_t radioBitLength=56;
SomfyShadeController somfy;
static unsigned gpioWrites=0;
static bool counterAllowed=true;
bool counterCommandAllowed(uint32_t,somfy_commands){return counterAllowed;}
static uint8_t levels[256];
void pinMode(uint8_t,uint8_t){}
void digitalWrite(uint8_t pin,uint8_t level){assert(pin!=255);++gpioWrites;levels[pin]=level;}
void encodeRepeatJob(somfy_tx_job_t &,somfy_frame_t &,uint8_t){}
void noteRepeatQueued(somfy_frame_t &,const somfy_tx_job_t &){}
#include "production-rx.inc"
static void unreadable_counter_refuses_rf_and_gpio_before_any_effect() {
 for(auto protocol:{radio_proto::RTS,radio_proto::GP_Remote,radio_proto::GP_Relay}) for(int fault=0;fault<3;++fault) {
  FakeNvs::reset();
  SomfyShade remote;remote.setRemoteAddress(123);remote.proto=protocol;remote.bitLength=56;
  remote.gpioUp=17;remote.gpioDown=40;remote.gpioMy=42;remote.lastRollingCode=77;
  remote.lastFrame.remoteAddress=321;remote.lastFrame.cmd=somfy_commands::My;
  if(fault==0)FakeNvs::failOpen=1;
  if(fault==1)FakeNvs::failRead=1;
  if(fault==2) {
   nvs_handle_t h;assert(nvs_open("ShadeCodes",NVS_READWRITE,&h)==ESP_OK);
   assert(nvs_set_i8(h,"_123",1)==ESP_OK);nvs_close(h);
   FakeNvs::sets=0;FakeNvs::writeOpens=0;
  }
  const auto gpio=gpioWrites,frames=somfy.frames,processed=somfy.processed,behaviour=behaviourCalls;
  const auto writes=storageWrites;
  assert(!remote.sendCommandAfter(somfy_commands::Up,1,0,0).sent);
  assert(gpioWrites==gpio && somfy.frames==frames && somfy.processed==processed && behaviourCalls==behaviour);
  assert(storageWrites==writes && !FakeNvs::sets && !FakeNvs::writeOpens && !FakeNvs::commits && FakeNvs::handles.empty());
  assert(remote.lastRollingCode==77 && remote.lastFrame.remoteAddress==321 && remote.lastFrame.cmd==somfy_commands::My);
 }
 FakeNvs::reset();storageReads=storageWrites=0;
}
static void checked_counter_reads_preserve_absence_rollover_and_sensor_semantics() {
 for(bool absentNamespace:{false,true}) {
  FakeNvs::reset();FakeNvs::readOnlyNamespaceMissing=absentNamespace;
  SomfyRemote remote;remote.setRemoteAddress(123);uint16_t next=91;
  assert(remote.peekNextRollingCode(next) && next==1);
  assert(FakeNvs::readOnlyOpens==1 && !FakeNvs::writeOpens && !FakeNvs::sets && !FakeNvs::commits && FakeNvs::handles.empty());
 }
 FakeNvs::reset();nvs_handle_t h;assert(nvs_open("ShadeCodes",NVS_READWRITE,&h)==ESP_OK);
 assert(nvs_set_u16(h,"_123",65535)==ESP_OK);nvs_close(h);
 FakeNvs::opens=FakeNvs::sets=FakeNvs::writeOpens=0;
 SomfyRemote remote;remote.setRemoteAddress(123);uint16_t next=91;
 assert(remote.peekNextRollingCode(next) && next==0 && remote.lastRollingCode==0);
 assert(!FakeNvs::writeOpens && !FakeNvs::sets && !FakeNvs::commits && FakeNvs::handles.empty());
 // A failed read is distinct from both exact absence and valid zero rollover.
 FakeNvs::failRead=FakeNvs::reads+1;next=91;
 assert(!remote.peekNextRollingCode(next) && next==91);
 FakeNvs::failRead=0;assert(remote.peekNextRollingCode(next) && next==0);
 assert(remote.sendCommandAfter(somfy_commands::Up,1,0,0).sent && remote.lastRollingCode==0 && remote.lastFrame.rollingCode==0);
 // The existing synchronized fast path performs no further NVS read.
 const auto reads=FakeNvs::reads,opens=FakeNvs::opens;FakeNvs::failOpen=opens+1;
 assert(remote.peekNextRollingCode(next) && next==1 && FakeNvs::reads==reads && FakeNvs::opens==opens);
 // Sensor rollingCode carries flags, so it is not a counter-storage read.
 FakeNvs::reset();FakeNvs::failOpen=1;
 SomfyRemote sensor;sensor.setRemoteAddress(456);sensor.bitLength=80;sensor.lastRollingCode=77;
 const auto stored=storageWrites;
 assert(sensor.sendSensorCommand(0,1,1) && sensor.lastFrame.rollingCode==2 && sensor.lastRollingCode==77);
 assert(!FakeNvs::opens && !FakeNvs::reads && !FakeNvs::sets && storageWrites==stored);
 FakeNvs::reset();storageReads=storageWrites=0;somfy.frames=somfy.processed=0;
}
int main(){
 unreadable_counter_refuses_rf_and_gpio_before_any_effect();
 checked_counter_reads_preserve_absence_rollover_and_sensor_semantics();
 { SomfyShade shade;shade.proto=radio_proto::GP_Remote;shade.gpioUp=17;shade.gpioDown=40;shade.gpioMy=42;
   somfy.modelLoad.fail();const auto before=gpioWrites;
   assert(!shade.sendCommand(somfy_commands::Up) && !shade.moveToTarget(100) && !shade.programMyPositionHere(false));
   shade.configureGPIOs();somfy.configureGPIOs();assert(gpioWrites==before);
   somfy.modelLoad.bootResult(ModelLoadStatus::loaded);
 }
 counterAllowed=false;
 SomfyShade blocked;blocked.proto=radio_proto::RTS;blocked.bitLength=0;
 assert(!blocked.sendCommand(somfy_commands::Up));
 assert(!blocked.sendCommandAfter(somfy_commands::Up,1,0,0).sent);
 assert(!blocked.sendSensorCommand(1,1,1));
 assert(!blocked.moveToTarget(80) && !blocked.moveToTiltTarget(80) && !blocked.sendTiltCommand(somfy_commands::Up));
 assert(!blocked.setMyPosition(80) && !blocked.moveToMyPosition() && !blocked.programMyPositionHere(false));
 assert(!FakeNvs::reads && !storageWrites && !gpioWrites && !behaviourCalls && !somfy.frames && !blocked.bitLength);
 counterAllowed=true;
 SomfyShade observed;observed.linkedRemotes[0].setRemoteAddress(12345);
 observed.linkedRemotes[0].lastRollingCode=100;somfy.transceiver.blocked=12345;
 somfy_frame_t incoming;incoming.valid=true;incoming.cmd=somfy_commands::Down;incoming.remoteAddress=12345;incoming.rollingCode=500;
 observed.processFrame(incoming,false);observed.behaviour.tick(1);
 assert(observed.state().direction==1 && observed.state().target==100 && observed.linkedRemotes[0].lastRollingCode==100);
 assert(!FakeNvs::reads && !storageWrites && !gpioWrites && !somfy.frames);
 somfy.transceiver.blocked=0;incoming.cmd=somfy_commands::My;incoming.rollingCode=501;incoming.processed=false;
 observed.processFrame(incoming,false);observed.behaviour.tick(2);
 assert(observed.state().direction==0 && observed.linkedRemotes[0].lastRollingCode==501 && storageWrites==1);
 incoming.cmd=somfy_commands::Down;incoming.rollingCode=400;incoming.processed=false;
 observed.processFrame(incoming,false);
 assert(observed.linkedRemotes[0].lastRollingCode==400 && storageWrites==2); // Existing unblocked RX is not global anti-replay.
 FakeNvs::reads=storageWrites=behaviourCalls=0;

 SomfyShade s;s.proto=radio_proto::GP_Remote;s.bitLength=56;s.lastFrame.cmd=somfy_commands::Up;
 assert(!s.sendCommandAfter(somfy_commands::Up,1,0,0).sent);
 assert(!FakeNvs::reads&&!storageWrites&&!gpioWrites&&!somfy.processed&&s.lastRollingCode==0);
 assert(!s.repeatFrame(1));assert(!s.sendSensorCommand(1,1,1));assert(!somfy.frames);
 s.gpioUp=17;
 assert(s.sendCommandAfter(somfy_commands::Up,1,0,0).sent);assert(FakeNvs::reads==1&&storageWrites==1&&gpioWrites==1);
 const auto writes=storageWrites; const auto frame=s.lastFrame;const auto code=s.lastRollingCode;
 s.conflict=-15;assert(!s.sendCommandAfter(somfy_commands::Up,1,0,0).sent);
 assert(storageWrites==writes&&s.lastFrame.cmd==frame.cmd&&s.lastRollingCode==code);
 s.conflict=0;assert(!s.sendCommandAfter(somfy_commands::StepUp,1,0,0).sent);
 s.proto=radio_proto::RTS;s.conflict=1;assert(s.sendCommandAfter(somfy_commands::Up,1,0,0).sent);assert(somfy.frames==1);
 SomfyRemote base;base.proto=radio_proto::GP_Relay;assert(!base.sendCommand(somfy_commands::Up));
 // Wrapper admission must happen before any Behaviour call or default bit-length mutation.
 s.proto=radio_proto::GP_Remote;s.bitLength=0;s.conflict=-15;
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
 single.gpioDown=255;const auto before=gpioWrites;const auto reads=FakeNvs::reads;const auto stored=storageWrites;
 assert(!single.send(somfy_commands::My,2,0,0,false).sent);
 assert(gpioWrites==before&&FakeNvs::reads==reads&&storageWrites==stored);
 SomfyShade binary;binary.proto=radio_proto::GP_Relay;binary.shadeType=shade_types::drycontact;binary.gpioDown=40;
 binary.behaviour.restore(100,0,-1,-1);
 assert(binary.sendCommand(somfy_commands::Up));binary.behaviour.tick(2);binary.setGPIOs();
 assert(binary.state().currentPos==0&&levels[40]==LOW);
 assert(binary.sendCommand(somfy_commands::Down));binary.behaviour.tick(4);binary.setGPIOs();
 assert(binary.state().currentPos==100&&levels[40]==HIGH);
 // Ordinary MY is an existing one-button toggle, not a favorite seek.
 for(auto type:{shade_types::drycontact,shade_types::garage1,shade_types::lgate1,shade_types::cgate1,shade_types::rgate1}) {
   SomfyShade toggle;toggle.proto=radio_proto::GP_Relay;toggle.shadeType=type;
   toggle.gpioUp=17;toggle.gpioDown=40;
   assert(toggle.sendCommand(somfy_commands::My));
 }
 puts("GPIO command boundaries and all category/opcode mappings passed");
}
