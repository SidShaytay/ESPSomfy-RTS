#include "LocalFrequencyScan.h"
#include <new>
#include <stdio.h>
#include <string.h>
namespace LocalUi {namespace RadioScan {
using namespace EntityAdmin;
namespace {
template<size_t N,size_t M>void text(char (&out)[N],const char (&value)[M]){static_assert(M<=N,"Scan label exceeds frame capacity");memcpy(out,value,M);}
}
struct Form::Session {
 enum class Page:uint8_t{confirm,active,refused,selected}page=Page::confirm;
 ::FrequencyScan::Source source;FrequencyScanToken token;SecuritySnapshot auth;
 ::FrequencyScan::Phase observedPhase=::FrequencyScan::Phase::idle;
 uint64_t observedAt=0,activityAt=0;uint8_t cursor=0;
};
void Form::close(Notice reason){
 if(session){
  if(session->token.valid()){
   const auto view=backend.status();
   if(view.token.same(session->token)&&(view.phase==::FrequencyScan::Phase::scanning||view.phase==::FrequencyScan::Phase::stopping))backend.stop(session->source,session->token);
  }
  session->~Session();allocator.deallocate(session);session=nullptr;
 }
 notice=reason;
}
bool Form::open(uint64_t now){
 discard();const auto auth=backend.authorization();const auto source=backend.capture();
 if(!auth.allowed||!auth.generation||!auth.configurationGeneration){notice=Notice::locked;return false;}
 if(!source.valid()||source.kind!=::FrequencyScan::SourceKind::lcd||source.session!=auth.generation||source.configuration!=auth.configurationGeneration){notice=Notice::changed;return false;}
 auto *memory=allocator.allocate(sizeof(Session));if(!memory){notice=Notice::noMemory;return false;}
 session=new(memory)Session{};session->auth=auth;session->source=source;session->observedAt=session->activityAt=now;return guard(now);
}
bool Form::guard(uint64_t now){
 if(!session)return false;
 if(now<session->observedAt||now-session->activityAt>=120000){close(Notice::expired);return false;}
 session->observedAt=now;const auto auth=backend.authorization();
 if(!auth.allowed){close(Notice::locked);return false;}
 if(auth.generation!=session->auth.generation||auth.configurationGeneration!=session->auth.configurationGeneration||!session->source.same(backend.capture())){close(Notice::changed);return false;}
 if(session->page==Session::Page::active){
  const auto view=backend.status();
  if(!view.token.same(session->token)){close(Notice::changed);return false;}
  if(view.phase!=session->observedPhase){session->observedPhase=view.phase;session->cursor=0;}
 }
 return true;
}
void Form::touch(uint64_t now){session->activityAt=now;backend.activity();}
void Form::tick(uint64_t now){guard(now);}
void Form::turn(int direction,uint64_t now){
 if(!guard(now)||!direction)return;touch(now);auto &s=*session;
 const auto view=backend.status();
 if(s.page==Session::Page::selected)return;
 if(s.page==Session::Page::active&&view.phase==::FrequencyScan::Phase::stopping)return;
 unsigned count=2;
 if(s.page==Session::Page::active&&(!view.token.same(s.token)||view.phase==::FrequencyScan::Phase::attention||
   (view.phase==::FrequencyScan::Phase::review&&(!view.sampleValid||view.sample.peakRssi<=-100))))count=1;
 s.cursor=(s.cursor+(direction>0?1:count-1))%count;
}
void Form::activate(uint64_t now){
 if(!guard(now))return;touch(now);auto &s=*session;
 if(s.page==Session::Page::confirm){
  if(!s.cursor){close(Notice::none);return;}
  if(!backend.begin(s.source,s.token)){s.page=Session::Page::refused;s.cursor=0;return;}
  s.page=Session::Page::active;s.observedPhase=::FrequencyScan::Phase::scanning;s.cursor=0;return;
 }
 if(s.page==Session::Page::refused){if(!s.cursor)close(Notice::none);else{s.page=Session::Page::confirm;s.cursor=0;}return;}
 if(s.page!=Session::Page::active)return;
 const auto view=backend.status();
 if(!view.token.same(s.token)||view.phase==::FrequencyScan::Phase::attention){close(Notice::none);return;}
 if(view.phase==::FrequencyScan::Phase::stopping)return;
 if(view.phase==::FrequencyScan::Phase::scanning){
  if(!s.cursor){backend.stop(s.source,s.token);s.cursor=0;}else close(Notice::none);
  return;
 }
 if(view.phase==::FrequencyScan::Phase::review){
  if(!s.cursor){close(Notice::none);return;}
  float value=0;
  if(!backend.peak(s.source,s.token,value)||!std::isfinite(value)||value<433||value>434){s.page=Session::Page::refused;s.cursor=0;return;}
  s.page=Session::Page::selected;
 }
}
void Form::back(uint64_t now){if(guard(now)){touch(now);close(Notice::none);}}
bool Form::takeFrequency(float &value,SecuritySnapshot &auth,uint64_t &revision,uint64_t now){
 if(!guard(now)||session->page!=Session::Page::selected)return false;
 float frequency=0;
 if(!backend.peak(session->source,session->token,frequency)||!std::isfinite(frequency)||frequency<433||frequency>434){close(Notice::changed);return false;}
 value=frequency;auth=session->auth;revision=session->source.radioRevision;close(Notice::none);return true;
}
void Form::render(FormView &out)const{
 out={};out.notice=notice;if(!session)return;const auto &s=*session;
 text(out.title,"Radio frequency scan");out.mode=Mode::confirm;out.selected=s.cursor;out.total=out.rowCount=2;
 if(s.page==Session::Page::confirm){text(out.rows[0].label,"Cancel");text(out.rows[1].label,"Start radio scan");text(out.hint,"Wired controls work; RF paused. Remotes can move motors.");}
 else if(s.page==Session::Page::refused){out.mode=Mode::validation;text(out.rows[0].label,"Close");text(out.rows[1].label,"Review Start again");text(out.hint,"Scan not admitted; cleanup may be pending. No automatic retry.");}
 else if(s.page==Session::Page::selected){out.total=out.rowCount=1;text(out.rows[0].label,"Opening Radio draft");}
 else {
  const auto view=backend.status();out.mode=Mode::validation;
  if(!view.token.same(s.token)||view.phase==::FrequencyScan::Phase::attention){out.total=out.rowCount=1;text(out.rows[0].label,"Scan needs attention");text(out.hint,"Radio restoration not verified. Close and review state.");}
  else if(view.phase==::FrequencyScan::Phase::stopping){out.total=out.rowCount=1;text(out.rows[0].label,"Restoring radio");text(out.hint,"RF remains paused until tuning and receive state restore.");}
  else if(view.phase==::FrequencyScan::Phase::scanning){text(out.rows[0].label,"Stop scan");text(out.rows[1].label,"Close and stop");text(out.hint,"RF paused; GPIO available. Back stops this scan.");}
  else {text(out.rows[0].label,"Close");text(out.rows[1].label,"Copy to Radio draft");text(out.hint,"Review result; copying never saves or transmits.");
   if(!view.sampleValid||view.sample.peakRssi<=-100){out.total=out.rowCount=1;text(out.hint,"No valid signal result. Nothing to apply.");}}
  if(view.sampleValid&&out.rowCount==2){out.mode=Mode::stackedRows;out.rowCount=3;snprintf(out.rows[2].label,sizeof(out.rows[2].label),"Now %.2f MHz",double(view.sample.frequency));snprintf(out.rows[2].value,sizeof(out.rows[2].value),"Peak %.2f / %d dBm",double(view.sample.peakFrequency),int(view.sample.peakRssi));}
 }
 for(uint8_t i=0;i<out.rowCount;++i)out.rows[i].selected=i==out.selected;
}
}}
