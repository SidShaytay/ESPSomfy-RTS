#pragma once
#include <assert.h>
#include <stdint.h>
#include <stddef.h>
struct TwoWire {
  explicit TwoWire(unsigned) {}
  void setTimeOut(unsigned) { assert(false); }
  bool begin(unsigned,unsigned,unsigned) { assert(false);return false; }
  void beginTransmission(unsigned) { assert(false); }
  size_t write(uint8_t) { assert(false);return 0; }
  unsigned endTransmission(bool) { assert(false);return 1; }
  unsigned requestFrom(uint8_t,uint8_t,bool) { assert(false);return 0; }
  unsigned available() { assert(false);return 0; }
  int read() { assert(false);return -1; }
};
