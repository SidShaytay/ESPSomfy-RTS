#include "LocalSystem.h"
#include <stdio.h>
#include <string.h>
namespace LocalUi { namespace System {
namespace {
constexpr const char *files[]={"Backup","Restore","Manual firmware","Refresh browser cache","Copy receive log"};
constexpr const char *guidance[]={
  "Browser: System > Firmware > Backup. Keep the file private.",
  "Browser: System > Firmware > Restore. Review sections first.",
  "Browser: System > Firmware > Update Firmware. Web UI included.",
  "Browser: System > Firmware > Refresh Cache. No device reset.",
  "Browser: Radio > Log. Use Copy; keep frame data private."};
const char *refusal(RestartReason value) {
  switch(value) {
  case RestartReason::none:return "Pending final safety checks; restart can still be refused.";
  case RestartReason::settingsUncertain:return "Settings storage uncertain. Save current settings first.";
  case RestartReason::modelFailed:return "Model file failed. Save current model, then request Restart.";
  case RestartReason::modelUncertain:return "Model file uncertain. Save current model before Restart.";
  case RestartReason::queuedOperations:return "Operations queued or active. Finish them, then Restart.";
  case RestartReason::pending:return "Another restart is pending. No new restart was requested.";
  case RestartReason::stale:return "State changed. Review it, then explicitly request Restart.";
  case RestartReason::cancelled:return "Restart cancelled. No automatic retry.";
  case RestartReason::exhausted:return "Restart history exhausted. Restart refused.";
  case RestartReason::clock:return "Restart clock moved backward. Restart refused.";
  case RestartReason::invalidRequest:return "Invalid restart request. Restart refused.";
  case RestartReason::notReady:return "Restart admission not ready. No automatic retry.";
  case RestartReason::persistenceBusy:return "Storage or update active. Finish it before Restart.";
  case RestartReason::counterUncertain:return "Counter storage uncertain. Resolve counter failure first.";
  case RestartReason::activeMovement:return "Movement or GPIO pulse active. Wait before Restart.";
  case RestartReason::modelUnavailable:return "Model not loaded. Use a verified full model restore.";
  case RestartReason::settingsUnavailable:return "Settings need repair. Review affected section.";
  case RestartReason::securityUnavailable:return "Security not loaded. Authentication is unavailable.";
  case RestartReason::unknownGroup:return "Group motion unknown. Review missing or untracked members.";
  case RestartReason::activationFailed:return "Update activation unverified. Controls paused; recovery required.";
  }
  return "Restart refused. Review device state before trying again.";
}
}
void Form::discard() {
  page=Page::closed;generation=configurationGeneration=0;
  observedAt=activityAt=restartGeneration=0;cursor=file=0;
  reason=RestartReason::none;notice=EntityAdmin::Notice::none;admitted=stopped=statusUnknown=false;
}
bool Form::open(uint64_t now,Menu requested,uint8_t initial) {
  discard();const auto auth=backend.authorization();
  if(!auth.allowed || !auth.generation || !auth.configurationGeneration) {
    notice=EntityAdmin::Notice::locked;return false;
  }
  generation=auth.generation;configurationGeneration=auth.configurationGeneration;
  observedAt=activityAt=now;menu=requested;cursor=initial<=(menu==Menu::maintenance?2:menu==Menu::diagnostics?3:menu==Menu::legacy?6:0)?initial:0;page=menu==Menu::wifiScan?Page::wifi:Page::menu;return true;
}
bool Form::guard(uint64_t now) {
  if(closed())return false;
  EntityAdmin::Notice failure=EntityAdmin::Notice::none;
  const auto auth=backend.authorization();
  if(now<observedAt || now-activityAt>=120000)failure=EntityAdmin::Notice::expired;
  else if(!auth.allowed)failure=EntityAdmin::Notice::locked;
  else if(auth.generation!=generation || auth.configurationGeneration!=configurationGeneration)
    failure=EntityAdmin::Notice::changed;
  if(failure!=EntityAdmin::Notice::none){discard();notice=failure;return false;}
  observedAt=now;return true;
}
void Form::touch(uint64_t now){activityAt=now;backend.activity();}
void Form::tick(uint64_t now) {
  if(!guard(now))return;
  if(page==Page::result && admitted && !statusUnknown) {
    const auto current=backend.status();
    if(current.generation!=restartGeneration) {stopped=true;reason=RestartReason::stale;}
    else if(!current.pending()) {stopped=true;reason=current.reason;}
  }
}
void Form::turn(int direction,uint64_t now) {
  if(!direction || !guard(now))return;
  touch(now);
  const uint8_t last=page==Page::menu?(menu==Menu::maintenance?2:menu==Menu::diagnostics?3:6):page==Page::network?networkDetailRows-1:page==Page::confirm?1:page==Page::files?(menu==Menu::maintenance?3:4):0;
  if(direction>0 && cursor<last)++cursor;
  if(direction<0 && cursor)--cursor;
}
void Form::activate(uint64_t now) {
  if(!guard(now))return;
  touch(now);
  switch(page) {
  case Page::menu:
    if(menu==Menu::maintenance)page=cursor==0?Page::updates:cursor==1?Page::confirm:Page::files;
    else if(menu==Menu::diagnostics){if(cursor==3){file=4;page=Page::guidance;}else page=cursor==0?Page::network:cursor==1?Page::diagnostics:Page::radio;}
    else page=cursor==6?Page::updates:cursor==5?Page::radio:cursor==4?Page::network:cursor==3?Page::diagnostics:cursor==2?Page::wifi:cursor?Page::files:Page::confirm;cursor=0;break;
  case Page::confirm:
    if(!cursor){page=Page::menu;cursor=menu==Menu::maintenance?1:0;break;}
    {const auto result=backend.restart(generation,configurationGeneration);admitted=result.admitted;reason=result.reason;
      restartGeneration=0;stopped=statusUnknown=false;
      if(admitted) {
        // Admission and this synchronized snapshot are separate operations:
        // cancellation may already have won. Bind the entire observed result.
        const auto current=backend.status();restartGeneration=current.generation;
        if(!restartGeneration)statusUnknown=true;
        else if(!current.pending()) {stopped=true;reason=current.reason;}
      }
      page=Page::result;cursor=0;}
    break;
  case Page::network:if(cursor==networkDetailRows-1){page=Page::menu;cursor=menu==Menu::diagnostics?0:4;}break;
  case Page::files:file=cursor;page=Page::guidance;cursor=0;break;
  case Page::guidance:page=menu==Menu::diagnostics?Page::menu:Page::files;cursor=menu==Menu::diagnostics?3:file;break;
  case Page::result:break; // A completion is never a second restart/retry action.
  case Page::wifi:case Page::radio:
  case Page::diagnostics:case Page::updates:break;
  case Page::closed:break;
  }
}
void Form::back(uint64_t now) {
  if(!guard(now))return;
  touch(now);
  if(page==Page::menu)discard();
  else if(page==Page::guidance){page=menu==Menu::diagnostics?Page::menu:Page::files;cursor=menu==Menu::diagnostics?3:file;}
  else if(page==Page::network){page=Page::menu;cursor=menu==Menu::diagnostics?0:4;}
  else {const auto old=page;page=Page::menu;cursor=menu==Menu::maintenance?(old==Page::files?2:old==Page::confirm || old==Page::result?1:0):0;}
}
void Form::render(EntityAdmin::FormView &out) const {
  out={};out.notice=notice;
  if(closed())return;
  out.mode=EntityAdmin::Mode::fields;
  const char *title="System",*hint=menu==Menu::legacy?"Press: choose   Back: Home":"Press: choose   Back: This controller";
  if(page==Page::menu) {
    const char *labels[]={"Restart","Browser files & Web","WiFi scan","Receive diagnostics","Network details","Radio frequency scan","GitHub updates"};
    const char *maintenance[]={"GitHub updates","Restart","Browser files & Web"};
    const char *diagnostics[]={"Network details","Receive diagnostics","Radio frequency scan","Copy receive log"};
    title=menu==Menu::maintenance?"Maintenance":menu==Menu::diagnostics?"Diagnostics":"System";
    out.total=menu==Menu::maintenance?3:menu==Menu::diagnostics?4:7;out.offset=cursor/3*3;
    for(uint8_t i=out.offset;i<out.total && out.rowCount<3;++i)strcpy(out.rows[out.rowCount++].label,menu==Menu::maintenance?maintenance[i]:menu==Menu::diagnostics?diagnostics[i]:labels[i]);
  } else if(page==Page::network){
    title="Network details";out.mode=EntityAdmin::Mode::stackedRows;out.total=networkDetailRows;out.offset=cursor/3*3;
    hint="Read only; current connection, no probe or reconnect.";
    for(uint8_t i=out.offset;i<out.total&&out.rowCount<3;++i){auto &row=out.rows[out.rowCount++];
      if(!backend.networkRow(i,row)){strcpy(row.label,"Network status unavailable");row.value[0]=0;}
    }
  } else if(page==Page::files) {
    title="Browser files";out.total=menu==Menu::maintenance?4:5;out.offset=cursor/3*3;
    for(uint8_t i=out.offset;i<out.total && out.rowCount<3;++i)strcpy(out.rows[out.rowCount++].label,files[i]);
  } else {
    out.mode=EntityAdmin::Mode::validation;
    if(page==Page::confirm) {
      title="Restart controller?";hint="Interrupts service. Finish all movement and operations first.";
      out.total=out.rowCount=2;strcpy(out.rows[0].label,"Cancel");strcpy(out.rows[1].label,"Restart now");
    } else if(page==Page::guidance) {
      title=files[file];hint=guidance[file];out.total=out.rowCount=1;strcpy(out.rows[0].label,menu==Menu::diagnostics?"Back: Diagnostics":"Back to files");
    } else {
      title=statusUnknown?"Restart status unknown":admitted?(stopped?"Restart stopped":"Restart scheduled"):"Restart refused";
      hint=statusUnknown?"Restart status unavailable. No automatic retry.":refusal(reason);out.total=out.rowCount=1;strcpy(out.rows[0].label,"Back: System");
    }
  }
  snprintf(out.title,sizeof(out.title),"%s",title);snprintf(out.hint,sizeof(out.hint),"%s",hint);
  out.selected=cursor;
  for(uint8_t i=0;i<out.rowCount;++i)out.rows[i].selected=out.offset+i==cursor;
}
} }
