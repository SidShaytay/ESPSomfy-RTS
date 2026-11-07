#include <cassert>
#include <functional>
#include <map>
#include <string>
#include <cstdint>
#include "ManualUpload.h"
struct Request {
  std::map<std::string,bool> attributes;
  std::function<void()> disconnect;
  bool getAttribute(const char *name,bool fallback) { auto it=attributes.find(name);return it==attributes.end()?fallback:it->second; }
  void setAttribute(const char *name,bool value) { attributes[name]=value; }
  void onDisconnect(std::function<void()> callback) { disconnect=callback; }
};
struct Gate {
  bool owned=false;unsigned claims=0,releases=0;
  bool beginManualUpdate() { ++claims;if(owned)return false;owned=true;return true; }
  void endManualUpdate() { assert(owned);owned=false;++releases; }
};
struct Writer {
  unsigned starts=0,writes=0,ends=0,aborts=0;
  bool running=false,error=false,failBegin=false,failWrite=false,failEnd=false;
  bool begin(int) { ++starts;if(failBegin)return false;running=true;return true; }
  size_t write(uint8_t *,size_t size) { ++writes;return failWrite ? size-1 : size; }
  bool end() { ++ends;running=false;return !failEnd; }
  bool isRunning() { return running; }
  bool hasError() { return error; }
  void abort() { ++aborts;running=false; }
};
struct Effects {
  unsigned quiesces=0,reboots=0;uint32_t now=0;bool timedOut=false;ManualUploadDeadline deadline;
  void quiesce() { ++quiesces;deadline.begin(now); }
  void reboot() { ++reboots;deadline.shorten(now); }
  bool expired() { return timedOut || deadline.expired(now); }
};
static uint8_t bytes[4]={1,2,3,4};
int main() {
  {
    ManualUploadDeadline deadline;
    deadline.begin(0xfffffff0u);
    assert(!deadline.expired(0xfffffff1u));
    assert(deadline.expired(0xfffffff0u+180000u));
    deadline.shorten(0xfffffff0u);
    const uint32_t firstFailure=deadline.at();
    assert(firstFailure==484u);
    deadline.shorten(10);
    assert(deadline.at()==firstFailure); // hostile repeated chunks cannot defer reboot
    assert(!deadline.expired(483u) && deadline.expired(484u));
  }
  {
    Gate gate;Writer writer;Effects effects;Request request;
    assert(beginManualUpload(&request,gate,writer,effects,0));
    assert(writeManualUpload(&request,writer,effects,bytes,sizeof(bytes),true));
    assert(effects.deadline.at()==180000); // fallback is armed while finalization is pending
    assert(writer.ends==0); // file-final must not make an image bootable
    assert(finishManualUpload(&request,writer,effects)==ManualUploadResult::success);
    assert(writer.ends==1 && effects.quiesces==1 && effects.reboots==1 && gate.owned);
    assert(effects.deadline.at()==500);
    assert(finishManualUpload(&request,writer,effects)==ManualUploadResult::success);
    assert(writer.ends==1); // completion cannot activate twice
    request.disconnect();
    assert(writer.aborts==0 && gate.owned); // success keeps gate until reboot
  }
  {
    Gate gate;Writer writer;Effects effects;Request request;
    assert(beginManualUpload(&request,gate,writer,effects,0));
    assert(writeManualUpload(&request,writer,effects,bytes,sizeof(bytes),true));
    assert(!beginManualUpload(&request,gate,writer,effects,0)); // second multipart file
    assert(request.getAttribute("manualOwned",false));
    assert(!writeManualUpload(&request,writer,effects,bytes,sizeof(bytes),true));
    assert(finishManualUpload(&request,writer,effects)==ManualUploadResult::failed);
    assert(writer.ends==0 && writer.writes==1 && writer.aborts==1);
    assert(effects.reboots>0 && gate.owned && gate.releases==0 && gate.claims==1);
  }
  {
    Gate gate;Writer writer;Effects effects;Request owner,refused,noFile;
    assert(beginManualUpload(&owner,gate,writer,effects,0));
    assert(!beginManualUpload(&refused,gate,writer,effects,1));
    assert(!writeManualUpload(&refused,writer,effects,bytes,sizeof(bytes),true));
    assert(finishManualUpload(&refused,writer,effects)==ManualUploadResult::notStarted);
    assert(finishManualUpload(&noFile,writer,effects)==ManualUploadResult::notStarted);
    // The HTTP adapter calls this on unauthorized chunks; those requests must
    // never abort a different client's writer or shorten its reboot deadline.
    failManualUpload(&refused,writer,effects);
    failManualUpload(&noFile,writer,effects);
    assert(writer.aborts==0 && effects.reboots==0 && gate.owned && gate.releases==0);
    owner.disconnect();
    assert(writer.aborts==1 && effects.reboots>0 && writer.ends==0 && gate.owned);
  }
  {
    Gate gate;Writer writer;Effects effects;Request request;
    writer.failBegin=true;
    assert(!beginManualUpload(&request,gate,writer,effects,0));
    assert(!gate.owned && gate.releases==1 && effects.quiesces==0);
    assert(finishManualUpload(&request,writer,effects)==ManualUploadResult::notStarted);
    assert(effects.reboots==0 && writer.ends==0);
  }
  {
    Gate gate;Writer writer;Effects effects;Request request;
    assert(beginManualUpload(&request,gate,writer,effects,0));
    writer.failWrite=true;
    assert(!writeManualUpload(&request,writer,effects,bytes,sizeof(bytes),false));
    assert(!writeManualUpload(&request,writer,effects,bytes,sizeof(bytes),true));
    assert(finishManualUpload(&request,writer,effects)==ManualUploadResult::failed);
    assert(writer.ends==0 && writer.writes==1 && writer.aborts==1 && gate.owned && effects.reboots>0);
  }
  {
    Gate gate;Writer writer;Effects effects;Request request;
    assert(beginManualUpload(&request,gate,writer,effects,1));
    assert(writeManualUpload(&request,writer,effects,bytes,sizeof(bytes),false));
    assert(finishManualUpload(&request,writer,effects)==ManualUploadResult::failed); // truncated multipart
    assert(writer.ends==0 && writer.aborts==1 && gate.owned && effects.reboots>0);
  }
  {
    Gate gate;Writer writer;Effects effects;Request request;
    assert(beginManualUpload(&request,gate,writer,effects,0));
    effects.now=180000;
    assert(!writeManualUpload(&request,writer,effects,bytes,sizeof(bytes),true));
    assert(finishManualUpload(&request,writer,effects)==ManualUploadResult::failed);
    assert(writer.ends==0 && writer.writes==0 && gate.owned && effects.reboots>0);
  }
  {
    Gate gate;Writer writer;Effects effects;Request request;
    assert(beginManualUpload(&request,gate,writer,effects,1));
    assert(!writeManualUpload(&request,writer,effects,nullptr,0,true));
    assert(finishManualUpload(&request,writer,effects)==ManualUploadResult::failed);
    assert(writer.ends==0 && gate.owned && effects.reboots>0);
  }
  {
    Gate gate;Writer writer;Effects effects;Request request;
    assert(beginManualUpload(&request,gate,writer,effects,0));
    assert(writeManualUpload(&request,writer,effects,bytes,sizeof(bytes),true));
    effects.timedOut=true;
    assert(finishManualUpload(&request,writer,effects)==ManualUploadResult::failed);
    assert(writer.ends==0 && writer.aborts==1 && gate.owned && effects.reboots>0);
  }
  {
    Gate gate;Writer writer;Effects effects;Request request;
    assert(beginManualUpload(&request,gate,writer,effects,0));
    assert(writeManualUpload(&request,writer,effects,bytes,sizeof(bytes),true));
    writer.failEnd=true;
    assert(finishManualUpload(&request,writer,effects)==ManualUploadResult::failed);
    assert(writer.ends==1 && gate.owned && effects.reboots>0);
  }
}
