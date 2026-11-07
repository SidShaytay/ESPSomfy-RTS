#pragma once
#include <assert.h>
#include "Arduino.h"
#define MSBFIRST 1
#define SPI_MODE0 0
struct SPISettings { unsigned hz; SPISettings(unsigned h, int, int) : hz(h) {} };
struct SPIStub {
  bool transaction = false;
  unsigned hz = 20000000, transactions = 0;
  void begin(int a, int b, int c, int d) { assert(a == 11 && b == 10 && c == 9 && d == 12); }
  void beginTransaction(SPISettings s) { assert(!transaction); transaction = true; hz = s.hz; ++transactions; }
  byte transfer(byte) { assert(transaction && hz == 4000000 && pins[12] == LOW && pins[41] == HIGH); return 0x42; }
  void endTransaction() { assert(transaction); transaction = false; }
  void end() { assert(!transaction); }
};
extern SPIStub SPI;
