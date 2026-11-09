#pragma once
#include <map>
#include <string>
#include <functional>
struct {std::map<std::string,std::string> files;unsigned removes=0;bool failRemove=false;std::function<void()> beforeRemove;
 bool remove(const char *path){if(beforeRemove)beforeRemove();++removes;if(failRemove)return false;files.erase(path);return true;}
}LittleFS;
