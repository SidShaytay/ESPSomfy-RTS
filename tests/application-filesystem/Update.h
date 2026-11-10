#pragma once
#define U_SPIFFS 100
#define U_LITTLEFS 300
#include <functional>
#include <cstddef>
struct FakeUpdate {
 bool beginOk=true,endOk=true,shortWrite=false,error=false;unsigned begins=0,writes=0,ends=0;size_t expected=0;
 std::function<void()> onWrite;
 bool begin(size_t bytes,int){++begins;expected=bytes;if(!beginOk)return false;assert(!running);running=true;return true;}
 size_t write(uint8_t *,size_t bytes){assert(running);++writes;if(onWrite)onWrite();return shortWrite&&bytes?bytes-1:bytes;}
 bool end(bool partial){assert(!partial);++ends;running=false;return endOk;}
 bool hasError(){return error;}
bool running=false,abortFails=false;unsigned aborts=0;void abort(){++aborts;if(!abortFails)running=false;}bool isRunning(){return running;}};
extern FakeUpdate Update;
