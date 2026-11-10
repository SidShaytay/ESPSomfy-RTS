#pragma once
#define U_SPIFFS 100
#define U_LITTLEFS 300
struct FakeUpdate {bool running=false,abortFails=false;unsigned aborts=0;void abort(){++aborts;if(!abortFails)running=false;}bool isRunning(){return running;}};
extern FakeUpdate Update;
