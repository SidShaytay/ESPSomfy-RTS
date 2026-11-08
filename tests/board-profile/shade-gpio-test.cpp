#include <assert.h>
#include <stdio.h>
#include <initializer_list>
#include <set>
#include <vector>
#include "BoardProfile.h"
#include <driver/gpio.h>
#include "SomfyFrame.h"
#include "ShadeSettings.h"
#include "ShadeBehaviour.h"
#include "ModelEditor.h"

// Substitute model/storage and the base frame dispatcher. Production MotorLink
// and GPIO methods below observe the actual board policy; no hardware is used.
#define SOMFY_H
enum class gpio_flags_t : byte { LowLevelTrigger = 0x01 };
struct SomfyRemote {
  virtual ~SomfyRemote() {}
  virtual void triggerGPIOs(somfy_frame_t &frame);
  send_result_t sendCommandAfter(somfy_commands cmd, uint8_t repeat, uint8_t stepSize, uint16_t delayMs, bool priority, bool hearsItself);
};
struct SomfyShade : SomfyRemote, ShadeSettings {
  uint8_t id=255;
  uint8_t getShadeId(){return id;}
  int8_t validateGPIOs(radio_proto,uint8_t,uint8_t,uint8_t);
  radio_proto proto = radio_proto::GP_Relay;
  uint8_t gpioFlags = 0;
  int8_t gpioDir = 0;
  uint8_t gpioUp = 17, gpioDown = 40, gpioMy = 42;
  uint32_t gpioRelease = 0;
  struct State { int8_t direction = -1, tiltDirection = 0; float currentPos = 0; } position;
  const State &state() const { return position; }
  bool isToggle() { return isToggleShade(shadeType); }
  send_result_t send(somfy_commands cmd, uint8_t repeat, uint8_t stepSize, uint16_t delayMs, bool priority);
  void configureGPIOs();
  void setGPIOs();
  void triggerGPIOs(somfy_frame_t &frame);
  bool usesPin(uint8_t pin);
};
#define SHADE_GPIO_ADMISSION_HOST_TEST
#define HAS_ETHERNET 0
struct SomfyShadeController {
  SomfyShade shades[2];
  void configureGPIOs();
  struct Radio {bool usesPin(uint8_t pin){return BoardProfile::usesPin(pin);}} transceiver;
} somfy;
struct ConfigSettings {} settings;
const char *modelCommitFailureDescription(ModelCommitState){return "Not exercised";}
#include "ShadeGpioAdmission.cpp"
#include "ShadeGpio.cpp"
#include "ShadeTransport.cpp"

// Model/storage and radio scheduling are replaced; the production MotorLink
// entry and GPIO driver above are exercised together. Never sends RF.
static somfy_commands transported;
send_result_t SomfyRemote::sendCommandAfter(somfy_commands cmd, uint8_t repeat, uint8_t stepSize, uint16_t delayMs, bool priority, bool hearsItself) {
  assert(stepSize == 0 && delayMs == 0 && !priority && hearsItself);
  transported = cmd;
  somfy_frame_t frame;
  frame.cmd = cmd;
  frame.repeats = repeat;
  triggerGPIOs(frame);
  send_result_t result;
  result.sent = true;
  return result;
}

TestSerial Serial;
TestESP ESP;
static unsigned int writes = 0;
static std::set<uint8_t> touched;
static std::vector<uint8_t> configured;
static int levels[49] = {};
int gpio_set_level(gpio_num_t pin, uint32_t value) { levels[pin] = value; return 0; }
void pinMode(uint8_t pin, uint8_t mode) { assert(GPIO_IS_VALID_OUTPUT_GPIO(pin));assert(mode==OUTPUT);configured.push_back(pin);touched.insert(pin); }
void digitalWrite(uint8_t pin, uint8_t value) { assert(GPIO_IS_VALID_OUTPUT_GPIO(pin));writes++; touched.insert(pin);levels[pin] = value; }
int digitalRead(uint8_t pin) { return levels[pin]; }
static unsigned long clockNow = 100;
unsigned long millis() { return clockNow; }
void delay(unsigned long) {}
// BoardProfile preflight is linked but this test never starts the hardware.
#include "SPI.h"
TestSPI SPI;
void TestSPI::begin(int8_t, int8_t, int8_t, int8_t) {}
void TestSPI::end() {}

int main() {
  SomfyShade shade;
  shade.setGPIOs();
  assert(writes == 2 && levels[17] == HIGH && levels[40] == LOW);
  somfy_frame_t frame;
  frame.cmd = somfy_commands::Down;
  frame.repeats = 2;
  shade.proto = radio_proto::GP_Remote;
  shade.triggerGPIOs(frame);
  assert(writes == 5 && levels[40] == HIGH);
  // 56-bit Toggle reaches MotorLink as legacy MY. One-button GPIO devices
  // must still pulse their existing configured toggle output, gpioDown.
  const shade_types oneButton[] = {shade_types::garage1, shade_types::drycontact,
    shade_types::lgate1, shade_types::cgate1, shade_types::rgate1};
  for(auto type : oneButton) {
    shade.shadeType = type;
    for(uint8_t low = 0; low < 2; low++) {
      shade.gpioFlags = low;
      unsigned int beforePulse = writes;
      levels[17] = 7;
      levels[40] = 7;
      levels[42] = 7;
      assert(shade.send(somfy_commands::My, 3, 0, 0, false).sent);
      assert(transported == somfy_commands::Toggle);
      assert(writes == beforePulse + 1);
      assert(levels[40] == (low ? LOW : HIGH));
      assert(levels[17] == 7 && levels[42] == 7);
      assert(shade.gpioRelease == 700);
      beforePulse = writes;
      assert(shade.send(somfy_commands::Toggle, 2, 0, 0, false).sent);
      assert(writes == beforePulse + 1 && shade.gpioRelease == 500);
      beforePulse = writes;
      clockNow = 500;
      shade.setGPIOs();
      assert(writes == beforePulse); // pulse held through its existing deadline
      clockNow = 501;
      shade.setGPIOs();
      assert(writes == beforePulse + 3 && shade.gpioRelease == 0);
      assert(levels[17] == (low ? HIGH : LOW));
      assert(levels[40] == (low ? HIGH : LOW));
      assert(levels[42] == (low ? HIGH : LOW));
      clockNow = 100;
    }
  }
  // Ordinary MY remains the dedicated MY output. Unsupported opcodes do not
  // pulse a single-button device, and RF/relay commands never use this driver.
  shade.shadeType = shade_types::roller;
  shade.gpioFlags = 0;
  const unsigned int beforeMy = writes;
  shade.send(somfy_commands::My, 1, 0, 0, false);
  assert(transported == somfy_commands::My && writes == beforeMy + 3);
  assert(levels[17] == LOW && levels[40] == LOW && levels[42] == HIGH);
  shade.shadeType = shade_types::garage1;
  unsigned int unchanged = writes;
  shade.send(somfy_commands::Up, 1, 0, 0, false);
  shade.send(somfy_commands::StepUp, 1, 0, 0, false);
  assert(writes == unchanged);
  for(auto proto : {radio_proto::RTS, radio_proto::RTW, radio_proto::RTV, radio_proto::GP_Relay}) {
    shade.proto = proto;
    shade.send(somfy_commands::My, 1, 0, 0, false);
    assert(transported == somfy_commands::My && writes == unchanged);
  }
  shade.shadeType = shade_types::roller;
  const unsigned int before = writes;
  // These assignments can arrive from an old backup with the radio disabled.
  // Test every reserved pad in each output slot, including USB/flash pads.
  for(uint8_t pin = 0; pin < 49; pin++) {
    if(!BoardProfile::usesPin(pin)) continue;
    for(uint8_t slot = 0; slot < 3; slot++) {
      shade.gpioUp = slot == 0 ? pin : 17;
      shade.gpioDown = slot == 1 ? pin : 40;
      shade.gpioMy = slot == 2 ? pin : 42;
      shade.proto = radio_proto::GP_Remote;
      shade.triggerGPIOs(frame);
      shade.shadeType = shade_types::garage1;
      shade.send(somfy_commands::My, 1, 0, 0, false);
      shade.shadeType = shade_types::roller;
      shade.setGPIOs();
      if(slot != 2) {
        shade.proto = radio_proto::GP_Relay;
        shade.setGPIOs();
      }
      assert(writes == before);
    }
  }
  // Legacy duplicate or unset assignments must not reach an SDK pin call.
  shade.proto=radio_proto::GP_Relay;shade.gpioUp=17;shade.gpioDown=17;shade.gpioMy=42;
  unchanged=writes;shade.setGPIOs();assert(writes==unchanged);
  shade.proto=radio_proto::GP_Remote;shade.gpioUp=shade.gpioDown=shade.gpioMy=255;
  shade.gpioRelease=0;shade.setGPIOs();shade.triggerGPIOs(frame);assert(writes==unchanged);
  // Setup, release and all existing category/command paths share the same
  // conservative physical admission. No reserved/invalid/duplicate call leaks.
  const somfy_commands commands[]={somfy_commands::Up,somfy_commands::Down,somfy_commands::My,
    somfy_commands::Toggle,somfy_commands::MyUp,somfy_commands::MyDown,somfy_commands::MyUpDown};
  for(auto protocol:{radio_proto::GP_Remote,radio_proto::GP_Relay})for(unsigned category=0;category<=16;category++) {
    shade.proto=protocol;shade.shadeType=static_cast<shade_types>(category);
    shade.gpioUp=17;shade.gpioDown=40;shade.gpioMy=42;shade.gpioRelease=0;
    configured.clear();touched.clear();shade.configureGPIOs();
    assert(configured.size()==(protocol==radio_proto::GP_Remote?3u:2u));
    assert(configured[0]==17 && configured[1]==40);
    if(protocol==radio_proto::GP_Remote)assert(configured[2]==42);
    for(auto command:commands){frame.cmd=command;shade.triggerGPIOs(frame);}
    clockNow=10000;shade.setGPIOs();clockNow=100;
    assert(shade.usesPin(17) && shade.usesPin(40));
    assert(shade.usesPin(42)==(protocol==radio_proto::GP_Remote));
    assert(!shade.usesPin(255));
    assert(touched.size()==(protocol==radio_proto::GP_Remote?3u:2u));
    for(unsigned slot=0;slot<(protocol==radio_proto::GP_Remote?3u:2u);slot++) {
      for(uint8_t bad:{uint8_t(15),uint8_t(22),uint8_t(46),uint8_t(99),uint8_t(17)}) {
        shade.gpioUp=17;shade.gpioDown=40;shade.gpioMy=42;
        if(slot==0)shade.gpioUp=bad==17?40:bad;
        if(slot==1)shade.gpioDown=bad;
        if(slot==2)shade.gpioMy=bad;
        const auto beforeBad=writes;configured.clear();shade.configureGPIOs();
        shade.setGPIOs();shade.triggerGPIOs(frame);
        assert(writes==beforeBad && configured.empty());
        assert((slot==0?shade.gpioUp:slot==1?shade.gpioDown:shade.gpioMy)==(slot==0 && bad==17?40:bad));
      }
    }
    shade.gpioUp=shade.gpioDown=shade.gpioMy=255;
    const auto beforeNone=writes;configured.clear();shade.configureGPIOs();shade.setGPIOs();shade.triggerGPIOs(frame);
    assert(writes==beforeNone && configured.empty());
  }
  shade.proto=radio_proto::GP_Remote;shade.gpioUp=17;shade.gpioDown=40;shade.gpioMy=42;
  auto &other=somfy.shades[0];other.id=7;other.proto=radio_proto::GP_Remote;
  other.gpioUp=17;other.gpioDown=255;other.gpioMy=255;
  unchanged=writes;configured.clear();shade.configureGPIOs();shade.triggerGPIOs(frame);shade.setGPIOs();
  assert(writes==unchanged && configured.empty());
  other.id=255;shade.configureGPIOs();assert(configured.size()==3);
  shade.shadeType=shade_types::roller;shade.gpioUp=17;shade.gpioDown=shade.gpioMy=255;shade.gpioRelease=0;
  configured.clear();shade.configureGPIOs();assert(configured.size()==1 && configured[0]==17);
  frame.cmd=somfy_commands::Up;unchanged=writes;shade.triggerGPIOs(frame);
  assert(writes==unchanged+1 && levels[17]==HIGH);
  clockNow=10000;unchanged=writes;shade.setGPIOs();assert(writes==unchanged+1 && levels[17]==LOW);
  // Preserve existing lift/integrated/tilt-only relay direction and polarity.
  shade.proto=radio_proto::GP_Relay;shade.shadeType=shade_types::blind;
  shade.gpioUp=17;shade.gpioDown=40;shade.gpioMy=255;
  for(unsigned tilt=0;tilt<=4;tilt++)for(unsigned low=0;low<2;low++) {
    shade.tiltType=static_cast<tilt_types>(tilt);shade.gpioFlags=low;
    const int on=low?LOW:HIGH,off=low?HIGH:LOW;
    shade.position.direction=1;shade.position.tiltDirection=-1;shade.setGPIOs();
    assert(levels[17]==(tilt==3?on:off) && levels[40]==(tilt==3?off:on));
    shade.position.direction=0;shade.setGPIOs();
    assert(levels[17]==((tilt==2 || tilt==3)?on:off) && levels[40]==off);
  }
  // Model completion sees final ownership, not the previous per-record owner.
  auto &loaded=somfy.shades[0];loaded.id=7;loaded.proto=radio_proto::GP_Remote;
  loaded.gpioUp=17;loaded.gpioDown=40;loaded.gpioMy=42;
  auto &second=somfy.shades[1];second.id=8;second.proto=radio_proto::GP_Remote;
  second.gpioUp=17;second.gpioDown=second.gpioMy=255;
  configured.clear();somfy.configureGPIOs();assert(configured.empty());
  second.gpioUp=255;configured.clear();somfy.configureGPIOs();assert(configured.size()==3);
  assert(configured[0]==17 && configured[1]==40 && configured[2]==42);
  puts("Reserved shade GPIO checks passed");
}
