#include "LocalDiagnostics.h"
#include "LocalJsonAllocator.h"
#include "PulseDecoder.h"
#include <new>
#include <stdio.h>
#include <string.h>
namespace LocalUi { namespace Diagnostics {
using namespace EntityAdmin;
namespace {
void wipe(void *data,size_t size){auto *p=static_cast<volatile unsigned char *>(data);while(size--)*p++=0;}
constexpr uint8_t detailRows=17,pulsesAction=15;
}
struct Form::Session final:ReceiveSink {
  struct Record {ReceiveObservation frame;unsigned int pulses[MAX_TIMINGS]{};uint16_t count=0;bool clipped=false;} records[2];
  Form &owner;
  ReceiveSubscription subscription;
  SecuritySnapshot auth;
  uint64_t observed=0,activity=0,lastSequence=0;
  enum class Page:uint8_t {begin,list,details,pulses,clear,failed} page=Page::begin;
  uint8_t count=0,next=0,cursor=0,chosen=0;
  explicit Session(Form &value):owner(value){}
  uint8_t slot(uint8_t ordinal)const{return (next+2-count+ordinal)%2;}
  uint8_t total()const{
    switch(page){
    case Page::begin:case Page::clear:return 2;
    case Page::list:return count+3;
    case Page::details:return detailRows;
    case Page::pulses:return (records[chosen].count+2)/3+1;
    case Page::failed:return 1;
    }
    return 0;
  }
  void received(const ReceiveObservation &value,ReceivePulses pulses)override{owner.receive(value,pulses);}
};
void Form::pause(){
  if(session && session->subscription.generation){
    backend.hub().unsubscribe(session->subscription,*session);
    session->subscription={};
  }
}
void Form::close(Notice reason){
  if(session){pause();session->~Session();allocator.deallocate(session);session=nullptr;}
  notice=reason;
}
bool Form::guard(uint64_t now){
  if(!session)return false;
  const auto auth=backend.authorization();auto &s=*session;
  Notice reason=Notice::none;
  if(now<s.observed || now<s.activity || now-s.activity>=120000)reason=Notice::expired;
  else if(!auth.allowed)reason=Notice::locked;
  else if(!auth.generation || auth.generation!=s.auth.generation ||
      auth.configurationGeneration!=s.auth.configurationGeneration)reason=Notice::changed;
  else if(s.subscription.generation && !backend.hub().observing(s.subscription,s))reason=Notice::changed;
  if(reason!=Notice::none){close(reason);return false;}
  s.observed=now;return true;
}
void Form::touch(uint64_t now){session->activity=now;backend.activity();}
bool Form::open(uint64_t now){
  discard();const auto auth=backend.authorization();
  if(!auth.allowed || !auth.generation || !auth.configurationGeneration){close(Notice::locked);return false;}
  static_assert(sizeof(Session)+JsonAllocator::allocationOverhead<=3072,"Two complete bounded pulse records in wiping PSRAM");
  static_assert(sizeof(unsigned int)<=4,"Raw timing values must fit their advertised 32-bit storage");
  void *memory=allocator.allocate(sizeof(Session));
  if(!memory){close(Notice::noMemory);return false;}
  session=new(memory)Session(*this);session->auth=auth;
  session->observed=session->activity=now;
  return guard(now);
}
void Form::receive(const ReceiveObservation &frame,ReceivePulses pulses){
  const auto now=backend.now();if(!guard(now))return;
  auto &s=*session;
  if(!frame.sequence || frame.sequence<=s.lastSequence || frame.observedAt>now){close(Notice::changed);return;}
  s.lastSequence=frame.sequence;
  auto &record=s.records[s.next];wipe(&record,sizeof(record));record.frame=frame;
  record.count=pulses.values?(pulses.count>MAX_TIMINGS?MAX_TIMINGS:pulses.count):0;
  record.clipped=pulses.clipped || record.count!=pulses.count;
  if(record.count)memcpy(record.pulses,pulses.values,record.count*sizeof(*pulses.values));
  s.next=(s.next+1)%2;if(s.count<2)++s.count;
  // Reception never changes selection, extends authority or issues a command.
}
void Form::turn(int direction,uint64_t now){
  if(!direction || !guard(now))return;touch(now);auto &s=*session;
  if(direction>0 && s.cursor+1<s.total())++s.cursor;
  if(direction<0 && s.cursor)--s.cursor;
}
void Form::activate(uint64_t now){
  if(!guard(now))return;touch(now);auto &s=*session;
  switch(s.page){
  case Session::Page::begin:
    if(!s.cursor){discard();return;}
    if(!backend.hub().subscribe(ReceiveChannel::diagnostics,s,s.subscription)){s.page=Session::Page::failed;s.cursor=0;return;}
    s.page=Session::Page::list;s.cursor=0;break;
  case Session::Page::list:
    if(s.cursor<s.count){s.chosen=s.slot(s.cursor);pause();s.page=Session::Page::details;s.cursor=0;}
    else if(s.cursor==s.count){
      if(s.subscription.generation)pause();
      else if(!backend.hub().subscribe(ReceiveChannel::diagnostics,s,s.subscription)){s.page=Session::Page::failed;s.cursor=0;}
    }else if(s.cursor==s.count+1){s.page=Session::Page::clear;s.cursor=0;}
    else discard();
    break;
  case Session::Page::details:
    if(s.cursor==pulsesAction){s.page=Session::Page::pulses;s.cursor=0;}
    else if(s.cursor==detailRows-1){s.page=Session::Page::list;s.cursor=0;}
    break;
  case Session::Page::pulses:
    if(s.cursor==s.total()-1){s.page=Session::Page::details;s.cursor=pulsesAction;}
    break;
  case Session::Page::clear:
    if(s.cursor){wipe(s.records,sizeof(s.records));s.count=s.next=0;}
    s.page=Session::Page::list;s.cursor=s.count+1;break;
  case Session::Page::failed:discard();break;
  }
}
void Form::back(uint64_t now){
  if(!guard(now))return;touch(now);auto &s=*session;
  if(s.page==Session::Page::pulses){s.page=Session::Page::details;s.cursor=pulsesAction;}
  else if(s.page==Session::Page::details || s.page==Session::Page::clear){s.page=Session::Page::list;s.cursor=0;}
  else discard();
}
void Form::render(FormView &out)const{
  out={};out.notice=notice;if(!session)return;const auto &s=*session;
  out.mode=Mode::fields;out.total=s.total();out.selected=s.cursor;out.offset=s.cursor/3*3;
  const char *title="Receive log",*hint="Pause freezes this log only. Normal reception continues.";
  if(s.page==Session::Page::begin || s.page==Session::Page::clear){
    out.mode=Mode::confirm;out.offset=0;out.rowCount=2;
    strcpy(out.rows[0].label,"Cancel");strcpy(out.rows[1].label,s.page==Session::Page::begin?"Begin observation":"Clear view");
    hint=s.page==Session::Page::begin?"Private frames/timings. No scan, motor or settings action.":"Erases this log only. Configuration/counters unchanged.";
  }else if(s.page==Session::Page::failed){out.mode=Mode::validation;out.rowCount=1;strcpy(out.rows[0].label,"Back to System");hint="Diagnostics observer unavailable. No automatic retry.";}
  else if(s.page==Session::Page::list){
    title=s.subscription.generation?"Receive log: live":"Receive log: paused";
    for(uint8_t i=out.offset;i<out.total && out.rowCount<3;++i){auto &row=out.rows[out.rowCount++];
      if(i<s.count){const auto &frame=s.records[s.slot(i)].frame;
        snprintf(row.label,sizeof(row.label),"#%llu",static_cast<unsigned long long>(frame.sequence));
        snprintf(row.value,sizeof(row.value),"%06lX code %u %s",static_cast<unsigned long>(frame.address),frame.rollingCode,frame.valid()?"Valid":"Invalid");
      }else strcpy(row.label,i==s.count?(s.subscription.generation?"Pause view":"Resume observation"):i==s.count+1?"Clear view":"Back to System");
    }
  }else if(s.page==Session::Page::pulses){
    out.mode=Mode::stackedRows;title="Pulse timings";const auto &record=s.records[s.chosen];
    for(uint8_t i=out.offset;i<out.total && out.rowCount<3;++i){auto &row=out.rows[out.rowCount++];
      if(i==out.total-1){strcpy(row.label,"Back to frame");continue;}
      const unsigned first=i*3,last=first+3<record.count?first+3:record.count;
      snprintf(row.label,sizeof(row.label),"%u-%u (microseconds)",first+1,last);
      size_t used=0;for(unsigned j=first;j<last;++j)used+=snprintf(row.value+used,sizeof(row.value)-used,"%s%lu",j==first?"":" ",static_cast<unsigned long>(record.pulses[j]));
    }
    hint=record.clipped?"Timing list clipped; original data was incomplete.":"Copied raw timings. Back: frame. No transmit action.";
  }else{
    title="Received frame";const auto &record=s.records[s.chosen];const auto &f=record.frame;
    const char *labels[]={"Sequence","Observed (ms)","Remote address","Rolling code","Protocol","Frame bits","Command","Raw command","Encoding key","HW sync","Signal (dBm)","Step size","Valid frame","Frequency scan","Copied timings","View pulse timings","Back to frames"};
    for(uint8_t i=out.offset;i<out.total && out.rowCount<3;++i){auto &row=out.rows[out.rowCount++];strcpy(row.label,labels[i]);
      switch(i){
      case 0:case 1:snprintf(row.value,sizeof(row.value),"%llu",static_cast<unsigned long long>(i?f.observedAt:f.sequence));break;
      case 2:snprintf(row.value,sizeof(row.value),"%06lX",static_cast<unsigned long>(f.address));break;
      case 4:snprintf(row.value,sizeof(row.value),"%s (%u)",f.protocol==0?"RTS":f.protocol==1?"RTW":f.protocol==2?"RTV":"Unknown",f.protocol);break;
      case 6:snprintf(row.value,sizeof(row.value),"0x%02X",f.command);break;
      case 7:case 8:snprintf(row.value,sizeof(row.value),"0x%02X",i==7?f.rawCommand:f.encodingKey);break;
      case 12:case 13:strcpy(row.value,(i==12?f.valid():f.scanning())?"Yes":"No");break;
      case 14:snprintf(row.value,sizeof(row.value),"%u%s",record.count,record.clipped?" (clipped)":"");break;
      case 15:case 16:break;
      default:snprintf(row.value,sizeof(row.value),"%d",i==3?f.rollingCode:i==5?f.bits:i==9?f.hardwareSync:i==10?f.rssi:f.stepSize);break;
      }
    }
    hint="Frozen copy. Rotation never sends or changes counters.";
  }
  snprintf(out.title,sizeof(out.title),"%s",title);snprintf(out.hint,sizeof(out.hint),"%s",hint);
  for(uint8_t i=0;i<out.rowCount;++i)out.rows[i].selected=out.offset+i==s.cursor;
}
} }
