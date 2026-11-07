#pragma once
#include <algorithm>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <string>
extern std::string fixtureRoot;
extern std::string failRead;
class File {
  std::string bytes, name;
  size_t offset=0;
  bool exists=false;
public:
  File()=default;
  explicit File(const char *path) : name(path) {
    std::ifstream input(fixtureRoot+path,std::ios::binary);
    exists=bool(input);
    if(exists) bytes.assign(std::istreambuf_iterator<char>(input),{});
  }
  explicit operator bool() const { return exists; }
  size_t size() const { return bytes.size(); }
  size_t available() const { return bytes.size()-offset; }
  size_t read(uint8_t *out,size_t count) {
    if(!exists || name==failRead) return 0;
    count=std::min(count,available());
    std::copy_n(bytes.data()+offset,count,out);offset+=count;return count;
  }
  void close() {}
};
struct TestFS { File open(const char *name,const char *) { return File(name); } };
extern TestFS LittleFS;
