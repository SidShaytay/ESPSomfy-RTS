#pragma once
#include "Arduino.h"
struct IPAddress {
 uint32_t value=0;
 IPAddress(uint32_t v=0):value(v){}
 IPAddress(unsigned a,unsigned b,unsigned c,unsigned d):value((a<<24)|(b<<16)|(c<<8)|d){}
 bool operator==(const IPAddress &other)const{return value==other.value;}
};
