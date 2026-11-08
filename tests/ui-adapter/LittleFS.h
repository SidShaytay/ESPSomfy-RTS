#pragma once
struct FakeLittleFS {bool remove(const char *) {return true;}};
inline FakeLittleFS LittleFS;
