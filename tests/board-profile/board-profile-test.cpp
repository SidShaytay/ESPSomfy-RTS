#include <assert.h>
#include <stdio.h>
#include "BoardProfile.h"

TestSerial Serial;
TestESP ESP;
static int levels[49] = {};
static int modes[49] = {};
static unsigned long clockMs = 0;
static bool stuckMiso = false;
static unsigned int powerTransitions = 0;

void pinMode(uint8_t pin, uint8_t mode) { modes[pin] = mode; }
void digitalWrite(uint8_t pin, uint8_t value) {
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
}
int digitalRead(uint8_t pin) {
  if(pin == 10) return stuckMiso ? HIGH : LOW;
  return levels[pin];
}
unsigned long millis() { return clockMs; }
void delay(unsigned long duration) { clockMs += duration; }

int main() {
  BoardProfile::begin();
  if(BoardProfile::integratedRadio) {
    assert(powerTransitions == 1);
    assert(modes[15] == OUTPUT);
    assert(BoardProfile::prepareRadio(433.42f));
    assert(modes[10] == INPUT && modes[9] == OUTPUT && modes[11] == OUTPUT);
    assert(levels[12] == HIGH); // Release CS after readiness probe.
    assert(!BoardProfile::prepareRadio(868.0f));
    assert(!BoardProfile::prepareRadio(0.0f / 0.0f));
    stuckMiso = true;
    auto started = millis();
    assert(!BoardProfile::prepareRadio(433.42f));
    assert(millis() - started <= 25); // Fail safely instead of hanging in SmartRC.
    assert(levels[12] == HIGH);
    stuckMiso = false;
    assert(BoardProfile::prepareRadio(433.42f));
    assert(powerTransitions == 1); // Retrying must not power-cycle shared devices.
    const uint8_t reserved[] = {3, 9, 10, 11, 12, 13, 15, 21, 38, 41, 43, 44, 47, 48};
    for(auto pin : reserved) assert(BoardProfile::usesPin(pin));
    assert(BoardProfile::usesPin(19) && BoardProfile::usesPin(20));
    for(uint8_t pin = 26; pin <= 37; pin++) assert(BoardProfile::usesPin(pin));
    assert(!BoardProfile::usesPin(255));
    assert(!BoardProfile::usesPin(16));
  }
  else {
    assert(powerTransitions == 0);
    assert(BoardProfile::prepareRadio(868.0f));
    for(uint8_t pin = 0; pin < 49; pin++) assert(!BoardProfile::usesPin(pin));
  }
  puts("Board profile checks passed");
}
