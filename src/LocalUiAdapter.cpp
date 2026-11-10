#include "SafeRestart.h"
#ifdef ESPSOMFY_LOCAL_UI
#include "LocalUiAdapter.h"
#include "LocalUiAuthorizationAdapter.h"
#include "LocalStopLease.h"
#include "LocalTargetRead.h"
#include "Somfy.h"
#include "ShadeRequests.h"
#include "LoopQueue.h"
#include "Utils.h"
#include <esp_timer.h>
#include <Preferences.h>
#include <math.h>
#include <string.h>
extern SomfyShadeController somfy;
extern ShadeRequests shadeRequests;
extern LoopQueue loopQueue;

namespace LocalUi {
namespace {
TargetContext focusedContext;
uint64_t focusedToken=0, tokenSerial=0, presentedToken=0;
EntityRef presentedRef;
bool hasFocus=false;
StopLease stopLease;
uint32_t focusedMoving=0, leasedMoving=0;
CataloguePage previousPage;
CatalogueKind previousKind=CatalogueKind::devices;
uint8_t previousRoom=allRooms;
uint64_t previousModelRevision=0, previousRestore=0, pageToken=0;
bool pageCached=false, controlsFrozen=false;
struct PendingControl {
  TargetContext guard;
  Command command;
  uint64_t token=0, admittedAt=0, observedAt=0;
  uint32_t eligible=0, securityGeneration=0;
  bool active=false, executing=false, cancelled=false;
} pendingControl;
static_assert(sizeof(PendingControl)<=704,"One local intent owns one bounded exact context");
uint64_t intentSerial=0;
bool resultReady=false;
DispatchResult lastResult=DispatchResult::cancelled;
uint64_t controlNow() {return uint64_t(esp_timer_get_time())/1000;}
void finishControl(DispatchResult result) {
  pendingControl={};lastResult=result;resultReady=true;
}

uint64_t nextToken() { return tokenSerial==UINT64_MAX?0:++tokenSerial; }
bool rf(radio_proto proto) {
  return proto==radio_proto::RTS || proto==radio_proto::RTW || proto==radio_proto::RTV;
}
uint8_t effectiveBits(uint8_t bits) { return bits?bits:somfy.transceiver.config.type; }
int8_t position(float value,bool flip) {
  if(!isfinite(value) || value<0 || value>100) return -1;
  return int8_t(flip?100-value:value);
}
void label(char (&out)[21],const char *input) {
  if(!memchr(input,0,21)) { strcpy(out,"Invalid name"); return; }
  memcpy(out,input,21);
}
ControlDescriptor descriptor(SomfyShade &shade) {
  ControlDescriptor d;
  d.shadeType=shade.shadeType; d.tiltType=shade.tiltType; d.proto=shade.proto;
  d.bitLength=effectiveBits(shade.bitLength); d.paired=shade.paired; d.sunSensor=shade.sunSensor;
  d.moving=!shade.isIdle(); d.tiltMoving=shade.state().tiltDirection!=0;
  return d;
}
ControlMask capabilities(const ControlDescriptor &d) {
  ControlMask mask=controlCapabilities(d);
  // Separate/Euro relay tilt has no independent actuator mapping. Keep ordinary
  // lift and existing integrated/tilt-only paths; never invent a pin remap.
  if(d.proto==radio_proto::GP_Relay &&
     (d.tiltType==tilt_types::tiltmotor || d.tiltType==tilt_types::euromode))
    mask &= ~(controlActionBit(Action::tiltUp)|controlActionBit(Action::tiltStop)|
              controlActionBit(Action::tiltDown)|controlActionBit(Action::tiltPosition));
  return mask;
}
ControlMask gpioCapabilities(SomfyShade &shade,ControlDescriptor d,ControlMask mask) {
  if(rf(d.proto))return mask;
  // Construct the same typed requests as dispatch; idle STOP remains visible.
  d.moving=d.tiltMoving=true;
  for(uint8_t n=0;n<uint8_t(Action::sunOff)+1;n++) {
    const auto action=Action(n); const auto bit=controlActionBit(action);
    if(!(mask&bit))continue;
    shade_request_t request;
    const bool target=action==Action::position||action==Action::tiltPosition;
    if(prepareControlRequest(action,d,1,target?50:-1,0,request)!=ControlResult::done) {mask&=~bit;continue;}
    bool allowed;
    if(request.type==shade_request_type_t::tilt_command)
      allowed=request.target>=0?shade.canGpioSeek(true):shade.canGpioTilt(request.command);
    else if(request.target>=0)allowed=shade.canGpioSeek(shade.tiltType==tilt_types::tiltonly);
    else {
      allowed=shade.canGpioCommand(request.command);
      if(action==Action::my && (shade.simMy || shade.proto==radio_proto::GP_Relay) && shade.isIdle())
        allowed=allowed&&shade.canGpioSeek(shade.tiltType!=tilt_types::none);
    }
    if(!allowed)mask&=~bit;
  }
  return mask;
}
bool readTarget(EntityRef ref,Target &target,TargetContext &context,uint32_t &moving) {
  target={}; context={}; moving=0; target.ref=ref;
  RawTargetContext raw;
  if(!readRawTargetContext(somfy,ref,raw) || !raw.targetExists) return false;
  if(ref.kind==EntityKind::device) {
    auto *shade=somfy.getShadeById(ref.id); if(!shade) return false;
    target.exists=true; label(target.name,shade->name);
    target.lightUnavailable=shade->light; // Configured lamp capability, never observed lamp state.
    target.identity=raw.identity;
    target.revision=raw.revision;
    if(!makeControlTargetContext(raw,context)) return false;
    target.descriptor=descriptor(*shade);
    target.capabilities=gpioCapabilities(*shade,target.descriptor,capabilities(target.descriptor));
    target.valid=target.capabilities!=0;
    target.position=position(shade->state().currentPos,shade->flipPosition);
    target.tiltPosition=position(shade->state().currentTiltPos,shade->flipPosition);
    target.moving=target.descriptor.moving; target.tiltMoving=target.descriptor.tiltMoving;
    target.movingCount=target.moving?1:0; moving=target.moving?1:0;
    target.pendingRelayTilt=shade->proto==radio_proto::GP_Relay &&
      (shade->tiltType==tilt_types::tiltmotor || shade->tiltType==tilt_types::euromode);
  } else {
    auto *group=somfy.getGroupById(ref.id); if(!group) return false;
    target.exists=true; label(target.name,group->name);
    target.identity=raw.identity;
    target.revision=raw.revision;
    target.descriptor.group=true; target.descriptor.proto=group->proto;
    target.descriptor.bitLength=effectiveBits(group->bitLength);
    if(!makeControlTargetContext(raw,context)) return false;
    ControlMask common=allControlActions;
    bool compatible=rf(group->proto) && context.identity!=0;
    const uint8_t count=context.memberCount;
    for(uint8_t i=0;i<count;i++) {
      auto *shade=somfy.getShadeById(context.members[i].id); if(!shade) return false;
      const auto member=descriptor(*shade);
      compatible &= member.paired && member.proto==group->proto &&
        member.bitLength==target.descriptor.bitLength && context.members[i].identity!=0;
      common &= capabilities(member);
      target.descriptor.sunSensor |= member.sunSensor;
      if(member.moving) { moving|=uint32_t(1)<<i; ++target.movingCount; }
      target.tiltMoving |= member.tiltMoving;
    }
    target.descriptor.memberCount=compatible?count:0;
    target.descriptor.commonActions=common;
    target.descriptor.moving=target.moving=moving!=0;
    target.descriptor.tiltMoving=target.tiltMoving;
    target.capabilities=capabilities(target.descriptor);
    target.valid=target.capabilities!=0;
  }
  if(rf(target.descriptor.proto) && !context.identity) { target.valid=false; target.capabilities=0; }
  return context.valid();
}
class RuntimeCatalogue : public CatalogueSource {
public:
  bool read(CatalogueKind kind,uint8_t slot,CatalogueEntry &out) override {
    out={};
    if(kind==CatalogueKind::rooms) {
      const auto &room=somfy.rooms[slot]; if(!room.roomId) return false;
      out.id=room.roomId; out.sortOrder=room.sortOrder; memcpy(out.name,room.name,21); return true;
    }
    if(kind==CatalogueKind::devices) {
      auto &shade=somfy.shades[slot]; const auto id=shade.getShadeId(); if(!id || id==255) return false;
      out.id=id; out.roomId=shade.roomId; out.sortOrder=shade.sortOrder; memcpy(out.name,shade.name,21);
      out.identity=shade.getRemoteAddress(); out.revision=shade.configurationRevision;
      const auto d=descriptor(shade); out.available=gpioCapabilities(shade,d,capabilities(d))!=0;
      out.position=position(shade.state().currentPos,shade.flipPosition); out.moving=d.moving; return true;
    }
    auto &group=somfy.groups[slot]; const auto id=group.getGroupId(); if(!id || id==255) return false;
    out.id=id; out.roomId=group.roomId; out.sortOrder=group.sortOrder; memcpy(out.name,group.name,21);
    out.identity=group.getRemoteAddress(); out.revision=group.configurationRevision;
    out.available=rf(group.proto) && out.identity!=0; out.moving=group.direction!=0; return true;
  }
};
bool samePage(const CataloguePage &a,const CataloguePage &b) {
  if(a.offset!=b.offset || a.cursor!=b.cursor || a.count!=b.count || a.total!=b.total) return false;
  for(uint8_t i=0;i<a.count;i++) {
    const auto &x=a.rows[i],&y=b.rows[i];
    if(x.id!=y.id || x.roomId!=y.roomId || x.sortOrder!=y.sortOrder || x.identity!=y.identity ||
       x.revision!=y.revision || memcmp(x.name,y.name,21)!=0) return false;
  }
  return true;
}
void observeLease(const View &view,const SecuritySnapshot &security) {
  const bool selected=hasFocus && view.entity==focusedContext.ref && view.openedToken==focusedToken;
  const bool authorized=selected && view.authorizedSelection && security.allowed &&
    view.selectionAuthGeneration==security.generation && presentedToken==focusedToken &&
    presentedRef==view.entity;
  const auto context=focusedContext.stopContext(focusedToken,security.configurationGeneration,
    security.allowed?security.generation:security.expiredSessionGeneration);
  stopLease.observe(context,focusedMoving,authorized,selected && security.stopGrace);
  leasedMoving=stopLease.allowed(context);
}
// Return queued only while every admission property still permits execution.
DispatchResult validateControl(Target &target,uint32_t &moving) {
  if(!pendingControl.active || pendingControl.cancelled) return DispatchResult::cancelled;
  const uint64_t now=controlNow();
  if(now<pendingControl.admittedAt || now<pendingControl.observedAt ||
     now-pendingControl.admittedAt>=controlQueueMaxAgeMs) return DispatchResult::expired;
  pendingControl.observedAt=now;
  if(controlsFrozen || restartPending()) return DispatchResult::refused;
  TargetContext current;
  if(!readTarget(pendingControl.command.ref,target,current,moving) || !current.same(pendingControl.guard))
    return DispatchResult::stale;
  // Any matching target read is authoritative idle evidence, including a
  // queued non-STOP action. Do not let a later UI sample revive an old run.
  stopLease.retainMoving(pendingControl.guard.stopContext(pendingControl.command.token,
    pendingControl.securityGeneration,pendingControl.command.authGeneration),moving);
  const auto security=pollAuthorization();
  const auto &cmd=pendingControl.command;
  const bool session=security.allowed && cmd.authGeneration && cmd.authGeneration==security.generation;
  const bool grace=!security.allowed && cmd.action==Action::stop && security.stopGrace && cmd.authGeneration &&
    cmd.authGeneration==security.expiredSessionGeneration;
  if(security.configurationGeneration!=pendingControl.securityGeneration || (!session && !grace)) return DispatchResult::denied;
  if(!(target.capabilities&controlActionBit(cmd.action))) return DispatchResult::refused;
  if(rf(target.descriptor.proto) && (!somfy.transceiver.available() || somfy.transceiver.txSuspended())) return DispatchResult::refused;
  if(cmd.action==Action::stop && cmd.repeat!=-1) return DispatchResult::refused;
  if(cmd.action==Action::stop) {
    pendingControl.eligible &= moving;
    if(!pendingControl.eligible) return DispatchResult::idle;
  } else {
    shade_request_t request;
    const auto prepared=prepareControlRequest(cmd.action,target.descriptor,cmd.ref.id,cmd.value,cmd.step,request,cmd.repeat);
    if(prepared!=ControlResult::done) return prepared==ControlResult::idle?DispatchResult::idle:DispatchResult::refused;
  }
  return DispatchResult::queued;
}
} // namespace
bool localControlsFrozen() { return controlsFrozen; }
CatalogueSource &runtimeCatalogue() {static RuntimeCatalogue source;return source;}
void invalidateControlPresentation() {
  presentedRef={};presentedToken=0;
  focusedContext={};focusedToken=0;hasFocus=false;
  focusedMoving=leasedMoving=0;stopLease={};
  // tokenSerial and intentSerial remain monotonic across graphics recovery.
}
void setLocalControlsFrozen(bool value) {
  if(value && !controlsFrozen) {
    controlsFrozen=true;lockAuthorization();cancelUnsubmittedLocalControl();
  } else controlsFrozen=value;
}
bool localControlPending() {return pendingControl.active;}
bool localControlTokenActive(uint64_t token) {return token && pendingControl.active && !pendingControl.executing && !pendingControl.cancelled && token==pendingControl.token;}
bool takeLocalControlResult(DispatchResult &result) {
  if(!resultReady) return false;
  result=lastResult;resultReady=false;return true;
}
bool cancelUnsubmittedLocalControl() {
  if(!pendingControl.active) return false;
  if(pendingControl.executing) pendingControl.cancelled=true;
  else finishControl(DispatchResult::cancelled);
  return true;
}
void refreshLocalControl() {
  if(!pendingControl.active) return;
  Target target;uint32_t moving=0;
  const auto result=validateControl(target,moving);
  if(result!=DispatchResult::queued && !pendingControl.executing) finishControl(result);
}
DisplayPreferences loadPreferences() {
  DisplayPreferences out; Preferences prefs;
  if(!prefs.begin("LCDUI",true)) return out;
  out.brightness=prefs.getUChar("brightness",32); out.dimSeconds=prefs.getUShort("dimSeconds",10);
  out.offSeconds=prefs.getUShort("offSeconds",20); prefs.end(); return out;
}
bool savePreferences(const DisplayPreferences &value) {
  Preferences prefs; if(!prefs.begin("LCDUI")) return false;
  const bool ok=prefs.putUChar("brightness",value.brightness)==1 &&
    prefs.putUShort("dimSeconds",value.dimSeconds)==2 && prefs.putUShort("offSeconds",value.offSeconds)==2;
  prefs.end(); return ok;
}
Model snapshotModel(bool running,const View &view) {
  Model m;
  if(!running) return m;
#ifdef ESPSOMFY_LOCAL_COMMANDS
  m.controlsEnabled=true;
#endif
  refreshLocalControl();m.controlPending=localControlPending();
  const auto security=pollAuthorization();
  m.authMode=security.mode; m.authorized=security.allowed; m.authGeneration=security.generation;
  m.authConfigurationGeneration=security.configurationGeneration; m.retryAfterMs=security.retryAfterMs;
  m.expiredSessionGeneration=security.expiredSessionGeneration;
  m.modelRevision=somfy.configurationRevision; m.restoreRevision=somfy.restoreRevision;
  m.catalogue=view.catalogue; m.room=view.room;
  for(auto &shade:somfy.shades) if(shade.getShadeId()!=255 && !shade.isIdle()) m.anyMoving=true;
  RuntimeCatalogue source;
  if(view.catalogue==CatalogueKind::rooms) m.pageValid=roomCataloguePage(source,view.cursor,m.page,view.anchorId,view.anchored);
  else if(view.room==0 || view.room==allRooms || somfy.getRoomById(view.room))
    m.pageValid=cataloguePage(source,view.catalogue,view.cursor,view.room,m.page,view.anchorId,view.anchored);
  if(m.pageValid && (!pageCached || previousKind!=m.catalogue || previousRoom!=m.room ||
      previousModelRevision!=m.modelRevision || previousRestore!=m.restoreRevision || !samePage(previousPage,m.page))) {
    previousPage=m.page; previousKind=m.catalogue; previousRoom=m.room;
    previousModelRevision=m.modelRevision; previousRestore=m.restoreRevision;
    pageToken=nextToken(); pageCached=true;
  }
  m.pageToken=m.pageValid?pageToken:0;
  TargetContext next;
  uint32_t moving=0;
  const bool valid=readTarget(view.entity,m.target,next,moving);
  if(valid && (!hasFocus || !focusedContext.same(next))) {
    focusedContext=next; focusedToken=nextToken(); presentedToken=0;
  }
  hasFocus=valid && focusedToken!=0;
  if(!hasFocus) { focusedToken=presentedToken=0; }
  focusedMoving=moving; m.target.token=hasFocus?focusedToken:0;
  observeLease(view,security);
  m.target.stopLease=leasedMoving!=0 && m.target.valid;
  m.ready=!controlsFrozen && !restartPending() && (!rf(m.target.descriptor.proto) ||
    (somfy.transceiver.available() && !somfy.transceiver.txSuspended()));
  return m;
}
void markControlPresented(EntityRef ref,uint64_t token,bool authorizedSelection,uint32_t sessionGeneration) {
  if(!hasFocus || ref!=focusedContext.ref || !token || token!=focusedToken) return;
  Target target; TargetContext current; uint32_t moving=0;
  if(!readTarget(ref,target,current,moving) || !focusedContext.same(current)) return;
  presentedRef=ref; presentedToken=token; focusedMoving=moving;
  View view; view.entity=ref; view.openedToken=token;
  view.authorizedSelection=authorizedSelection; view.selectionAuthGeneration=sessionGeneration;
  observeLease(view,pollAuthorization());
}
DispatchResult dispatch(const Command &cmd) {
#ifndef ESPSOMFY_LOCAL_COMMANDS
  (void)cmd; return DispatchResult::refused;
#else
  refreshLocalControl();resultReady=false; // new explicit input supersedes an old completion
  if(controlsFrozen || restartPending()) return DispatchResult::refused;
  if(!hasFocus || !cmd.ref.valid() || cmd.ref!=focusedContext.ref || !cmd.token || cmd.token!=focusedToken ||
     presentedRef!=cmd.ref || presentedToken!=cmd.token) return DispatchResult::stale;
  Target target; TargetContext current; uint32_t moving=0;
  if(!readTarget(cmd.ref,target,current,moving) || !current.same(focusedContext)) return DispatchResult::stale;
  const auto security=pollAuthorization();
  const bool session=security.allowed && cmd.authGeneration && cmd.authGeneration==security.generation;
  View view; view.entity=cmd.ref; view.openedToken=cmd.token;
  view.authorizedSelection=session; view.selectionAuthGeneration=cmd.authGeneration;
  focusedMoving=moving; observeLease(view,security);
  if(!session && !(cmd.action==Action::stop && leasedMoving && cmd.authGeneration &&
     cmd.authGeneration==security.expiredSessionGeneration)) return DispatchResult::denied;
  if(rf(target.descriptor.proto) && (!somfy.transceiver.available() || somfy.transceiver.txSuspended()))
    return DispatchResult::refused;
  if(!(target.capabilities & controlActionBit(cmd.action))) return DispatchResult::refused;
  uint32_t eligible=0;
  if(cmd.action==Action::stop && cmd.repeat!=-1) return DispatchResult::refused;
  if(cmd.action==Action::stop) {
    eligible=session?moving:moving&leasedMoving;
  } else {
    shade_request_t request;
    const auto prepared=prepareControlRequest(cmd.action,target.descriptor,cmd.ref.id,cmd.value,cmd.step,request,cmd.repeat);
    if(prepared!=ControlResult::done) return prepared==ControlResult::idle?DispatchResult::idle:DispatchResult::refused;
  }
  bool superseded=false;
  if(pendingControl.active) {
    // A fresh, presented STOP retracts only this unchanged target's local
    // unsubmitted start. The replacement always joins the GLOBAL FIFO tail.
    if(cmd.action!=Action::stop || pendingControl.command.action==Action::stop || pendingControl.executing ||
       !pendingControl.guard.same(current)) return DispatchResult::busy;
    finishControl(DispatchResult::cancelled);resultReady=false;superseded=true;
  }
  if(cmd.action==Action::stop && !eligible) return superseded?DispatchResult::cancelled:DispatchResult::idle;
  if(intentSerial==UINT64_MAX) return superseded?DispatchResult::cancelledBusy:DispatchResult::refused;
  const uint64_t now=controlNow(), token=++intentSerial;
  pendingControl.guard=current;pendingControl.command=cmd;
  pendingControl.token=token;pendingControl.admittedAt=pendingControl.observedAt=now;
  pendingControl.eligible=eligible;pendingControl.securityGeneration=security.configurationGeneration;
  pendingControl.active=true;
  loop_item_t item;item.type=loop_item_type_t::local_control;item.localToken=token;item.localRadio=rf(target.descriptor.proto);
  if(!loopQueue.submit(item)) {pendingControl={};return superseded?DispatchResult::cancelledBusy:DispatchResult::busy;}
  return DispatchResult::queued;
#endif
}
void runLocalControl(uint64_t token) {
  if(!localControlTokenActive(token)) return;
  Target target;uint32_t moving=0;
  const auto initial=validateControl(target,moving);
  if(initial!=DispatchResult::queued) {finishControl(initial);return;}
  pendingControl.executing=true;
  const auto cmd=pendingControl.command;
  shade_request_t request;
  if(cmd.action!=Action::stop) {
    if(prepareControlRequest(cmd.action,target.descriptor,cmd.ref.id,cmd.value,cmd.step,request,cmd.repeat)!=ControlResult::done)
      {finishControl(DispatchResult::refused);return;}
    const auto result=shadeRequests.run(request);
    finishControl(result==shade_request_result_t::done?DispatchResult::done:DispatchResult::refused);return;
  }
  const uint8_t members=pendingControl.guard.memberCount;
  const uint32_t all=members==32?UINT32_MAX:(uint32_t(1)<<members)-1;
  if(cmd.ref.kind==EntityKind::group && moving==all && pendingControl.eligible==all) {
    request.type=shade_request_type_t::group_command;request.id=cmd.ref.id;request.command=somfy_commands::My;
    finishControl(shadeRequests.run(request)==shade_request_result_t::done?DispatchResult::done:DispatchResult::refused);return;
  }
  bool sent=false;uint32_t remaining=pendingControl.eligible;
  for(uint8_t i=0;i<members;i++) {
    const uint32_t bit=uint32_t(1)<<i;
    if(!(remaining&bit)) continue;
    const auto checked=validateControl(target,moving); // cap/age/context fresh for EACH member
    if(checked!=DispatchResult::queued) {
      finishControl(sent?(checked==DispatchResult::idle?DispatchResult::done:DispatchResult::partial):checked);return;
    }
    remaining &= pendingControl.eligible;
    if(!(remaining&bit)) continue;
    request.id=pendingControl.guard.members[i].id;request.command=somfy_commands::My;
    if(shadeRequests.run(request)!=shade_request_result_t::done) {
      finishControl(sent?DispatchResult::partial:DispatchResult::refused);return;
    }
    sent=true;remaining &= ~bit;
    if(!remaining) {finishControl(DispatchResult::done);return;}
  }
  finishControl(sent?DispatchResult::done:DispatchResult::idle);
}

} // namespace LocalUi
#endif
