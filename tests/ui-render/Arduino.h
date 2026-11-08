#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <math.h>
#define PROGMEM
using byte=uint8_t;
class __FlashStringHelper;
class String {
  std::string s;

public:
  String(const char *v = "") : s(v) {}
  String(uint32_t v) : s(std::to_string(v)) {}
  size_t length() const { return s.size(); }
  const char *c_str() const { return s.c_str(); }
};
#ifdef UI_RUNTIME_HOST_TEST
#define INPUT_PULLUP 2
extern int testPins[64];
extern uint32_t testNow;
inline int digitalRead(int p) {
  return testPins[p];
}
inline void pinMode(int, int) {}
inline uint32_t millis() {
  return testNow;
}
inline uint32_t micros() {
  return testNow * 1000;
}
#endif
