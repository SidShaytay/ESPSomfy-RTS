#include "GatedReadOnlyFilesystem.h"
#include <FSImpl.h>
#include <cstring>
#include <utility>
#include <mutex>
namespace {
class ReadOnlyFile final:public fs::FileImpl {
  fs::File file;
  MountedFilesystemGate::Lease lease;
  mutable std::mutex mutex;
  using Lock=std::lock_guard<std::mutex>;
public:
  ReadOnlyFile(fs::File source,MountedFilesystemGate::Lease held):file(std::move(source)),lease(std::move(held)){}
  ~ReadOnlyFile()override{close();}
  size_t write(const uint8_t*,size_t)override{return 0;}
  size_t read(uint8_t *out,size_t length)override{Lock hold(mutex);return lease?file.read(out,length):0;}
  void flush()override{} // Read-only response never flushes a filesystem writer.
  bool seek(uint32_t pos,fs::SeekMode mode)override{Lock hold(mutex);return lease&&file.seek(pos,mode);}
  size_t position()const override{Lock hold(mutex);return lease?file.position():0;}
  size_t size()const override{Lock hold(mutex);return lease?file.size():0;}
  bool setBufferSize(size_t size)override{Lock hold(mutex);return lease&&file.setBufferSize(size);}
  void close()override{Lock hold(mutex);file.close();lease.close();} // Close FIRST; only then can the mount drain.
  time_t getLastWrite()override{Lock hold(mutex);return lease?file.getLastWrite():0;}
  const char *path()const override{Lock hold(mutex);return lease?file.path():nullptr;}
  const char *name()const override{Lock hold(mutex);return lease?file.name():nullptr;}
  boolean isDirectory()override{Lock hold(mutex);return lease&&file.isDirectory();}
  // Static asset serving does not enumerate directories or export new handles.
  fs::FileImplPtr openNextFile(const char*)override{return {};}
  boolean seekDir(long)override{return false;}
  String getNextFileName()override{return {};}
  String getNextFileName(bool*)override{return {};}
  void rewindDirectory()override{}
  operator bool()override{Lock hold(mutex);return lease&&bool(file);}
};
class ReadOnlyFilesystem final:public fs::FSImpl {
  fs::FS &source;MountedFilesystemGate &gate;
public:
  ReadOnlyFilesystem(fs::FS &value,MountedFilesystemGate &guard):source(value),gate(guard){}
  fs::FileImplPtr open(const char *path,const char *mode,bool create)override {
    if(!path || !mode || std::strcmp(mode,"r") || create)return {};
    auto lease=gate.enter();if(!lease)return {};
    auto file=source.open(path,mode,false);if(!file)return {};
    return std::make_shared<ReadOnlyFile>(std::move(file),std::move(lease));
  }
  bool exists(const char *path)override{auto lease=gate.enter();return lease&&path&&source.exists(path);}
  bool rename(const char*,const char*)override{return false;}
  bool remove(const char*)override{return false;}
  bool mkdir(const char*)override{return false;}
  bool rmdir(const char*)override{return false;}
};
}
GatedReadOnlyFilesystem::GatedReadOnlyFilesystem(fs::FS &source,MountedFilesystemGate &gate)
  :fs::FS(std::make_shared<ReadOnlyFilesystem>(source,gate)){}
MountedFilesystemGate &mountedFilesystemGate(){static MountedFilesystemGate gate;return gate;}
