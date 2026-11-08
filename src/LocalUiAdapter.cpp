#ifdef ESPSOMFY_LOCAL_UI
#include "LocalUiAdapter.h"
#include "LocalUiAuthorizationAdapter.h"
#include "LocalStopLease.h"
#include "Somfy.h"
#include "ShadeRequests.h"
#include <Preferences.h>
#include <math.h>
#include <string.h>
extern SomfyShadeController somfy;
extern ShadeRequests shadeRequests;
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
bool readTarget(EntityRef ref,Target &target,TargetContext &context,uint32_t &moving) {
  target={}; context={}; moving=0; target.ref=ref;
  if(!ref.valid()) return false;
  context.ref=ref; context.modelRevision=somfy.configurationRevision;
  context.restoreRevision=somfy.restoreRevision; context.radioRevision=somfy.transceiver.configurationRevision;
  if(ref.kind==EntityKind::device) {
    auto *shade=somfy.getShadeById(ref.id); if(!shade) return false;
    target.exists=true; label(target.name,shade->name);
    target.identity=context.identity=shade->getRemoteAddress();
    target.revision=context.revision=shade->configurationRevision;
    context.memberCount=1; context.members[0]={ref.id,context.identity,context.revision};
    target.descriptor=descriptor(*shade);
    target.capabilities=capabilities(target.descriptor);
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
    target.identity=context.identity=group->getRemoteAddress();
    target.revision=context.revision=group->configurationRevision;
    target.descriptor.group=true; target.descriptor.proto=group->proto;
    target.descriptor.bitLength=effectiveBits(group->bitLength);
    ControlMask common=allControlActions;
    bool compatible=rf(group->proto) && context.identity!=0;
    uint8_t ids[32]={}, count=0;
    for(auto id:group->linkedShades) {
      if(!id || id==255) continue;
      uint8_t at=count;
      while(at && ids[at-1]>id) { ids[at]=ids[at-1]; --at; }
      if((at && ids[at-1]==id) || (at<count && ids[at]==id)) return false;
      ids[at]=id; ++count;
    }
    context.memberCount=count;
    for(uint8_t i=0;i<count;i++) {
      auto *shade=somfy.getShadeById(ids[i]); if(!shade) return false;
      const auto member=descriptor(*shade);
      context.members[i]={ids[i],shade->getRemoteAddress(),shade->configurationRevision};
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
      const auto d=descriptor(shade); out.available=capabilities(d)!=0;
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
} // namespace
bool localControlsFrozen() { return controlsFrozen; }
void setLocalControlsFrozen(bool value) {
  if(value && !controlsFrozen) lockAuthorization();
  controlsFrozen=value;
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
  m.ready=!controlsFrozen && (!rf(m.target.descriptor.proto) ||
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
  if(controlsFrozen) return DispatchResult::refused;
  if(!hasFocus || !cmd.ref.valid() || cmd.ref!=focusedContext.ref || !cmd.token || cmd.token!=focusedToken ||
     presentedRef!=cmd.ref || presentedToken!=cmd.token) return DispatchResult::stale;
  Target target; TargetContext current; uint32_t moving=0;
  if(!readTarget(cmd.ref,target,current,moving) || !current.same(focusedContext)) return DispatchResult::stale;
  const auto security=pollAuthorization();
  const bool session=security.allowed && cmd.authGeneration==security.generation;
  View view; view.entity=cmd.ref; view.openedToken=cmd.token;
  view.authorizedSelection=session; view.selectionAuthGeneration=cmd.authGeneration;
  focusedMoving=moving; observeLease(view,security);
  if(!session && !(cmd.action==Action::stop && leasedMoving && cmd.authGeneration &&
     cmd.authGeneration==security.expiredSessionGeneration)) return DispatchResult::denied;
  if(rf(target.descriptor.proto) && (!somfy.transceiver.available() || somfy.transceiver.txSuspended()))
    return DispatchResult::refused;
  if(!(target.capabilities & controlActionBit(cmd.action))) return DispatchResult::refused;
  shade_request_t request;
  if(cmd.action==Action::stop) {
    const uint32_t all=current.memberCount==32?UINT32_MAX:(uint32_t(1)<<current.memberCount)-1;
    const uint32_t eligible=session?moving:moving&leasedMoving;
    if(!eligible) return DispatchResult::idle;
    if(cmd.ref.kind==EntityKind::group && moving==all && (session || leasedMoving==all)) {
      request.type=shade_request_type_t::group_command; request.id=cmd.ref.id; request.command=somfy_commands::My;
      return shadeRequests.run(request)==shade_request_result_t::done?DispatchResult::done:DispatchResult::refused;
    }
    bool sent=false;
    for(uint8_t i=0;i<current.memberCount;i++) {
      if(!(eligible&(uint32_t(1)<<i))) continue;
      auto *shade=somfy.getShadeById(current.members[i].id);
      if(!shade || shade->isIdle()) continue;
      request.id=current.members[i].id; request.command=somfy_commands::My;
      if(shadeRequests.run(request)!=shade_request_result_t::done) return sent?DispatchResult::partial:DispatchResult::refused;
      sent=true;
    }
    return sent?DispatchResult::done:DispatchResult::idle;
  }
  const auto prepared=prepareControlRequest(cmd.action,target.descriptor,cmd.ref.id,cmd.value,cmd.step,request);
  if(prepared!=ControlResult::done) return prepared==ControlResult::idle?DispatchResult::idle:DispatchResult::refused;
  return shadeRequests.run(request)==shade_request_result_t::done?DispatchResult::done:DispatchResult::refused;
#endif
}
} // namespace LocalUi
#endif
