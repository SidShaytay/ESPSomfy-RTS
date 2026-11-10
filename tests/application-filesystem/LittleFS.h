#pragma once
#include <cassert>
struct FakeLittleFS {
 bool mounted=true,unmountFails=false,mountFails=false;unsigned ends=0,begins=0;
 void end(){++ends;if(!unmountFails)mounted=false;}
 bool begin(bool format){assert(!format);++begins;if(mountFails)return false;mounted=true;return true;}
};
extern FakeLittleFS LittleFS;
