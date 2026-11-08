#pragma once
#include "Arduino.h"
class IPAddress {
  uint8_t bytes[4] = {};
public:
  IPAddress() = default;
  IPAddress(uint8_t a, uint8_t b, uint8_t c, uint8_t d) : bytes{a,b,c,d} {}
  uint8_t operator[](int index) const { return bytes[index]; }
  bool fromString(const char *text) {
    unsigned a,b,c,d;
    if(sscanf(text, "%u.%u.%u.%u", &a,&b,&c,&d) != 4 || a>255 || b>255 || c>255 || d>255) return false;
    bytes[0]=a; bytes[1]=b; bytes[2]=c; bytes[3]=d;
    return true;
  }
  String toString() const {
    char text[16];
    snprintf(text,sizeof(text),"%u.%u.%u.%u",bytes[0],bytes[1],bytes[2],bytes[3]);
    return String(text);
  }
};
