#pragma once
#include "../../test/stubs/Arduino.h"
using boolean=bool;
#define log_w(...) ((void)0)
class Stream {
protected:unsigned long _timeout=0;
public:
 virtual ~Stream()=default;
 virtual size_t write(uint8_t)=0;
 virtual size_t write(const uint8_t*,size_t)=0;
 virtual int available()=0;virtual int read()=0;virtual int peek()=0;virtual void flush()=0;
};
