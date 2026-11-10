#pragma once
#include "FrequencyScanToken.h"
#include "GitUpdateOwner.h"
#include "FrequencyScanSample.h"
#include <cmath>
namespace FrequencyScan {
enum class SourceKind:uint8_t {web,lcd};
struct Source {
 SourceKind kind=SourceKind::web;uint32_t client=0,session=0,configuration=0;
 uint64_t securityRevision=0,radioRevision=0;
 bool valid()const{return securityRevision&&radioRevision&&(kind==SourceKind::web?client&&!session&&!configuration:kind==SourceKind::lcd&&!client&&session&&configuration);}
 bool same(const Source &other)const{return valid()&&other.valid()&&kind==other.kind&&client==other.client&&session==other.session&&configuration==other.configuration&&securityRevision==other.securityRevision&&radioRevision==other.radioRevision;}
};
enum class Phase:uint8_t {idle,scanning,stopping,review,attention};
enum class Reason:uint8_t {none,busy,stale,expired,clock,ownership};
struct Snapshot {FrequencyScanToken token;Phase phase=Phase::idle;Reason reason=Reason::none;Sample sample;uint64_t sampledAt=0;bool sampleValid=false;};
class Backend {
public:
 virtual ~Backend()=default;
 virtual bool current(const Source&)=0;
 virtual bool acquire(GitUpdateOwnerToken&)=0;
 virtual bool owns(const GitUpdateOwnerToken&)=0;
 virtual bool begin()=0;
 virtual void stop()=0;
 virtual bool active()=0;
 virtual bool sample(Sample&)=0;
 virtual bool release(const GitUpdateOwnerToken&)=0;
};
// Loop-owned bounded context around the EXISTING scan driver and update owner.
// No worker, allocation, queue, configuration writer or second radio engine.
class Session {
 Backend &backend;const uint64_t bootSession;Source source;GitUpdateOwnerToken owner;
 Snapshot view;uint64_t startedAt=0,observedAt=0;
 bool owned=false;
 void requestStop(Reason reason){view.phase=Phase::stopping;view.reason=reason;backend.stop();}
 void finish(){
  if(!backend.release(owner)){view.phase=Phase::attention;view.reason=Reason::ownership;return;}
  owned=false;owner={};view.phase=Phase::review;
 }
 static bool validSample(const Sample &value){return std::isfinite(value.frequency)&&std::isfinite(value.peakFrequency)&&value.frequency>=433&&value.frequency<=434&&value.peakFrequency>=433&&value.peakFrequency<=434&&value.rssi>=-127&&value.rssi<=0&&value.peakRssi>=-127&&value.peakRssi<=0;}
public:
 static constexpr uint64_t maximumMs=300000;
 Session(Backend &value,uint64_t boot):backend(value),bootSession(boot){}
 Session(const Session&)=delete;Session &operator=(const Session&)=delete;
 Snapshot snapshot()const{return view;}
 bool start(const Source &expected,uint64_t now,FrequencyScanToken &out){
  if(!bootSession||owned||view.phase==Phase::attention||!expected.valid()||!backend.current(expected))return false;
  GitUpdateOwnerToken lease;
  if(!backend.acquire(lease))return false;
  owner=lease;owned=true;source=expected;view={};
  if(!lease.generation||lease.kind!=GitUpdateExternalOwner::configuration||!backend.owns(lease)){
   view.phase=Phase::attention;view.reason=Reason::ownership;return false;
  }
  if(!backend.current(expected)||!backend.begin()){
   if(!backend.release(owner)){view.phase=Phase::attention;view.reason=Reason::ownership;return false;}
   owner={};owned=false;return false;
  }
  view={};view.token={bootSession,lease.generation};view.phase=Phase::scanning;startedAt=observedAt=now;
  if(!backend.current(expected)){requestStop(Reason::stale);return false;}
  out=view.token;return true;
 }
 bool stop(const FrequencyScanToken &expected,const Source &caller,uint64_t now){
  if(!owned||!expected.same(view.token)||!source.same(caller)||!backend.owns(owner))return false;
  if(view.phase==Phase::stopping)return true;
  if(view.phase!=Phase::scanning)return false;
  const bool backwards=now<observedAt;if(now>observedAt)observedAt=now;
  requestStop(backwards?Reason::clock:Reason::none);return true;
 }
 bool peak(const FrequencyScanToken &expected,const Source &caller,float &frequency){
  if(owned||view.phase!=Phase::review||view.reason==Reason::clock||view.reason==Reason::stale||view.reason==Reason::ownership||!view.token.same(expected)||!source.same(caller)||!backend.current(source)||
     !view.sampleValid||view.sample.peakRssi<=-100)return false;
  frequency=view.sample.peakFrequency;return true;
 }
 void poll(uint64_t now){
  if(!owned||view.phase==Phase::attention)return;
  if(!backend.owns(owner)){view.phase=Phase::attention;view.reason=Reason::ownership;return;}
  if(view.phase==Phase::stopping){if(now<observedAt)view.reason=Reason::clock;else if(!backend.current(source))view.reason=Reason::stale;}
  if(view.phase==Phase::scanning){
   if(now<observedAt)requestStop(Reason::clock);
   else if(now-startedAt>=maximumMs)requestStop(Reason::expired);
   else if(!backend.current(source))requestStop(Reason::stale);
   else {
    Sample result;view.sampleValid=backend.sample(result)&&validSample(result);
    if(view.sampleValid){view.sample=result;view.sampledAt=now;}
   }
  }
  if(now>observedAt)observedAt=now;
  if(!backend.active())finish(); // Never release ownership while restoration is pending.
 }
};
static_assert(sizeof(Session)<=192,"One bounded scan context, no retained model or request");
}
