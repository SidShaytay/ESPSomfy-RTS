#pragma once
#include <stddef.h>
#include <stdio.h>
#include <sys/stat.h>
enum class ModelLoadStatus : unsigned char { unknown, loaded, absent, unavailable };
struct ModelLoadIo {
  FILE *(*open)(const char *,const char *);
  int (*stat)(const char *,struct stat *);
  size_t (*read)(void *,size_t,size_t,FILE *);
  int (*error)(FILE *);
  int (*close)(FILE *);
  void *(*allocate)(size_t);
  void (*release)(void *,size_t);
};
const ModelLoadIo &modelLoadIo();
void noteModelFilesystemMounted(bool mounted);
bool modelFilesystemMounted();
// Immutable input only: no decoded model or credentials are logged. Capture
// completes (including checked close) before any parser can mutate live state.
class ModelLoadSnapshot {
  unsigned char *bytes=nullptr;
  size_t length=0;
  void (*releaseBytes)(void *,size_t)=nullptr;
public:
  // Current maximum model records need under 32 KiB. Additional bounded space
  // accommodates historical backup settings tails without trusting header sizes.
  static constexpr size_t maximumBytes=65536;
  ModelLoadSnapshot()=default;
  ModelLoadSnapshot(const ModelLoadSnapshot &)=delete;
  ModelLoadSnapshot &operator=(const ModelLoadSnapshot &)=delete;
  ~ModelLoadSnapshot(){clear();}
  void clear();
  ModelLoadStatus capture(const char *path,bool filesystemMounted,
                          const ModelLoadIo &io=modelLoadIo());
  const unsigned char *data()const{return bytes;}
  size_t size()const{return length;}
};
