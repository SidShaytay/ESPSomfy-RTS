#pragma once
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <string>
extern std::string fixtureRoot;
extern std::string failRead;
extern bool failSeek;
extern bool stopRequested;
extern unsigned activeFiles;
bool fileDirectory(const char *name);
class File {
  std::string bytes, name;
  size_t offset=0;
  bool exists=false;
public:
  File()=default;
  explicit File(const char *path) : name(path) {
    assert(!stopRequested);
    assert(name!="/shades.cfg" && name!="/shades.tmp");
    std::ifstream input(fixtureRoot+path,std::ios::binary);
    exists=bool(input);
    if(exists) { bytes.assign(std::istreambuf_iterator<char>(input),{});++activeFiles; }
  }
  File(const File &)=delete;
  File &operator=(const File &)=delete;
  ~File() { close(); }
  explicit operator bool() const { return exists; }
  bool isDirectory() const { return fileDirectory(name.c_str()); }
  size_t size() const { return bytes.size(); }
  bool seek(size_t at) { assert(!stopRequested);if(failSeek) return false;offset=at;return at<=bytes.size(); }
  size_t available() const { return bytes.size()-offset; }
  size_t read(uint8_t *out,size_t count) {
    assert(!stopRequested);
    if(!exists || name==failRead) return 0;
    count=std::min(count,available());
    std::copy_n(bytes.data()+offset,count,out);offset+=count;return count;
  }
  void close() { if(exists) { exists=false;--activeFiles; } }
};
struct TestFS { const char *mountpoint() { return "/littlefs"; } File open(const char *name,const char *) { return File(name); } };
extern TestFS LittleFS;
