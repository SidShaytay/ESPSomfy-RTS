#include <assert.h>
#include <stdio.h>
#include "BoardProfile.h"
#include "StatusBuild.h"
#include "SPI.h"
#include <driver/gpio.h>
#include <initializer_list>

TestSerial Serial;
TestESP ESP;
TestSPI SPI;
void TestSPI::begin(int8_t sck, int8_t miso, int8_t mosi, int8_t cs) {
  assert(sck == 11 && miso == 10 && mosi == 9 && cs == 12);
  assert(!active);
  active = true;
  starts++;
}
void TestSPI::end() { active = false; stops++; }
static int levels[49] = {};
static int modes[49] = {};
static unsigned long clockMs = 0;
static bool stuckMiso = false;
static unsigned int powerTransitions = 0;

void pinMode(uint8_t pin, uint8_t mode) {
  // Arduino 3 must release SPI ownership before preflight changes its pins.
  if(pin == 9 || pin == 10 || pin == 11) assert(!SPI.active);
  if(mode == OUTPUT) {
    // Initial output enable must already have the safe level latched.
    const bool high = pin == 12 || pin == 13 || pin == 15 || pin == 41 ||
      pin == 44 || pin == 47 || pin == 48;
    assert(levels[pin] == (high ? HIGH : LOW));
  }
  modes[pin] = mode;
}
int gpio_set_level(gpio_num_t pin, uint32_t value) {
  if(pin == 15 && value == HIGH) {
    // No other SPI device may drive MISO when the rail comes up.
    assert(levels[12] == HIGH && levels[13] == HIGH);
    assert(levels[41] == HIGH && levels[44] == HIGH);
    assert(levels[43] == LOW && levels[21] == LOW);
    assert(levels[47] == HIGH && levels[48] == HIGH);
    const uint8_t controls[] = {12, 13, 21, 41, 43, 44, 47, 48};
    for(auto control : controls) assert(modes[control] == OUTPUT);
    powerTransitions++;
  }
  levels[pin] = value;
  return 0;
}
void digitalWrite(uint8_t pin, uint8_t value) {
  // Arduino 3 only accepts writes after pinMode establishes GPIO ownership.
  if(modes[pin] == OUTPUT) gpio_set_level(pin, value);
}
int digitalRead(uint8_t pin) {
  if(pin == 10) return stuckMiso ? HIGH : LOW;
  return levels[pin];
}
unsigned long millis() { return clockMs; }
void delay(unsigned long duration) { clockMs += duration; }

int main() {
#if defined(ESPSOMFY_LILYGO_T_EMBED_CC1101)
  assert(BoardProfile::integratedRadio && strcmp(BoardProfile::name, "CC1101 / Plus") == 0);
#else
  assert(!BoardProfile::integratedRadio && strcmp(BoardProfile::name, "Generic") == 0);
#endif
  assert(!BoardProfile::radioFrequencyAllowed(NAN));assert(!BoardProfile::radioFrequencyAllowed(INFINITY));
  if(BoardProfile::integratedRadio) {
    assert(BoardProfile::radioFrequencyAllowed(387));assert(BoardProfile::radioFrequencyAllowed(464));
    assert(!BoardProfile::radioFrequencyAllowed(386.99f));assert(!BoardProfile::radioFrequencyAllowed(464.01f));
  } else {
    for(float f:{300.f,348.f,378.f,464.f,779.f,928.f})assert(BoardProfile::radioFrequencyAllowed(f));
    for(float f:{299.99f,348.01f,377.99f,464.01f,778.99f,928.01f})assert(!BoardProfile::radioFrequencyAllowed(f));
  }
  char build[48];
  StatusPanel::buildIdentity(build, sizeof(build), "test");
#if defined(ESPSOMFY_LILYGO_T_EMBED_CC1101)
#ifdef ESPSOMFY_LOCAL_UI
  assert(strcmp(build, "CC1101 / Plus UI test") == 0);
#else
  assert(strcmp(build, "CC1101 / Plus LCD test") == 0);
#endif
#else
  assert(strcmp(build, "Generic LCD test") == 0);
#endif
  BoardProfile::begin();
  if(BoardProfile::integratedRadio) {
    assert(powerTransitions == 1);
    assert(modes[15] == OUTPUT);
    assert(BoardProfile::prepareRadio(433.42f));
    assert(SPI.active && SPI.starts == 1);
    assert(modes[10] == INPUT && modes[9] == OUTPUT && modes[11] == OUTPUT);
    assert(levels[12] == HIGH); // Release CS after readiness probe.
    assert(BoardProfile::prepareRadio(433.42f)); // Reconfigure a running bus safely.
    assert(SPI.active && SPI.starts == 2 && SPI.stops == 2);
    assert(!BoardProfile::prepareRadio(868.0f));
    assert(!BoardProfile::prepareRadio(0.0f / 0.0f));
    stuckMiso = true;
    auto started = millis();
    assert(!BoardProfile::prepareRadio(433.42f));
    assert(millis() - started <= 25); // Fail safely instead of hanging in SmartRC.
    assert(levels[12] == HIGH);
    assert(!SPI.active); // A failed preflight must not initialize SPI.
    stuckMiso = false;
    assert(BoardProfile::prepareRadio(433.42f));
    assert(powerTransitions == 1); // Retrying must not power-cycle shared devices.
    const uint8_t reserved[] = {3, 9, 10, 11, 12, 13, 15, 21, 38, 41, 43, 44, 47, 48};
    for(auto pin : reserved) assert(BoardProfile::usesPin(pin));
    assert(BoardProfile::usesPin(19) && BoardProfile::usesPin(20));
    for(uint8_t pin = 26; pin <= 37; pin++) assert(BoardProfile::usesPin(pin));
    assert(!BoardProfile::usesPin(255));
    assert(BoardProfile::usesPin(16));
    assert(BoardProfile::usesPin(0));
    assert(BoardProfile::usesPin(4) && BoardProfile::usesPin(5) && BoardProfile::usesPin(6));
    assert(BoardProfile::usesPin(8) && BoardProfile::usesPin(18));
  }
  else {
    assert(powerTransitions == 0);
    assert(!SPI.active && SPI.starts == 0 && SPI.stops == 0);
    assert(BoardProfile::prepareRadio(868.0f));
    for(uint8_t pin = 0; pin < 49; pin++) assert(!BoardProfile::usesPin(pin));
  }
  puts("Board profile checks passed");
}
