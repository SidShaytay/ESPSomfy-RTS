#pragma once
#include <stdint.h>
extern unsigned storageReads,storageWrites;
class Preferences { public:
 bool begin(const char *){return true;}
 uint16_t getUShort(const char *,uint16_t){++storageReads;return 10;}
 void putUShort(const char *,uint16_t){++storageWrites;}
 void end(){}
};
