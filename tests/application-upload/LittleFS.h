#pragma once
#include <map>
#include <string>
#include <vector>
#include <stdint.h>
struct FakeLittleFS;
struct File {
  FakeLittleFS *fs=nullptr;std::string path;bool valid=false;
  explicit operator bool() const{return valid;}
  size_t write(const uint8_t *data,size_t count);
  size_t size() const;
  void close(){valid=false;}
};
struct FakeLittleFS {
  std::map<std::string,std::vector<uint8_t>> files;
  unsigned opens=0,writes=0,removes=0;
  bool failOpen=false,shortWrite=false;
  File open(const char *path,const char *mode) {
    ++opens;if(failOpen)return {};
    if(mode[0]=='w')files[path].clear();return {this,path,true};
  }
  bool remove(const char *path){++removes;files.erase(path);return true;}
};
inline size_t File::write(const uint8_t *data,size_t count) {
  ++fs->writes;const auto written=fs->shortWrite && count ? count-1 : count;
  fs->files[path].insert(fs->files[path].end(),data,data+written);return written;
}
inline size_t File::size() const{return fs->files[path].size();}
inline FakeLittleFS LittleFS;
