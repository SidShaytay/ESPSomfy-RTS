#include "LocalUpdates.h"
#include <new>
#include <cstdio>
#include <cstring>
namespace LocalUi {namespace Updates {
using namespace EntityAdmin;
namespace {
const char *message(GitUpdateReason reason) {
  switch(reason){
  case GitUpdateReason::none:return "Firmware and Web update together. No automatic checks.";
  case GitUpdateReason::development:return "Development build: stable updates unavailable.";
  case GitUpdateReason::clock:return "Clock is unset. Connect to the network and try again.";
  case GitUpdateReason::certificateTime:return "HTTPS certificate date refused. Check the device clock.";
  case GitUpdateReason::memory:return "Not enough update memory. No automatic retry.";
  case GitUpdateReason::cancelled:return "Update cancelled; controls resume after cleanup.";
  case GitUpdateReason::timeout:return "Update timed out; controls resume after cleanup.";
  case GitUpdateReason::candidateChanged:case GitUpdateReason::invalidCandidate:return "Release changed. Check and review it again.";
  case GitUpdateReason::activationFailed:return "Boot selection uncertain. Controls paused. Recovery required.";
  case GitUpdateReason::writeFailed:return "Firmware write failed. Wait for cleanup before retrying.";
  case GitUpdateReason::admission:return "Controller busy or state changed. No automatic retry.";
  case GitUpdateReason::bootloader:case GitUpdateReason::assets:case GitUpdateReason::layout:return "Installed image is not qualified for GitHub updates.";
  case GitUpdateReason::transport:return "Verified download failed. Check the network and try again.";
  case GitUpdateReason::digest:case GitUpdateReason::invalidImage:return "Release verification failed. Update refused.";
  case GitUpdateReason::noNewerRelease:return "This firmware is current. No newer stable release.";
  default:return "Update unavailable. Review network, clock and compatibility.";
  }
}
const char *progress(GitUpdatePhase phase) {
  switch(phase){
  case GitUpdatePhase::retryWaiting:return "Waiting to retry";
  case GitUpdatePhase::queued:return "Waiting for checks";
  case GitUpdatePhase::checking:return "Checking GitHub";
  case GitUpdatePhase::downloading:return "Downloading update";
  case GitUpdatePhase::verifying:return "Verifying update";
  case GitUpdatePhase::writing:return "Installing firmware";
  case GitUpdatePhase::written:return "Final restart checks";
  case GitUpdatePhase::cleanupPending:return "Cleanup pending";
  default:return "Update in progress";
  }
}
bool checkable(const GitUpdateSnapshot &value,GitUpdateBootStatus boot) {
  return value.supported && value.phase!=GitUpdatePhase::unavailable && !value.busy && !boot.pending;
}
}
struct Form::Session {
  SecuritySnapshot auth;
  uint64_t observed=0,activity=0,candidate=0,operation=0;
  enum class Page:uint8_t {menu,confirm,progress,result}page=Page::menu;
  GitUpdateReason reason=GitUpdateReason::none;
  uint8_t cursor=1;
  bool checking=false,installing=false;
  uint64_t installAccepted=0;
};
void Form::close(Notice reason){
  if(session){
    if(session->operation)backend.cancel(session->operation,session->auth.generation,session->auth.configurationGeneration);
    session->~Session();allocator.deallocate(session);session=nullptr;
  }
  notice=reason;
}
bool Form::open(uint64_t now){
  discard();const auto auth=backend.authorization();
  if(!auth.allowed || !auth.generation || !auth.configurationGeneration){notice=Notice::locked;return false;}
  auto *memory=allocator.allocate(sizeof(Session));if(!memory){notice=Notice::noMemory;return false;}
  session=new(memory)Session{};session->auth=auth;session->observed=session->activity=now;
  const auto value=backend.status();
  if(value.busy && value.installContinuation && !value.manual && value.operation && value.installRemainingMs && value.installRemainingMs<=gitUpdateInstallDurationMs && !backend.boot().pending){
    const uint64_t elapsed=gitUpdateInstallDurationMs-value.installRemainingMs;
    if(now>=elapsed){session->installing=true;session->operation=value.operation;session->candidate=value.candidate.generation;session->installAccepted=now-elapsed;session->page=Session::Page::progress;}
  }
  return true;
}
bool Form::guard(uint64_t now){
  if(!session)return false;
  const auto auth=backend.authorization();Notice reason=Notice::none;
  const auto value=backend.status();
  const bool continuation=session->installing && session->operation && value.operation==session->operation &&
    value.busy && value.installContinuation && now>=session->installAccepted && now-session->installAccepted<gitUpdateInstallDurationMs;
  if(now<session->observed || now<session->activity || (session->installing && (now<session->installAccepted || now-session->installAccepted>=gitUpdateInstallDurationMs)))reason=Notice::expired;
  else if(auth.configurationGeneration!=session->auth.configurationGeneration)reason=Notice::changed;
  else if(!continuation && now-session->activity>=120000)reason=Notice::expired;
  else if(!continuation && !auth.allowed)reason=Notice::locked;
  else if(!continuation && auth.generation!=session->auth.generation)reason=Notice::changed;
  if(reason!=Notice::none){close(reason);return false;}
  session->observed=now;return true;
}
void Form::touch(uint64_t now){session->activity=now;backend.activity();}
void Form::tick(uint64_t now){
  if(!guard(now))return;
  auto &s=*session;const auto value=backend.status();
  if(s.page==Session::Page::confirm && (s.candidate!=value.candidate.generation || !value.canInstall || value.busy || backend.boot().pending)){
    s.candidate=0;s.page=Session::Page::menu;s.cursor=1;notice=Notice::changed;
  }else if(s.page==Session::Page::progress){
    if(s.installing && !backend.authorization().allowed){notice=Notice::locked;return;}
    if(s.operation!=value.operation){s.operation=0;s.page=Session::Page::result;s.reason=GitUpdateReason::invalidCandidate;notice=Notice::changed;}
    else if(!value.busy){
      const bool review=s.checking && value.phase==GitUpdatePhase::available && value.reason==GitUpdateReason::none &&
        value.canInstall && value.candidate.generation>s.operation && value.candidate.generation!=s.candidate &&
        value.candidate.version[0] && !backend.boot().pending;
      s.operation=0;s.checking=false;s.page=review?Session::Page::confirm:Session::Page::menu;
      s.cursor=review?0:1;s.candidate=review?value.candidate.generation:0;s.reason=value.reason;
    }
  }
}
void Form::turn(int direction,uint64_t now){
  if(!direction || !guard(now))return;touch(now);auto &s=*session;
  if(s.page==Session::Page::confirm)s.cursor=direction>0?1:0;
  else if(s.page==Session::Page::menu){
    const auto value=backend.status();s.cursor=direction>0 && value.candidate.version[0] && value.canInstall && !value.busy && !backend.boot().pending?2:1;
  }
}
void Form::activate(uint64_t now){
  if(!guard(now))return;auto &s=*session;
  const auto value=backend.status();const auto boot=backend.boot();
  if(s.page==Session::Page::progress && s.installing && !backend.authorization().allowed){s.operation=0;close(Notice::locked);return;}
  touch(now);
  if(s.page==Session::Page::progress){
    if(s.operation==value.operation && value.cancellable){s.checking=false;backend.cancel(s.operation,s.auth.generation,s.auth.configurationGeneration);}
    return;
  }
  if(s.page==Session::Page::result)return;
  if(s.page==Session::Page::confirm && !s.cursor){s.page=Session::Page::menu;s.cursor=1;s.candidate=0;return;}
  if(s.page==Session::Page::menu && s.cursor==2){
    if(!value.candidate.version[0] || !value.canInstall || value.busy || boot.pending)return;
    s.candidate=value.candidate.generation;s.page=Session::Page::confirm;s.cursor=0;notice=Notice::none;return;
  }
  GitUpdateRequest result;uint64_t operation=0;
  if(s.page==Session::Page::confirm){
    if(s.candidate!=value.candidate.generation || !value.canInstall || value.busy || boot.pending){s.page=Session::Page::menu;s.cursor=1;notice=Notice::changed;return;}
    result=backend.install(s.candidate,s.auth.generation,s.auth.configurationGeneration,operation);
  }else{
    if(!checkable(value,boot)){discard();return;}
    result=backend.check(s.auth.generation,s.auth.configurationGeneration,operation);
  }
  if(result==GitUpdateRequest::accepted && operation){s.checking=s.page!=Session::Page::confirm;s.installing=!s.checking;if(s.installing)s.installAccepted=now;if(s.checking)s.candidate=value.candidate.generation;s.operation=operation;s.page=Session::Page::progress;s.cursor=0;notice=Notice::none;}
  else{s.page=Session::Page::result;s.reason=result==GitUpdateRequest::invalidCandidate?GitUpdateReason::invalidCandidate:GitUpdateReason::admission;}
}
void Form::back(uint64_t now){
  if(!guard(now))return;touch(now);
  if(session->installing && !backend.authorization().allowed){session->operation=0;close(Notice::locked);return;}
  if(session->page==Session::Page::confirm){session->page=Session::Page::menu;session->cursor=1;session->candidate=0;}
  else discard();
}
void Form::render(FormView &out)const{
  out={};out.notice=notice;
  if(!session) {
    if(notice==Notice::none)return;
    const auto value=backend.status();out.mode=Mode::updateStatus;
    strcpy(out.title,"System updates");out.rowCount=out.total=3;out.selected=2;
    strcpy(out.rows[0].label,notice==Notice::expired?"Session expired":notice==Notice::locked?"Unlock required":"Session ended");
    strcpy(out.rows[1].label,value.busy?"Update still active":"No active update");
    strcpy(out.rows[2].label,"Back: System");out.rows[2].selected=true;
    snprintf(out.hint,sizeof(out.hint),"%s",message(value.reason));return;
  }
  const auto &s=*session;const auto value=backend.status();const auto boot=backend.boot();
  out.mode=Mode::updateStatus;const char *title="System updates",*hint=message(value.reason);
  if(s.page==Session::Page::confirm){
    out.mode=Mode::updateReview;title="Install update?";hint="Firmware + Web; saves current state, then restarts.";
    out.rowCount=3;out.total=2;out.selected=s.cursor;
    strcpy(out.rows[0].label,"Cancel");strcpy(out.rows[1].label,"Install and restart");
    strcpy(out.rows[2].label,"GitHub release");
    if(value.candidate.generation==s.candidate)snprintf(out.rows[2].value,sizeof(out.rows[2].value),"%s",value.candidate.version);
  }else if(s.page==Session::Page::progress){
    title=progress(value.phase);hint=s.installing && !backend.authorization().allowed?"Install continues; Back to unlock menus.":value.phase==GitUpdatePhase::cleanupPending?"Controls stay paused until cleanup completes.":value.cancellable?"Keep power connected. Back cancels this update.":"Keep power connected. Back leaves this screen.";
    out.rowCount=out.total=2;
    if(value.phase==GitUpdatePhase::retryWaiting)snprintf(out.rows[0].label,sizeof(out.rows[0].label),"Retry %u in %lus",unsigned(value.retryCount),static_cast<unsigned long>((value.retryDelayMs+999)/1000));
    else if(s.checking)strcpy(out.rows[0].label,"Checking GitHub...");
    else snprintf(out.rows[0].label,sizeof(out.rows[0].label),"Progress: %u%%",unsigned(value.progress));
    strcpy(out.rows[1].label,s.installing && !backend.authorization().allowed?"Back to unlock menus":value.cancellable?"Cancel update":"Final checks; please wait");out.selected=1;
    if(value.phase==GitUpdatePhase::error){title="Update needs recovery";hint=message(value.reason);strcpy(out.rows[0].label,"Controls paused");strcpy(out.rows[1].label,"Back: System");}
  }else if(s.page==Session::Page::result){
    title="Update refused";hint=message(s.reason);out.rowCount=out.total=1;strcpy(out.rows[0].label,"Back: System");
  }else{
    out.rowCount=out.total=value.candidate.version[0]?3:2;out.selected=s.cursor;
    strcpy(out.rows[0].label,"Installed");snprintf(out.rows[0].value,sizeof(out.rows[0].value),"%s",value.installed);
    strcpy(out.rows[1].label,checkable(value,boot)?"Check GitHub release":"Back: System");
    strcpy(out.rows[2].label,value.canInstall?"Review release":"Latest release");
    snprintf(out.rows[2].value,sizeof(out.rows[2].value),"%s",value.candidate.version);
    if(boot.pending){title=boot.rollbackRequired?"Automatic rollback":"Verifying new firmware";hint=boot.rollbackRequired?bootRecoveryText(boot.recovery):"Waiting for continuous healthy operation.";}
    else if(notice==Notice::changed)hint="Release changed. Check and review it again.";
  }
  for(uint8_t i=0;i<out.rowCount;++i)out.rows[i].selected=i==out.selected;
  snprintf(out.title,sizeof(out.title),"%s",title);snprintf(out.hint,sizeof(out.hint),"%s",hint);
}
}}
