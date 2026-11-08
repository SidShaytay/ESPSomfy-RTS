#pragma once
#include <vector>
#include <string>
#include <cstring>
#include <cerrno>
#define DT_REG 8
#define DT_DIR 4
struct dirent { char d_name[256]; unsigned char d_type; };
struct Entry { std::string name; unsigned char kind; };
extern std::vector<Entry> rootEntries;
extern int rootFailure;
extern bool stopRequested;
extern unsigned activeDirectories;
struct DIR { size_t at=0; };
inline DIR *opendir(const char *) {
  assert(!stopRequested);
  if(rootFailure==1) { errno=EIO;return nullptr; }
  ++activeDirectories;return new DIR;
}
inline dirent *readdir(DIR *dir) {
  assert(!stopRequested);
  if(rootFailure==2) { errno=EIO;return nullptr; }
  if(dir->at>=rootEntries.size()) return nullptr;
  static dirent entry;
  const auto &next=rootEntries[dir->at++];
  memset(&entry,0,sizeof(entry));
  memcpy(entry.d_name,next.name.data(),next.name.size()<sizeof(entry.d_name) ? next.name.size() : sizeof(entry.d_name));
  entry.d_type=next.kind;return &entry;
}
inline int closedir(DIR *dir) { delete dir;--activeDirectories;if(rootFailure==3) {errno=EIO;return -1;}return 0; }
