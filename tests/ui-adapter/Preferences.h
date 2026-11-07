#pragma once
#include <stdint.h>
#include <stddef.h>
class Preferences {
public:
  bool begin(const char *, bool = false) { return true; }
  void end() {}
  uint8_t getUChar(const char *, uint8_t v) { return v; }
  uint16_t getUShort(const char *, uint16_t v) { return v; }
  size_t putUChar(const char *, uint8_t) { return 1; }
  size_t putUShort(const char *, uint16_t) { return 2; }
};
