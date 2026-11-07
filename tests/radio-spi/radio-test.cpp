#include "SPI.h"
#include "ELECHOUSE_CC1101_SRC_DRV.h"
#include <stdio.h>
int pins[64] = {};
SPIStub SPI;
int main() {
  pins[12] = pins[41] = HIGH;
  ELECHOUSE_cc1101.setSpiPin(11, 10, 9, 12);
  assert(ELECHOUSE_cc1101.SpiReadReg(0) == 0x42);
  assert(!SPI.transaction && pins[12] == HIGH && SPI.transactions == 1);
  SPI.beginTransaction(SPISettings(20000000, MSBFIRST, SPI_MODE0)); // intervening display
  SPI.endTransaction();
  assert(ELECHOUSE_cc1101.SpiReadStatus(0) == 0x42);
  assert(!SPI.transaction && pins[12] == HIGH && SPI.transactions == 3);
  ELECHOUSE_cc1101.Init(); // reset and register writes must not nest transactions
  assert(!SPI.transaction && pins[12] == HIGH);
  puts("Radio SPI: restored mode/clock and balanced transactions after display access");
}
