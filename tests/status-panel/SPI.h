#pragma once
#include <assert.h>
#include <stdint.h>
#include <vector>
#include "Arduino.h"
#define MSBFIRST 1
#define SPI_MODE0 0
extern bool owned, spiOK;
struct SPISettings { unsigned hz; SPISettings(unsigned h, int, int) : hz(h) {} };
struct SPIStub {
  bool active = false;
  unsigned bytes = 0, transactions = 0;
  std::vector<uint8_t> commands;
  bool begin(int sck, int miso, int mosi, int cs) { assert(owned); assert(sck == 11 && miso == 10 && mosi == 9 && cs == 12); return spiOK; }
  void beginTransaction(SPISettings s) { assert(owned && !active && s.hz == 20000000); active = true; ++transactions; }
  uint8_t transfer(uint8_t v) { assert(owned && active && pins[41] == LOW && pins[12] == HIGH); if(!pins[16]) commands.push_back(v); ++bytes; return 0; }
  void writeBytes(const uint8_t *, size_t n) { assert(owned && active && pins[41] == LOW && pins[12] == HIGH); bytes += n; }
  void endTransaction() { assert(owned && active && pins[41] == HIGH); active = false; }
};
extern SPIStub SPI;
