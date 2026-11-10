#include "GatedReadOnlyFilesystem.h"
#include <FSImpl.h>
#include <cassert>
#include <atomic>
#include <thread>
#include <vector>
#include <cstdio>
#include <future>
#include <chrono>
struct Source:fs::FSImpl {
 MountedFilesystemGate &gate;unsigned opens=0,closes=0;bool missing=false;
 std::atomic<bool> pauseRead{false},reading{false},releaseRead{false};
 explicit Source(MountedFilesystemGate &g):gate(g){}
 struct Handle:fs::FileImpl {
  Source &source;bool opened=true;size_t offset=0;
  explicit Handle(Source &s):source(s){}
  ~Handle(){close();}
  size_t write(const uint8_t*,size_t)override{assert(false);return 0;}
  size_t read(uint8_t *out,size_t length)override{assert(opened);if(source.pauseRead){source.reading=true;while(!source.releaseRead)std::this_thread::yield();}if(!length||offset>=4)return 0;*out='x';++offset;return 1;}
  void flush()override{}
  bool seek(uint32_t pos,fs::SeekMode)override{offset=pos;return true;}
  size_t position()const override{return offset;}size_t size()const override{return 4;}
  bool setBufferSize(size_t)override{return true;}
  void close()override{if(opened){assert(!source.gate.drained());++source.closes;opened=false;}}
  time_t getLastWrite()override{return 123;}
  const char *path()const override{return "/index.html";}const char *name()const override{return "index.html";}
  boolean isDirectory()override{return false;}
  fs::FileImplPtr openNextFile(const char*)override{return {};}
  boolean seekDir(long)override{return false;}
  String getNextFileName()override{return {};}
  String getNextFileName(bool*)override{return {};}
  void rewindDirectory()override{}
  operator bool()override{return opened;}
 };
 fs::FileImplPtr open(const char*,const char*,bool)override{++opens;return missing?fs::FileImplPtr{}:std::make_shared<Handle>(*this);}
 bool exists(const char*)override{++opens;return !missing;}
 bool rename(const char*,const char*)override{assert(false);return false;}
 bool remove(const char*)override{assert(false);return false;}
 bool mkdir(const char*)override{assert(false);return false;}
 bool rmdir(const char*)override{assert(false);return false;}
};
static void actual_sdk_file_aliases_hold_gate_until_underlying_close(){
 MountedFilesystemGate gate;auto impl=std::make_shared<Source>(gate);fs::FS source(impl);GatedReadOnlyFilesystem web(source,gate);
 auto response=web.open("/index.html");assert(response);auto request=response;
 assert(response.size()==4&&response.getLastWrite()==123&&response.read()=='x');
 assert(gate.seal()&&!gate.drained());auto refused=web.open("/index.html");assert(!refused&&impl->opens==1);
 response=fs::File{};assert(!gate.drained()); // Dropping one alias cannot release the actual open file.
 request.close();assert(gate.drained()&&impl->closes==1);
 assert(!web.exists("/index.html")&&impl->opens==1);
 gate.resume();assert(web.exists("/index.html"));
 {auto cancelled=web.open("/index.html");assert(cancelled);gate.seal();assert(!gate.drained());}
 assert(gate.drained()&&impl->closes==2); // Last-alias destruction closes before releasing its lease.
 gate.resume();impl->missing=true;assert(!web.open("/missing"));assert(gate.seal()&&gate.drained());
}
static void read_only_surface_never_calls_underlying_writes(){
 MountedFilesystemGate gate;auto impl=std::make_shared<Source>(gate);fs::FS source(impl);GatedReadOnlyFilesystem web(source,gate);
 assert(!web.open("/index.html","w"));assert(!web.open("/index.html","a"));assert(!web.open("/index.html","r",true));
 assert(!web.remove("/index.html")&&!web.rename("/a","/b")&&!web.mkdir("/dir")&&!web.rmdir("/dir"));assert(!impl->opens);
 auto file=web.open("/index.html");const uint8_t byte=1;assert(!file.write(&byte,1));
 file.close();assert(gate.seal()&&gate.drained());
}
static void concurrent_close_waits_for_read_before_releasing_mount_lease(){
 MountedFilesystemGate gate;auto impl=std::make_shared<Source>(gate);fs::FS source(impl);GatedReadOnlyFilesystem web(source,gate);
 auto reader=web.open("/index.html"),closer=reader;impl->pauseRead=true;
 auto read=std::async(std::launch::async,[&]{return reader.read();});
 while(!impl->reading)std::this_thread::yield();
 gate.seal();
 auto close=std::async(std::launch::async,[&]{closer.close();});
 assert(close.wait_for(std::chrono::milliseconds(20))==std::future_status::timeout);
 assert(!gate.drained());impl->releaseRead=true;assert(read.get()=='x');close.get();assert(gate.drained());
 assert(reader.read()==-1 && reader.size()==0); // A remaining alias cannot touch the unmounted source.
}
static void concurrent_seal_never_observes_active_untracked_io(){
 for(unsigned run=0;run<80;++run){
  MountedFilesystemGate gate;std::atomic<unsigned> active{0};std::atomic<bool> start{false};std::vector<std::thread> readers;
  for(unsigned n=0;n<4;++n)readers.emplace_back([&]{while(!start.load()){}for(unsigned i=0;i<500;++i){auto handle=gate.enter();if(handle){++active;--active;}}});
  start=true;gate.seal();for(auto &thread:readers)thread.join();assert(gate.drained()&&!active.load());assert(!gate.enter());
 }
}
int main(){concurrent_close_waits_for_read_before_releasing_mount_lease();actual_sdk_file_aliases_hold_gate_until_underlying_close();read_only_surface_never_calls_underlying_writes();concurrent_seal_never_observes_active_untracked_io();puts("Actual pinned SDK File/FS: alias lifetime, close-before-drain, read-only denial, failed open and concurrent seal passed");}
