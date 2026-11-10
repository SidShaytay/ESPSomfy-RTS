#ifdef ESPSOMFY_LOCAL_UI
#include "LocalAdvancedRuntime.h"
#include "CounterEdit.h"
#include "CounterRuntime.h"
#include "GroupReadiness.h"
#include "GroupMembership.h"
#include "LocalMembershipRead.h"
#include "ModelIdleForAdministration.h"
#include "LocalEntityRuntime.h"
#include "LocalOperationAdmission.h"
#include "LocalUiAuthorizationAdapter.h"
#include "LoopQueue.h"
#include "ModelCommitService.h"
#include "SafeRestart.h"
#include "Somfy.h"
#include "SomfyEditor.h"
#include <esp_timer.h>
#include <stdio.h>
#include <string.h>
extern SomfyShadeController somfy;
extern LoopQueue loopQueue;
extern ShadeRequests shadeRequests;
namespace LocalUi {
namespace Advanced {
using namespace EntityAdmin;
using EntityAdmin::Authorization;
namespace {
class ConfigurationLease {
  GitUpdateOwnerToken token;
  bool held = false;

public:
  ConfigurationLease() {
    held =
        gitUpdate.acquireExternal(GitUpdateExternalOwner::configuration, token);
  }
  ~ConfigurationLease() {
    if (held)
      gitUpdate.releaseExternal(token);
  }
  bool owns() const { return held && gitUpdate.ownsExternal(token); }
};
class RuntimeBackend final : public Backend {
  bool sensorRequest(Target target,const SensorSimulation &draft,shade_request_t &request){
    radio_proto protocol;uint8_t bits;uint32_t address;
    if(target.scope==Scope::devices){
      auto *shade=somfy.getShadeById(target.id);if(!shade)return false;
      protocol=shade->proto;bits=shade->bitLength;address=shade->getRemoteAddress();
    }else if(target.scope==Scope::groups){
      auto *group=somfy.getGroupById(target.id);if(!group)return false;
      protocol=group->proto;bits=group->bitLength;address=group->getRemoteAddress();
    }else return false;
    return address&&address<=0xffffff&&prepareSensorSimulation(protocol,bits?bits:somfy.transceiver.config.type,
        target.scope==Scope::groups,target.id,draft,request);
  }

  bool remoteRequest(Target target,const RemoteProgram &program,shade_request_t &request) {
    radio_proto protocol;uint8_t bits;uint32_t address;
    if(target.scope==Scope::devices) {
      auto *shade=somfy.getShadeById(target.id);if(!shade)return false;
      protocol=shade->proto;bits=shade->bitLength;address=shade->getRemoteAddress();
    } else if(target.scope==Scope::groups) {
      auto *group=somfy.getGroupById(target.id);if(!group)return false;
      protocol=group->proto;bits=group->bitLength;address=group->getRemoteAddress();
    } else return false;
    return address&&address<=0xffffff&&prepareRemoteProgram(protocol,bits?bits:somfy.transceiver.config.type,
        target.scope==Scope::groups,target.id,program,request);
  }
  bool pairingRequest(Target target,uint8_t repeats,shade_request_t &out,bool *oneButton=nullptr){
    if(target.scope!=Scope::devices||!repeats||repeats>127)return false;
    auto *shade=somfy.getShadeById(target.id);if(!shade)return false;
    const auto protocol=shade->proto;const auto bits=shade->bitLength?shade->bitLength:somfy.transceiver.config.type;
    const auto address=shade->getRemoteAddress();
    if(!address||address>0xffffff||(protocol!=radio_proto::RTS&&protocol!=radio_proto::RTW&&protocol!=radio_proto::RTV)||
       (bits!=56&&bits!=80))return false;
    shade_request_t request;request.type=shade_request_type_t::shade_command;request.id=target.id;
    request.command=somfy_commands::Prog;request.repeat=int8_t(repeats);out=request;
    if(oneButton)*oneButton=isToggleShade(shade->shadeType);
    return true;
  }
  bool validTarget(Target target) {
    if (target.scope == Scope::repeaters)
      return !target.id;
    if (target.scope == Scope::devices)
      return target.id && target.id != 255 && somfy.getShadeById(target.id);
    if (target.scope == Scope::groups)
      return target.id && target.id != 255 && somfy.getGroupById(target.id);
    return false;
  }
  Admission admittedIdentity(Authorization auth, uint64_t revision) {
    const auto now = authorization();
    if (!auth.allowed || !now.allowed)
      return Admission::locked;
    if (!auth.generation || !auth.configurationGeneration || !revision ||
        auth.generation != now.generation ||
        auth.configurationGeneration != now.configurationGeneration ||
        revision != this->revision())
      return Admission::stale;
    return Admission::accepted;
  }
  Admission admit(Target target, const Context &expected, Authorization auth,
                  uint64_t revision,
                  const ConfigurationLease *lease = nullptr) {
    const auto identity = admittedIdentity(auth, revision);
    if (identity != Admission::accepted)
      return identity;
    if ((lease ? !lease->owns() : gitUpdate.busy()) || loopQueue.pending() ||
        restartPending())
      return Admission::busy;
    Context current;
    if (!capture(target, current) || !expected.same(current))
      return Admission::stale;
    return Admission::accepted;
  }
  bool idle(Target target) {
    if (target.scope == Scope::repeaters)
      return somfy.transceiver.restartQuiescent();
    if (target.scope == Scope::devices) {
      auto *shade = somfy.getShadeById(target.id);
      return shade && shade->idleForAdministration();
    }
    auto *group = somfy.getGroupById(target.id);
    if (!group)
      return false;
    // Missing members remain repairable. Only present physical members must
    // be idle; cached group.direction is the last command, not live motion.
    return !readGroupReadiness(somfy, *group).activeMember;
  }
  bool matches(Target target, const Action &action) {
    if (action.kind == ActionKind::setPaired)
      return target.scope == Scope::devices && !action.key && !action.slot;
    Item item;
    const uint8_t count = itemCount(target);
    for (uint8_t i = 0; i < count; i++)
      if (itemAt(target, i, item) && item.slot == action.slot &&
          item.key == action.key) {
        if (action.kind == ActionKind::unlinkRemote)
          return target.scope == Scope::devices;
        if (action.kind == ActionKind::unlinkRepeater)
          return target.scope == Scope::repeaters;
        if (action.kind == ActionKind::removeMissingMember)
          return target.scope == Scope::groups && item.missing &&
                 action.key > 0 && action.key < 255;
      }
    return false;
  }

  bool identityBlocked(Target target, const Action &action) {
    uint32_t address = 0;
    if (action.kind == ActionKind::setPaired) {
      auto *shade = somfy.getShadeById(target.id);
      if (shade)
        address = shade->getRemoteAddress();
    } else if (action.kind == ActionKind::unlinkRemote ||
               action.kind == ActionKind::unlinkRepeater)
      address = action.key;
    return address && somfy.transceiver.counterBlocksIdentity(address);
  }
  static ApplyResult counterRefused() {
    ApplyResult result;
    result.admission = Admission::busy;
    strcpy(result.reason, "Counter recovery is required for this identity.");
    return result;
  }

  static MembershipApplyResult membershipRefused(Admission admission,const char *reason) {
    MembershipApplyResult out;out.result.admission=admission;
    snprintf(out.result.reason,sizeof(out.result.reason),"%s",reason);return out;
  }
  Admission membershipAdmit(Target target,const Context &context,Authorization auth,
      uint64_t history,const MembershipSelection &selection,const ConfigurationLease *lease=nullptr) {
    auto result=admit(target,context,auth,history,lease);
    if(result!=Admission::accepted)return result;
    result=membershipCurrent(target,context,auth,history,selection);
    if(result!=Admission::accepted)return result;
    if(!configuredShadesIdleForAdministration(somfy)||!somfy.transceiver.restartQuiescent()||somfy.transceiver.txSuspended())return Admission::busy;
    // Configured aliases must remain visible through counter recovery. Missing
    // references/unused sentinels remove no actual alias and stay repairable.
    if(selection.hasCandidate&&selection.candidate.targetExists) {
      if(somfy.transceiver.counterBlocksIdentity(context.raw.identity)||somfy.transceiver.counterBlocksIdentity(selection.candidate.identity))return Admission::busy;
    }
    return Admission::accepted;
  }
  const char *repeaterCandidateRefusal(const LearnSelection &candidate){
    if(!candidate.session||!candidate.revision||!candidate.address||candidate.address>0xffffff||
      candidate.protocol>2||(candidate.bits!=56&&candidate.bits!=80))return "Invalid received remote selection.";
    bool free=false;
    for(const auto address:somfy.repeaters){if(address==candidate.address)return "Remote is already a repeater.";if(!address)free=true;}
    return free?nullptr:"Repeater list is full; remove an entry first.";
  }
  LearnApplyResult linkObservedRepeater(Target target,const Context &context,Authorization auth,uint64_t history,
      const LearnSelection &candidate,LearnPending &witness,ArduinoJson::Allocator &allocator){
    ConfigurationLease lease;auto admission=admit(target,context,auth,history,&lease);
    if(admission!=Admission::accepted)return {{},admission};
    if(!somfy.transceiver.restartQuiescent())return {{},Admission::busy};
    if(const auto *reason=repeaterCandidateRefusal(candidate))return {edit_result_t::error(reason),Admission::invalid};
    JsonDocument payload(&allocator),expanded(&allocator);
    payload[protoField(proto_field_t::remoteAddress).key]=candidate.address;
    proto_error_buffer_t why={};const auto *error=expandEditPayload(model_edit_type_t::link_repeater,payload.as<JsonObjectConst>(),expanded,why);
    if(payload.overflowed()||expanded.overflowed())return {{},Admission::noMemory};
    if(error)return {{},Admission::invalid};
    admission=admit(target,context,auth,history,&lease);if(admission!=Admission::accepted)return {{},admission};
    admission=checkLearnContext(target,context,auth,history);if(admission!=Admission::accepted)return {{},admission};
    if(!somfy.transceiver.restartQuiescent())return {{},Admission::busy};
    if(const auto *reason=repeaterCandidateRefusal(candidate))return {edit_result_t::error(reason),Admission::invalid};
    const auto fresh=witness.current(uint64_t(esp_timer_get_time())/1000);
    if(fresh!=LearnQueueReason::ready)return {{},Admission::accepted,fresh};
    model_edit_t edit;edit.type=model_edit_type_t::link_repeater;swap(edit.doc,expanded);return {applyModelEdit(edit)};
  }
public:
  bool supportsSensor(Target target)override{
    SensorSimulation probe;probe.sunny=0;shade_request_t request;
    return authorization().allowed&&somfy.modelLoad.usable()&&sensorRequest(target,probe,request);
  }
  Admission checkSensorContext(Target target,const Context &expected,Authorization auth,uint64_t history)override{
    const auto identity=admittedIdentity(auth,history);if(identity!=Admission::accepted)return identity;
    Context current;
    if(!supportsSensor(target)||!capture(target,current)||!expected.same(current))return Admission::stale;
    if(!somfy.transceiver.available()||somfy.transceiver.txSuspended()||!idle(target))return Admission::busy;
    if(target.scope==Scope::groups&&readGroupReadiness(somfy,*somfy.getGroupById(target.id)).unknownMovement())return Admission::busy;
    return Admission::accepted;
  }
  SensorResult sendSensor(Target target,const Context &expected,Authorization auth,uint64_t history,
                          const SensorSimulation &draft,LearnPending &witness)override{
    ConfigurationLease lease;
    auto admission=admit(target,expected,auth,history,&lease);
    if(admission!=Admission::accepted)return {shade_request_result_t::radio_refused,admission};
    admission=checkSensorContext(target,expected,auth,history);
    if(admission!=Admission::accepted)return {shade_request_result_t::radio_refused,admission};
    if(!somfy.transceiver.restartQuiescent()||somfy.transceiver.counterBlocksIdentity(expected.raw.identity))
      return {shade_request_result_t::radio_refused,Admission::busy};
    shade_request_t request;
    if(!sensorRequest(target,draft,request))return {shade_request_result_t::radio_refused,Admission::invalid};
    if(witness.current(uint64_t(esp_timer_get_time())/1000)!=LearnQueueReason::ready)
      return {shade_request_result_t::radio_refused,Admission::stale};
    return {shadeRequests.run(request)};
  }
  bool statusRow(Target target,uint8_t index,FormRow &row)override{
    row={};if(!authorization().allowed||!somfy.modelLoad.usable()||target.scope==Scope::repeaters||!validTarget(target))return false;
    EntityStatus state;state.group=target.scope==Scope::groups;state.radioReady=somfy.transceiver.available()&&!somfy.transceiver.txSuspended();
    if(state.group){auto *group=somfy.getGroupById(target.id);state.roomId=group->roomId;state.protocol=group->proto;
      state.bits=group->bitLength?group->bitLength:somfy.transceiver.config.type;state.flags=group->getFlags();state.sunSensor=state.flags&2;
      state.unknownMotion=readGroupReadiness(somfy,*group).unknownMovement();
      for(const auto id:group->linkedShades)if(id&&id!=255){auto *member=somfy.getShadeById(id);if(member){++state.members;if(!member->idleForAdministration())++state.activeMembers;}else ++state.missing;}
    }else{auto *shade=somfy.getShadeById(target.id);const auto &live=shade->state();
      state.roomId=shade->roomId;state.shadeType=shade->shadeType;state.tiltType=shade->tiltType;state.protocol=shade->proto;
      state.bits=shade->bitLength?shade->bitLength:somfy.transceiver.config.type;state.flags=shade->getFlags();state.sunSensor=shade->sunSensor;
      state.paired=shade->paired;state.flip=shade->flipPosition;state.lift=live.currentPos;state.liftTarget=live.target;
      state.tilt=live.currentTiltPos;state.tiltTarget=live.tiltTarget;state.favourite=live.myPos;state.tiltFavourite=live.myTiltPos;
      state.direction=live.direction;state.tiltDirection=live.tiltDirection;
    }
    if(state.roomId){auto *room=somfy.getRoomById(state.roomId);if(room)snprintf(state.room,sizeof(state.room),"%s",room->name);}
    return entityStatusRow(state,index,row);
  }
  bool membershipRows(Target target,bool candidates,MembershipRows &out) override {
    out={};
    if(!authorization().allowed||target.scope!=Scope::groups||!somfy.modelLoad.usable())return false;
    MembershipReader<SomfyShadeController> reader(somfy);
    return reader.read(target.id,candidates,EntityAdmin::runtimeBackend(),out);
  }
  bool membershipLabel(Target target,bool candidates,const MembershipRows &rows,uint8_t index,Choice &name,char (&detail)[48]) override {
    if(!authorization().allowed||target.scope!=Scope::groups)return false;
    MembershipReader<SomfyShadeController> reader(somfy);return reader.label(target.id,candidates,rows,index,name,detail);
  }
  bool selectMembership(Target target,const Context &context,bool candidates,const MembershipRows &rows,uint8_t index,MembershipSelection &out) override {
    out={};Context current;
    if(!authorization().allowed||target.scope!=Scope::groups||!capture(target,current)||!context.same(current))return false;
    MembershipReader<SomfyShadeController> reader(somfy);return reader.select(target.id,candidates,rows,index,out);
  }
  Admission membershipCurrent(Target target,const Context &context,Authorization auth,uint64_t history,const MembershipSelection &selection) override {
    const auto admitted=checkLearnContext(target,context,auth,history);
    if(admitted!=Admission::accepted)return admitted;
    if(target.scope!=Scope::groups)return Admission::invalid;
    MembershipReader<SomfyShadeController> reader(somfy);
    return reader.current(target.id,selection)?Admission::accepted:Admission::stale;
  }
  MembershipApplyResult applyMembership(Target target,const Context &context,Authorization auth,uint64_t history,
      const MembershipSelection &selection,LearnPending &witness,ArduinoJson::Allocator &allocator) override {
    auto admission=membershipAdmit(target,context,auth,history,selection);
    if(admission!=Admission::accepted)return membershipRefused(admission,"Wait for movement, queued/radio work, recovery or restart.");
    auto *group=somfy.getGroupById(target.id);
    if(selection.adding&&!inspectGroupMembership(group->linkedShades,selection.id).freeSlots)return membershipRefused(Admission::invalid,"Group is full; remove an entry or unused placeholder first.");
    ConfigurationLease lease;
    admission=membershipAdmit(target,context,auth,history,selection,&lease);
    if(admission!=Admission::accepted)return membershipRefused(admission,"Configuration is busy or the reviewed selection changed.");
    const auto type=selection.adding?model_edit_type_t::link_to_group:model_edit_type_t::unlink_from_group;
    JsonDocument payload(&allocator),expanded(&allocator);
    payload[protoField(proto_field_t::groupId).key]=target.id;
    payload[protoField(proto_field_t::shadeId).key]=selection.id;
    proto_error_buffer_t why={};const auto error=expandEditPayload(type,payload.as<JsonObjectConst>(),expanded,why);
    if(payload.overflowed()||expanded.overflowed())return membershipRefused(Admission::noMemory,"No draft memory; no membership change was applied.");
    if(error)return membershipRefused(Admission::invalid,error);
    admission=membershipAdmit(target,context,auth,history,selection,&lease);
    if(admission!=Admission::accepted)return membershipRefused(admission,"Selection/work changed during preparation; review again.");
    const auto fresh=witness.current(uint64_t(esp_timer_get_time())/1000);
    if(fresh!=LearnQueueReason::ready)return {{},fresh};
    model_edit_t edit;edit.type=type;swap(edit.doc,expanded);
    MembershipApplyResult out;out.result.edit=applyModelEdit(edit);return out;
  }
  LearnFileResult retryMembershipFile(Target target,const Context &context,Authorization auth,uint64_t history,LearnPending &witness) override {
    auto admission=admit(target,context,auth,history);
    if(admission!=Admission::accepted)return {{},admission};
    if(target.scope!=Scope::groups)return {{},Admission::invalid};
    ConfigurationLease lease;admission=admit(target,context,auth,history,&lease);
    if(admission!=Admission::accepted)return {{},admission};
    const auto fresh=witness.current(uint64_t(esp_timer_get_time())/1000);
    if(fresh!=LearnQueueReason::ready)return {{},Admission::accepted,fresh};
    return {applyModelCommit({},ModelCommitMode::save)};
  }
  bool supportsProgramming(Target target) override {
    shade_request_t request;
    return authorization().allowed&&somfy.modelLoad.usable()&&remoteRequest(target,{},request);
  }
  Admission checkProgrammingContext(Target target,const Context &expected,Authorization auth,uint64_t history) override {
    const auto identity=admittedIdentity(auth,history);if(identity!=Admission::accepted)return identity;
    Context current;
    return supportsProgramming(target)&&capture(target,current)&&expected.same(current)?Admission::accepted:Admission::stale;
  }
  ProgrammingResult sendProgramming(Target target,const Context &expected,Authorization auth,uint64_t history,
      const RemoteProgram &program,LearnPending &witness) override {
    ConfigurationLease lease;
    auto admission=admit(target,expected,auth,history,&lease);
    if(admission!=Admission::accepted)return {shade_request_result_t::radio_refused,admission};
    if(localControlsFrozen()||!idle(target)||!somfy.transceiver.available()||somfy.transceiver.txSuspended()||
       !somfy.transceiver.restartQuiescent()||somfy.transceiver.counterBlocksIdentity(expected.raw.identity))
      return {shade_request_result_t::radio_refused,Admission::busy};
    shade_request_t request;
    if(!remoteRequest(target,program,request))return {shade_request_result_t::radio_refused,Admission::invalid};
    if(witness.current(uint64_t(esp_timer_get_time())/1000)!=LearnQueueReason::ready)return {shade_request_result_t::radio_refused,Admission::stale};
    return {shadeRequests.run(request)};
  }
  bool pairingAvailable(Target target,bool &oneButton)override{
    oneButton=false;shade_request_t request;
    return authorization().allowed&&somfy.modelLoad.usable()&&pairingRequest(target,1,request,&oneButton);
  }
  Admission checkPairingContext(Target target,const Context &expected,Authorization auth,uint64_t history)override{
    const auto identity=admittedIdentity(auth,history);if(identity!=Admission::accepted)return identity;
    bool oneButton=false;Context current;
    if(!pairingAvailable(target,oneButton)||!capture(target,current)||!expected.same(current))return Admission::stale;
    if(!idle(target)||!somfy.transceiver.available()||somfy.transceiver.txSuspended())return Admission::busy;
    return Admission::accepted;
  }
  PairingSendResult sendPairing(Target target,const Context &expected,Authorization auth,uint64_t history,
                                uint8_t repeats,LearnPending &witness)override{
    ConfigurationLease lease;
    auto admission=admit(target,expected,auth,history,&lease);
    if(admission!=Admission::accepted)return {shade_request_result_t::radio_refused,admission};
    admission=checkPairingContext(target,expected,auth,history);
    if(admission!=Admission::accepted)return {shade_request_result_t::radio_refused,admission};
    if(!somfy.transceiver.restartQuiescent()||somfy.transceiver.counterBlocksIdentity(expected.raw.identity))
      return {shade_request_result_t::radio_refused,Admission::busy};
    shade_request_t request;
    if(!pairingRequest(target,repeats,request))return {shade_request_result_t::radio_refused,Admission::invalid};
    if(witness.current(uint64_t(esp_timer_get_time())/1000)!=LearnQueueReason::ready)return {shade_request_result_t::radio_refused,Admission::stale};
    return {shadeRequests.run(request)};
  }
  bool groupProgrammingSelection(Target target,bool adding,uint8_t id,GroupProgrammingSelection &out)override{
    out={};
    if(!authorization().allowed||!somfy.modelLoad.usable()||target.scope!=Scope::groups||!id||id==255)return false;
    auto *group=somfy.getGroupById(target.id);auto *shade=somfy.getShadeById(id);
    if(!group||!shade)return false;
    const auto protocol=shade->proto;
    const auto bits=shade->bitLength?shade->bitLength:somfy.transceiver.config.type;
    const auto groupBits=group->bitLength?group->bitLength:somfy.transceiver.config.type;
    const auto address=shade->getRemoteAddress(),groupAddress=group->getRemoteAddress();
    if(!address||address>0xffffff||!groupAddress||groupAddress>0xffffff||address==groupAddress||
      (protocol!=radio_proto::RTS&&protocol!=radio_proto::RTW&&protocol!=radio_proto::RTV)||
      group->proto!=protocol||bits!=groupBits||(bits!=56&&bits!=80)||!shade->configurationRevision)return false;
    bool present=false,free=false;
    for(auto member:group->linkedShades){if(member==id)present=true;if(!member)free=true;}
    if(adding?(present||!free):!present)return false;
    OrderIds ids{},slots{};uint8_t count=0;if(!targets(Scope::devices,ids,slots,count)||count>32)return false;
    bool found=false;uint8_t slot=0;
    for(uint8_t i=0;i<count;++i)if(ids[i]==id){found=true;slot=slots[i];break;}
    if(!found)return false;
    out.deviceId=id;out.deviceSlot=slot;out.deviceRevision=shade->configurationRevision;
    out.deviceAddress=address;out.groupAddress=groupAddress;out.protocol=uint8_t(protocol);out.bits=bits;
    out.shadeType=uint8_t(shade->shadeType);out.adding=adding;return true;
  }
  Admission groupProgrammingCurrent(Target target,const Context &context,Authorization auth,uint64_t history,
      const GroupProgrammingSelection &selection)override{
    const auto admitted=admittedIdentity(auth,history);if(admitted!=Admission::accepted)return admitted;
    Context actual;
    if(target.scope!=Scope::groups||!somfy.modelLoad.usable()||!capture(target,actual)||!context.same(actual))return Admission::stale;
    GroupProgrammingSelection current;
    if(!groupProgrammingSelection(target,selection.adding,selection.deviceId,current)||!selection.same(current))return Admission::stale;
    auto *shade=somfy.getShadeById(selection.deviceId);auto *group=somfy.getGroupById(target.id);
    const auto readiness=readGroupReadiness(somfy,*group);
    if(!shade->idleForAdministration()||readiness.activeMember||readiness.unknownMovement()||
      !somfy.transceiver.available()||somfy.transceiver.txSuspended())return Admission::busy;
    return Admission::accepted;
  }
  GroupProgrammingSendResult sendGroupProgramming(Target target,const Context &context,Authorization auth,uint64_t history,
      const GroupProgrammingSelection &selection,bool memory,LearnPending &witness)override{
    ConfigurationLease lease;auto admitted=admit(target,context,auth,history,&lease);
    if(admitted!=Admission::accepted)return {shade_request_result_t::radio_refused,admitted};
    admitted=groupProgrammingCurrent(target,context,auth,history,selection);
    if(admitted!=Admission::accepted)return {shade_request_result_t::radio_refused,admitted};
    if(!somfy.transceiver.restartQuiescent()||somfy.transceiver.counterBlocksIdentity(selection.deviceAddress)||
      somfy.transceiver.counterBlocksIdentity(selection.groupAddress))return {shade_request_result_t::radio_refused,Admission::busy};
    shade_request_t request;request.type=memory?shade_request_type_t::shade_command:shade_request_type_t::group_command;
    request.id=memory?selection.deviceId:target.id;request.command=somfy_commands::Prog;request.repeat=memory?40:1;
    if(witness.current(uint64_t(esp_timer_get_time())/1000)!=LearnQueueReason::ready)
      return {shade_request_result_t::radio_refused,Admission::stale};
    return {shadeRequests.run(request)};
  }
  LearnApplyResult saveProgrammingMembership(Target target,const Context &context,Authorization auth,uint64_t history,
      const GroupProgrammingSelection &selection,LearnPending &witness,ArduinoJson::Allocator &allocator)override{
    ConfigurationLease lease;auto admitted=admit(target,context,auth,history,&lease);
    if(admitted!=Admission::accepted)return {{},admitted};
    admitted=groupProgrammingCurrent(target,context,auth,history,selection);
    if(admitted!=Admission::accepted)return {{},admitted};
    if(!somfy.transceiver.restartQuiescent())return {{},Admission::busy};
    const auto type=selection.adding?model_edit_type_t::link_to_group:model_edit_type_t::unlink_from_group;
    JsonDocument payload(&allocator),expanded(&allocator);payload[protoField(proto_field_t::groupId).key]=target.id;payload[protoField(proto_field_t::shadeId).key]=selection.deviceId;
    proto_error_buffer_t why;const auto *error=expandEditPayload(type,payload.as<JsonObjectConst>(),expanded,why);
    if(payload.overflowed()||expanded.overflowed())return {{},Admission::noMemory};
    if(error)return {{},Admission::invalid};
    admitted=admit(target,context,auth,history,&lease);if(admitted!=Admission::accepted)return {{},admitted};
    admitted=groupProgrammingCurrent(target,context,auth,history,selection);if(admitted!=Admission::accepted)return {{},admitted};
    if(!somfy.transceiver.restartQuiescent())return {{},Admission::busy};
    const auto fresh=witness.current(uint64_t(esp_timer_get_time())/1000);
    if(fresh!=LearnQueueReason::ready)return {{},Admission::accepted,fresh};
    model_edit_t edit;edit.type=type;swap(edit.doc,expanded);return {applyModelEdit(edit)};
  }
  PrimaryReadResult readPrimary(Target target,const Context &context,
      Authorization auth,uint64_t history,PrimaryCounterReview &out) override {
    out={};
    auto admission=admit(target,context,auth,history);
    if(admission!=Admission::accepted)return {CounterDomainState::invalid,admission};
    if(target.scope!=Scope::devices)return {CounterDomainState::invalid,Admission::invalid};
    const auto state=reviewPrimaryCounter(target.id,out);
    admission=admit(target,context,auth,history);
    if(admission!=Admission::accepted)out={};
    return {state,admission};
  }
  bool primaryCurrent(Target target,const PrimaryCounterReview &review) override {
    return target.scope==Scope::devices && primaryCounterReviewCurrent(target.id,review);
  }
  PrimaryApplyResult correctPrimary(Target target,const Context &context,
      Authorization auth,uint64_t history,const PrimaryCounterReview &review,
      uint16_t code,LearnPending &pending) override {
    auto admission=admit(target,context,auth,history);
    if(admission!=Admission::accepted)return {{},admission};
    if(!primaryCurrent(target,review))return {{},Admission::stale};
    // No configuration lease: the shared correction acquires its counter
    // owner/reservation. This witness remains read-only under that owner.
    class Witness final : public CounterCorrectionWitness {
      LearnPending &pending;
    public:
      LearnQueueReason reason=LearnQueueReason::ready;
      explicit Witness(LearnPending &p):pending(p){}
      bool current() override {
        reason=pending.current(uint64_t(esp_timer_get_time())/1000);
        return reason==LearnQueueReason::ready;
      }
    } witness(pending);
    if(!witness.current())return {{},Admission::accepted,witness.reason};
    const auto result=applyPrimaryCounterCorrection(target.id,code,review,witness);
    return {result,Admission::accepted,witness.reason};
  }
  ReceiveHub &receivedFrames() override { return LocalUi::receiveHub(); }
  LearnQueued queueLearn(LearnPending &pending, uint64_t now) override {
    return submitLocalLearn(pending, now);
  }
  bool cancelLearn(uint64_t token, LearnPending &pending) override {
    return cancelLocalLearn(token, pending);
  }
  Admission checkLearnContext(Target target, const Context &expected,
                              Authorization auth, uint64_t history) override {
    const auto identity = admittedIdentity(auth, history);
    if (identity != Admission::accepted)
      return identity;
    if ((target.scope != Scope::devices && target.scope != Scope::groups && target.scope != Scope::repeaters) || !somfy.modelLoad.usable())
      return Admission::stale;
    Context current;
    return capture(target, current) && expected.same(current)
               ? Admission::accepted
               : Admission::stale;
  }
  LearnApplyResult linkLearn(Target target, const Context &context,
                             Authorization expected, uint64_t history,
                             const LearnSelection &candidate,
                             LearnPending &witness,
                             ArduinoJson::Allocator &allocator) override {
    if(target.scope!=Scope::devices&&target.scope!=Scope::repeaters)return {{},Admission::invalid};
    auto admission = admit(target, context, expected, history);
    if (admission != Admission::accepted)
      return {{}, admission};
    admission = checkLearnContext(target, context, expected, history);
    if (admission != Admission::accepted)
      return {{}, admission};
    if(target.scope==Scope::repeaters)return linkObservedRepeater(target,context,expected,history,candidate,witness,allocator);
    JsonDocument payload(&allocator), expanded(&allocator);
    payload[protoField(proto_field_t::shadeId).key] = target.id;
    payload[protoField(proto_field_t::remoteAddress).key] = candidate.address;
    payload[protoField(proto_field_t::lastRollingCode).key] =
        candidate.rollingCode;
    proto_error_buffer_t why = {};
    const char *error =
        expandEditPayload(model_edit_type_t::link_remote,
                          payload.as<JsonObjectConst>(), expanded, why);
    if (payload.overflowed() || expanded.overflowed())
      return {{}, Admission::noMemory};
    if (error)
      return {{}, Admission::invalid};
    // No Stage1 configuration lease: checked linking owns its counter lease,
    // fresh NVS/all-alias guard and one model-file commit/fanout.
    admission = admit(target, context, expected, history);
    if (admission != Admission::accepted)
      return {{}, admission};
    admission = checkLearnContext(target, context, expected, history);
    if (admission != Admission::accepted)
      return {{}, admission};
    const auto fresh = witness.current(uint64_t(esp_timer_get_time()) / 1000);
    if (fresh != LearnQueueReason::ready)
      return {{}, Admission::accepted, fresh};
    model_edit_t edit;
    edit.type = model_edit_type_t::link_remote;
    swap(edit.doc, expanded);
    return {applyModelEdit(edit)};
  }
  LearnFileResult retryLearnFile(Target target, const Context &context,
                                 Authorization expected, uint64_t history,
                                 LearnPending &witness) override {
    if(target.scope!=Scope::devices&&target.scope!=Scope::repeaters)return {{},Admission::invalid};
    auto admission = admit(target, context, expected, history);
    if (admission != Admission::accepted)
      return {{}, admission};
    admission = checkLearnContext(target, context, expected, history);
    if (admission != Admission::accepted)
      return {{}, admission};
    const auto fresh = witness.current(uint64_t(esp_timer_get_time()) / 1000);
    if (fresh != LearnQueueReason::ready)
      return {{}, Admission::accepted, fresh};
    if (somfy.transceiver.counterQuarantine() == CounterQuarantine::none) {
      // Another deliberate Commit may already have recovered the counter file.
      // Protect this ordinary current-model serialization with its own lease.
      ConfigurationLease lease;
      admission = admit(target, context, expected, history, &lease);
      if (admission != Admission::accepted)
        return {{}, admission};
      const auto final = witness.current(uint64_t(esp_timer_get_time()) / 1000);
      if (final != LearnQueueReason::ready)
        return {{}, Admission::accepted, final};
      return {applyModelCommit({}, ModelCommitMode::counterFileRecovery)};
    }
    // Known-NVS recovery acquires its own counter owner. Unknown NVS returns
    // the shared typed refusal; neither path may replay linking or NVS writes.
    return {applyModelCommit({}, ModelCommitMode::counterFileRecovery)};
  }
  Authorization authorization() override {
    const auto state = pollAuthorization();
    return {state.allowed && !localControlsFrozen(), state.generation,
            state.configurationGeneration};
  }
  uint64_t revision() override { return somfy.configurationRevision; }
  void activity() override { authorizationActivity(); }
  bool targets(Scope scope, OrderIds &ids, OrderIds &slots,
               uint8_t &count) override {
    if (!authorization().allowed ||
        (scope != Scope::devices && scope != Scope::groups))
      return false;
    bool normalization = false;
    return EntityAdmin::runtimeBackend().readOrder(
        scope == Scope::devices ? Kind::shade : Kind::group, ids, slots, count,
        normalization);
  }
  bool targetLabel(Target target, uint8_t slot, Choice &out) override {
    out = {};
    if (!authorization().allowed || !validTarget(target) ||
        target.scope == Scope::repeaters)
      return false;
    Value value;
    if (!EntityAdmin::runtimeBackend().readOrderLabel(
            target.scope == Scope::devices ? Kind::shade : Kind::group, slot,
            target.id, value) ||
        !memchr(value.text, 0, sizeof(value.text)))
      return false;
    out.value = target.id;
    snprintf(out.label, sizeof(out.label), "#%u %s", unsigned(target.id),
             value.text);
    return true;
  }
  bool capture(Target target, Context &out) override {
    out = {};
    if (!authorization().allowed || !validTarget(target))
      return false;
    out.scope = target.scope;
    out.history = revision();
    if (!out.history)
      return false;
    if (target.scope == Scope::repeaters) {
      static_assert(sizeof(somfy.repeaters) == sizeof(out.repeaters),
                    "Capture all repeater slots");
      for (unsigned i = 0; i < 7; i++)
        out.repeaters[i] = somfy.repeaters[i];
      return true;
    }
    return readRawTargetContext(somfy,
                                {target.scope == Scope::devices
                                     ? EntityKind::device
                                     : EntityKind::group,
                                 target.id},
                                out.raw) &&
           out.raw.targetExists;
  }
  bool paired(Target target, bool &out) override {
    if (!authorization().allowed || target.scope != Scope::devices)
      return false;
    auto *shade = somfy.getShadeById(target.id);
    if (!shade)
      return false;
    out = shade->paired;
    return true;
  }
  uint8_t itemCount(Target target) override {
    if (!authorization().allowed || !validTarget(target))
      return 0;
    uint8_t count = 0;
    if (target.scope == Scope::devices) {
      for (auto &remote : somfy.getShadeById(target.id)->linkedRemotes)
        if (remote.getRemoteAddress())
          ++count;
    } else if (target.scope == Scope::groups) {
      for (auto id : somfy.getGroupById(target.id)->linkedShades)
        if (id && id != 255)
          ++count;
    } else
      for (auto address : somfy.repeaters)
        if (address)
          ++count;
    return count;
  }
  bool itemAt(Target target, uint8_t ordinal, Item &out) override {
    out = {};
    if (!authorization().allowed || !validTarget(target))
      return false;
    if (target.scope == Scope::devices) {
      auto &remotes = somfy.getShadeById(target.id)->linkedRemotes;
      for (uint8_t slot = 0; slot < SOMFY_MAX_LINKED_REMOTES; slot++) {
        const auto address = remotes[slot].getRemoteAddress();
        if (!address || ordinal--)
          continue;
        out.key = address;
        out.rollingCode = remotes[slot].lastRollingCode;
        out.slot = slot;
        snprintf(out.label, sizeof(out.label), "%lu",
                 static_cast<unsigned long>(address));
        return true;
      }
    } else if (target.scope == Scope::groups) {
      auto *group = somfy.getGroupById(target.id);
      for (uint8_t slot = 0; slot < SOMFY_MAX_GROUPED_SHADES; slot++) {
        const auto id = group->linkedShades[slot];
        if (!id || id == 255 || ordinal--)
          continue;
        auto *shade = somfy.getShadeById(id);
        out.key = id;
        out.slot = slot;
        out.missing = !shade;
        snprintf(out.label, sizeof(out.label), "#%u %s", unsigned(id),
                 shade ? "Device" : "Missing device");
        return true;
      }
    } else
      for (uint8_t slot = 0; slot < SOMFY_MAX_REPEATERS; slot++) {
        const auto address = somfy.repeaters[slot];
        if (!address || ordinal--)
          continue;
        out.key = address;
        out.slot = slot;
        snprintf(out.label, sizeof(out.label), "%lu",
                 static_cast<unsigned long>(address));
        return true;
      }
    return false;
  }
  ApplyResult apply(Target target, const Action &action, const Context &context,
                    Authorization expected, uint64_t revision,
                    ArduinoJson::Allocator &allocator) override {
    auto admission = admit(target, context, expected, revision);
    if (admission != Admission::accepted)
      return {{}, admission};
    if (!idle(target))
      return {{}, Admission::busy};
    if (!matches(target, action))
      return {{}, Admission::invalid};
    if (identityBlocked(target, action))
      return counterRefused();
    ConfigurationLease lease;
    admission = admit(target, context, expected, revision, &lease);
    if (admission != Admission::accepted)
      return {{}, admission};
    JsonDocument doc(&allocator);
    model_edit_type_t type;
    switch (action.kind) {
    case ActionKind::setPaired:
      type = model_edit_type_t::set_paired;
      doc["shadeId"] = target.id;
      doc["paired"] = action.paired;
      break;
    case ActionKind::unlinkRemote:
      type = model_edit_type_t::unlink_remote;
      doc["shadeId"] = target.id;
      doc["remoteAddress"] = action.key;
      break;
    case ActionKind::unlinkRepeater:
      type = model_edit_type_t::unlink_repeater;
      doc["remoteAddress"] = action.key;
      break;
    case ActionKind::removeMissingMember:
      type = model_edit_type_t::unlink_from_group;
      doc["groupId"] = target.id;
      doc["shadeId"] = action.key;
      break;
    default:
      return {{}, Admission::invalid};
    }
    if (doc.overflowed())
      return {{}, Admission::noMemory};
    admission = admit(target, context, expected, revision, &lease);
    if (admission != Admission::accepted)
      return {{}, admission};
    if (!idle(target))
      return {{}, Admission::busy};
    if (!matches(target, action))
      return {{}, Admission::stale};
    if (identityBlocked(target, action))
      return counterRefused();
    model_edit_t edit;
    edit.type = type;
    swap(edit.doc, doc);
    return {applyModelEdit(edit)};
  }
  CommitResult retryFile(Target target, const Context &context,
                         Authorization expected, uint64_t revision) override {
    auto admission = admit(target, context, expected, revision);
    if (admission != Admission::accepted)
      return {{}, admission};
    ConfigurationLease lease;
    admission = admit(target, context, expected, revision, &lease);
    if (admission != Admission::accepted)
      return {{}, admission};
    // Current model serialization only. No mutation, counter write or RF.
    return {applyModelCommit().file};
  }
};
static_assert(sizeof(RuntimeBackend) <= 16,
              "No retained Advanced model or context");
} // namespace
Backend &runtimeBackend() {
  static RuntimeBackend source;
  return source;
}
} // namespace Advanced
} // namespace LocalUi
#endif
