#include <assert.h>
#include <stdio.h>
#include <initializer_list>
#include "BoardProfile.h"
#include <driver/gpio.h>
#include "SomfyFrame.h"
#include "ShadeSettings.h"
#include "ShadeBehaviour.h"

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
  radio_proto proto = radio_proto::GP_Relay;
  uint8_t gpioFlags = 0;
  int8_t gpioDir = 0;
  uint8_t gpioUp = 17, gpioDown = 40, gpioMy = 42;
  uint32_t gpioRelease = 0;
  struct State { int8_t direction = -1, tiltDirection = 0; float currentPos = 0; } position;
  const State &state() const { return position; }
  bool isToggle() { return isToggleShade(shadeType); }
  send_result_t send(somfy_commands cmd, uint8_t repeat, uint8_t stepSize, uint16_t delayMs, bool priority);
  void setGPIOs();
  void triggerGPIOs(somfy_frame_t &frame);
  bool usesPin(uint8_t pin);
};
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
static int levels[49] = {};
int gpio_set_level(gpio_num_t pin, uint32_t value) { levels[pin] = value; return 0; }
void pinMode(uint8_t, uint8_t) {}
void digitalWrite(uint8_t pin, uint8_t value) { writes++; levels[pin] = value; }
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
  puts("Reserved shade GPIO checks passed");
}
