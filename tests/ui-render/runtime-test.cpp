#include "LocalUiRuntime.h"
#ifdef AUDIT_UI_RUNTIME_HOST_TEST
#include "AuditConsole.h"
namespace AuditConsole {
bool inputActive=false;
UiInput injectedInput;
uint8_t reportedScreen=255,reportedSelected=255,reportedMode=255,reportedFormSelected=255;
bool reportedVisible=false;
unsigned stateReports=0;
bool uiInput(UiInput &out){if(!inputActive)return false;out=injectedInput;injectedInput.turn=0;return true;}
void uiState(uint8_t screen,uint8_t selected,uint8_t mode,uint8_t formSelected,bool visible){
 reportedScreen=screen;reportedSelected=selected;reportedMode=mode;reportedFormSelected=formSelected;reportedVisible=visible;++stateReports;
}
}
#endif
#include "LocalQuickAccessNvs.h"
#include "LocalReceive.h"
#include "SafeRestart.h"
#include "LocalUiAdapter.h"
#include "LocalUiCapture.h"
#include "LocalUiAuthorizationAdapter.h"
#define LOCAL_QUICK_EXTERNAL_IO
#include "../ui-adapter/Model.h"
#include <assert.h>
#include <vector>
#include <string.h>
#include <stdio.h>
#include <map>
#include "DeviceOps.h"
#include "LoopQueue.h"
#include "LocalEntityRuntime.h"
#include "LocalAdvancedRuntime.h"
#include "LocalFrequencyScan.h"
#include "ModelIdleForAdministration.h"
#include "SomfyEditor.h"
#include "CounterRuntime.h"
#include "ModelCommitService.h"
namespace LocalUi {
bool readLocalNetworkRow(uint8_t index,EntityAdmin::FormRow &out){out={};if(index>=networkDetailRows)return false;
  const char *labels[]={"Connection","WiFi SSID","WiFi channel","WiFi signal","IP address","Ethernet link","Ethernet speed","Ethernet duplex","Back"};
  snprintf(out.label,sizeof(out.label),"%s",labels[index]);snprintf(out.value,sizeof(out.value),"%s",index==1?"Test network":index==3?"-55 dBm":index==8?"":"Available");return true;
}
struct RuntimeTestAccess {
  static uint16_t *losePixels(Runtime &ui) {auto *pixels=ui.memory.pixels;ui.memory.pixels=nullptr;return pixels;}
  static void restorePixels(Runtime &ui,uint16_t *pixels) {ui.memory.pixels=pixels;}
  static const View &view(const Runtime &ui) {return ui.controller.view();}
  static bool fullyPresented(const Runtime &ui){return ui.hasPresented && ui.bandIndex==43;}
  static uint32_t frameRevision(const Runtime &ui){return ui.revision;}
  static uint8_t nextBand(const Runtime &ui){return ui.bandIndex;}
  static uint8_t presentedSelection(const Runtime &ui){return ui.presentedView.selected;}
  static Screen presentedScreen(const Runtime &ui){return ui.presentedView.screen;}
  static const Settings::FormView &settings(const Runtime &ui) {return ui.settingsForm.view();}
  static void settingsRestartReason(Runtime &ui,RestartReason reason) {const_cast<Settings::FormView &>(ui.settingsForm.view()).restartReason=reason;}
  static const EntityAdmin::FormView &administration(const Runtime &ui) {return ui.administration.view();}
  static bool updatesActive(const Runtime &ui){return ui.systemChild==Runtime::SystemChild::updates;}
  static EntityAdmin::FormView system(const Runtime &ui) {EntityAdmin::FormView out;ui.renderSystem(out);return out;}
  static bool systemContains(const Runtime&ui,const char*needle) {
    for(const auto*view:{&ui.frameAdministration,&ui.presentedAdministration}) {
      if(strstr(view->title,needle)||strstr(view->hint,needle))return true;
      for(const auto&row:view->rows)if(strstr(row.label,needle)||strstr(row.value,needle))return true;
    }
    return false;
  }
  static bool noPresentation(const Runtime &ui) {
    return !ui.hasPresented && !ui.presentedSettingsValid && !ui.stopPresentedToken && !ui.validBands;
  }
};
}
bool hasPsram=true,allocateOK=true;
void *allocated=nullptr;
bool settingsAllocateOK=true;
std::map<void *,size_t> settingsAllocations;
uint64_t authTestNow=0;
static unsigned systemRestarts=0;
static RestartResult systemRestartResult{true,RestartReason::none};
static RestartSnapshot systemRestartStatus;
RestartSnapshot restartStatus(){return systemRestartStatus;}
RestartResult requestRestart(uint32_t) {++systemRestarts;if(systemRestartResult.admitted){systemRestartStatus.phase=RestartPhase::admitted;++systemRestartStatus.generation;}return systemRestartResult;}
// Bounded preference I/O boundary only. Production NVS/commit/readback has
// independent coverage; these journeys exercise the real UI and adapter.
static LocalUi::QuickAccessLoad quickReadState=LocalUi::QuickAccessLoad::missing;
static LocalUi::QuickAccess quickStored;
static unsigned quickReads=0,quickWrites=0;
static LocalUi::QuickPreferenceSave quickWriteResult=LocalUi::QuickPreferenceSave::saved;
LocalUi::QuickAccessLoad LocalUi::readQuickAccessPreferences(QuickAccess &out){
  ++quickReads;out={};if(quickReadState==QuickAccessLoad::valid)out=quickStored;return quickReadState;
}
LocalUi::QuickPreferenceSave LocalUi::writeQuickAccessPreferences(const QuickAccess &value,QuickPreferenceWitness &witness){
  assert(gitUpdate.busy());++quickWrites;
  if(!witness.current())return QuickPreferenceSave::failed;
  if(quickWriteResult!=QuickPreferenceSave::failed){quickStored=value;quickReadState=QuickAccessLoad::valid;}
  return quickWriteResult;
}
static unsigned settingsSaves=0;
static RestartReason settingsRestartCause=RestartReason::none;
static unsigned entityEdits=0,entityCommits=0;
// Synthetic checked-domain boundary: storage below is an in-memory projection,
// never NVS, RF or a real model file. Production counter-domain I/O is exercised
// independently by test-explicit-counter; these journeys test UI orchestration.
static unsigned learnedAttempts=0,learnedLinks=0,learnedWrites=0,learnedRetries=0;
static std::map<uint32_t,uint16_t> learnedCodes;
static uint32_t learnedModelAddress=0;
static CounterDomainState learnedInjected=CounterDomainState::saved;
static unsigned primaryReads=0,primaryAttempts=0,primaryWrites=0;
static CounterDomainState primaryInjected=CounterDomainState::saved;
static void (*afterPrimaryApply)()=nullptr;
static void (*afterLearnApply)()=nullptr;
static ModelCommitState entitySaveState=ModelCommitState::saved,entityCommitState=ModelCommitState::saved;
// A fake filesystem projection, not a real persistence claim. Failed writes
// retain its prior contents; uncertain verification makes the projection unknown.
static LocalUi::EntityAdmin::OrderIds fileOrder[3]{};
static int8_t fileRanks[3][32]{};
static uint8_t fileCounts[3]{};
static bool fileKnown=false,filePairedMarker=false;
static uint32_t fileRepeaterAddress=0;
static uint64_t settingsRevision=1;
uint64_t deviceSettingsRevision(settings_section_t section) {return uint8_t(section)<5?settingsRevision:0;}
bool readDeviceSettings(settings_section_t section,SettingsSnapshotSink &sink) {
  for(uint8_t i=0;i<settingsFieldCount(section,false);i++) {
    const auto &field=*settingsFieldAt(section,i,false);
    settings_value_t value;value.kind=field.schema().kind;
    value.integer=field.lo;value.number=float(field.lo)/100;
    if(field.field==proto_field_t::hostname) strcpy(value.text,"lcd-host");
    if(field.field==proto_field_t::ntpServer) strcpy(value.text,"pool.ntp.org");
    if(field.field==proto_field_t::posixZone) strcpy(value.text,"UTC0");
    if(field.secret()) {value.kind='b';value.boolean=true;sink.field(field.presenceField(),value);}
    else sink.field(field.field,value);
  }
  return true;
}
settings_apply_result_t applyDeviceSettings(settings_section_t section,JsonObject request,uint32_t origin) {
  assert(section==settings_section_t::general && origin==0 && request["ssdpBroadcast"].as<bool>());
  ++settingsSaves;++settingsRevision;
  settings_apply_result_t result;result.restartBlocked=settingsRestartCause!=RestartReason::none;result.restartReason=settingsRestartCause;return result;
}
int testPins[64]={};uint32_t testNow=0;
SomfyShadeController somfy;
// In-memory checked-counter endpoint only; actual NVS/reservation ordering and
// fault injection are exercised by tools/test-explicit-counter.sh.
static bool fakePrimaryProjection(uint8_t id,PrimaryCounterReview &out) {
  out={};auto *target=somfy.getShadeById(id);if(!target)return false;
  out.shadeId=id;out.address=target->address;out.primary=target->lastRollingCode;
  const auto stored=learnedCodes.find(out.address);if(stored!=learnedCodes.end())out.stored={stored->second,true};
  unsigned slot=0;auto include=[&](bool match,uint16_t code){if(match){out.matching[slot/8]|=uint8_t(1u<<(slot%8));out.codes[slot]=code;}++slot;};
  for(const auto &shade:somfy.shades){include(shade.id!=255&&shade.address==out.address,shade.lastRollingCode);for(const auto &remote:shade.linkedRemotes)include(shade.id!=255&&remote.address==out.address,remote.lastRollingCode);}
  for(const auto &group:somfy.groups)include(group.id!=255&&group.address==out.address,group.lastRollingCode);
  assert(slot==PrimaryCounterReview::aliasSlots);return true;
}
CounterDomainState reviewPrimaryCounter(uint8_t id,PrimaryCounterReview &out) {
  ++primaryReads;out={};if(somfy.transceiver.quarantine!=CounterQuarantine::none)return CounterDomainState::quarantined;
  GitUpdateOwnerToken owner;assert(gitUpdate.acquireExternal(GitUpdateExternalOwner::counter,owner));
  const bool found=fakePrimaryProjection(id,out);assert(gitUpdate.releaseExternal(owner));
  return found?CounterDomainState::saved:CounterDomainState::missing;
}
bool primaryCounterReviewCurrent(uint8_t id,const PrimaryCounterReview &expected) {
  if(somfy.transceiver.quarantine!=CounterQuarantine::none)return false;
  PrimaryCounterReview actual;if(!fakePrimaryProjection(id,actual))return false;
  actual.stored=expected.stored;return actual.same(expected); // No retained-store comparison in the live-only predicate.
}
CounterDomainResult applyPrimaryCounterCorrection(uint8_t id,uint16_t code,const PrimaryCounterReview &expected,CounterCorrectionWitness &witness) {
  assert(!gitUpdate.busy());GitUpdateOwnerToken owner;assert(gitUpdate.acquireExternal(GitUpdateExternalOwner::counter,owner));
  PrimaryCounterReview actual;const bool matches=fakePrimaryProjection(id,actual)&&actual.same(expected);
  if(!matches||!witness.current()){assert(gitUpdate.releaseExternal(owner));return {CounterDomainState::stale};}
  ++primaryAttempts;bool applied=primaryInjected==CounterDomainState::saved;
  CounterDomainState state=primaryInjected;
  if(applied||state==CounterDomainState::nvsUncertain)++primaryWrites;
  if(state==CounterDomainState::nvsUncertain){somfy.transceiver.quarantine=CounterQuarantine::nvsUnknown;somfy.transceiver.blockedIdentity=expected.address;}
  if(applied){
    learnedCodes[expected.address]=code;
    for(auto &shade:somfy.shades)if(shade.id!=255){if(shade.address==expected.address)shade.lastRollingCode=code;for(auto &remote:shade.linkedRemotes)if(remote.address==expected.address)remote.lastRollingCode=code;}
    for(auto &group:somfy.groups)if(group.id!=255&&group.address==expected.address)group.lastRollingCode=code;
    ++somfy.configurationRevision;
    if(entitySaveState!=ModelCommitState::saved){state=entitySaveState==ModelCommitState::uncertain?CounterDomainState::modelUncertain:CounterDomainState::modelFailed;somfy.transceiver.quarantine=CounterQuarantine::filePending;somfy.transceiver.blockedIdentity=expected.address;}
  }
  assert(gitUpdate.releaseExternal(owner));
  if(applied&&afterPrimaryApply){auto callback=afterPrimaryApply;afterPrimaryApply=nullptr;callback();}
  return {state,applied,{entitySaveState,ModelCommitStage::write}};
}
static void snapshotFakeOrderFile() {
  for(uint8_t type=0;type<3;type++) {
    LocalUi::EntityAdmin::OrderIds slots{};bool normalize=false;
    assert(LocalUi::EntityAdmin::runtimeBackend().readOrder(LocalUi::EntityAdmin::Kind(type),fileOrder[type],slots,fileCounts[type],normalize));
    for(uint8_t i=0;i<fileCounts[type];i++)fileRanks[type][i]=type==0?somfy.rooms[slots[i]].sortOrder:type==1?somfy.shades[slots[i]].sortOrder:somfy.groups[slots[i]].sortOrder;
  }
  const auto *marker=somfy.getShadeById(4);filePairedMarker=marker && marker->paired;
  fileRepeaterAddress=0;for(const auto address:somfy.repeaters)if(address){fileRepeaterAddress=address;break;}
  fileKnown=true;
}
edit_result_t applyModelEdit(model_edit_t &edit) {
  if(edit.type==model_edit_type_t::link_remote) {
    assert(!gitUpdate.busy());GitUpdateOwnerToken owner;
    assert(gitUpdate.acquireExternal(GitUpdateExternalOwner::counter,owner));
    ++learnedAttempts;auto object=edit.doc.as<JsonObject>();assert(object.size()==3 && object["proto"].isNull() && object["bitLength"].isNull());
    const uint32_t address=object["remoteAddress"];const uint16_t code=object["rollingCode"];
    auto *shade=somfy.getShadeById(object["shadeId"].as<uint8_t>());assert(shade);
    CounterDomainState state=learnedInjected;
    if(somfy.transceiver.quarantine!=CounterQuarantine::none)state=CounterDomainState::quarantined;
    else if(learnedCodes.count(address) && code<learnedCodes[address])state=CounterDomainState::stale;
    decltype(&shade->linkedRemotes[0]) slot=nullptr;
    for(auto &remote:shade->linkedRemotes)if(remote.address==address){slot=&remote;break;}
    if(!slot)for(auto &remote:shade->linkedRemotes)if(!remote.address){slot=&remote;break;}
    if(!slot)state=CounterDomainState::full;
    const bool applied=state==CounterDomainState::saved;
    if(state==CounterDomainState::nvsUncertain){++learnedWrites;somfy.transceiver.quarantine=CounterQuarantine::nvsUnknown;somfy.transceiver.blockedIdentity=address;}
    if(applied){
      ++learnedLinks;++learnedWrites;++entityEdits;learnedCodes[address]=code;slot->address=address;slot->lastRollingCode=code;
      ++shade->configurationRevision;++somfy.configurationRevision;
      if(entitySaveState==ModelCommitState::saved){snapshotFakeOrderFile();learnedModelAddress=address;}
      else {state=entitySaveState==ModelCommitState::uncertain?CounterDomainState::modelUncertain:CounterDomainState::modelFailed;
        somfy.transceiver.quarantine=CounterQuarantine::filePending;somfy.transceiver.blockedIdentity=address;}
    }
    assert(gitUpdate.releaseExternal(owner));
    if(applied && afterLearnApply){auto callback=afterLearnApply;afterLearnApply=nullptr;callback();}
    edit_result_t result;result.applied=applied;result.ok=applied && entitySaveState==ModelCommitState::saved;
    result.id=applied?shade->id:0;result.persistence={entitySaveState,ModelCommitStage::write};
    result.hasCounterResult=true;result.counterState=state;return result;
  }
  if(edit.type==model_edit_type_t::link_to_group) {
    assert(gitUpdate.busy());const auto object=edit.doc.as<JsonObjectConst>();
    const auto id=object["groupId"].as<uint8_t>(),device=object["shadeId"].as<uint8_t>();
    auto *group=somfy.getGroupById(id);assert(group&&somfy.getShadeById(device));
    bool inserted=false;for(auto &member:group->linkedShades)if(!member){member=device;inserted=true;break;}assert(inserted);
    ++entityEdits;++somfy.configurationRevision;
    if(entitySaveState==ModelCommitState::saved)snapshotFakeOrderFile();
    edit_result_t result;result.applied=true;result.ok=entitySaveState==ModelCommitState::saved;result.id=id;result.persistence={entitySaveState,ModelCommitStage::write};return result;
  }
  if(edit.type==model_edit_type_t::set_paired || edit.type==model_edit_type_t::unlink_remote || edit.type==model_edit_type_t::unlink_repeater || edit.type==model_edit_type_t::unlink_from_group || edit.type==model_edit_type_t::link_repeater) {
    assert(gitUpdate.busy());auto object=edit.doc.as<JsonObject>();uint8_t id=0;
    if(edit.type==model_edit_type_t::set_paired){id=object["shadeId"];auto *shade=somfy.getShadeById(id);assert(shade);shade->paired=object["paired"].as<bool>();}
    else if(edit.type==model_edit_type_t::unlink_remote){id=object["shadeId"];auto *shade=somfy.getShadeById(id);assert(shade);for(auto &remote:shade->linkedRemotes)if(remote.address==object["remoteAddress"].as<uint32_t>())remote.address=0;}
    else if(edit.type==model_edit_type_t::link_repeater){assert(object.size()==1&&object["rollingCode"].isNull());bool added=false;
      GitUpdateOwnerToken competing;assert(!gitUpdate.acquireExternal(GitUpdateExternalOwner::counter,competing));
      for(auto &entry:somfy.repeaters)if(!entry&&!added){entry=object["remoteAddress"];added=true;}assert(added);}
    else if(edit.type==model_edit_type_t::unlink_repeater){const auto address=object["remoteAddress"].as<uint32_t>();for(auto &remote:somfy.repeaters)if(remote==address)remote=0;}
    else {id=object["groupId"];auto *group=somfy.getGroupById(id);assert(group);const auto shadeId=object["shadeId"].as<uint8_t>();
      if(edit.type==model_edit_type_t::link_to_group){bool added=false;for(auto &member:group->linkedShades)if(!member&&!added){member=shadeId;added=true;}assert(added);}
      else for(auto &member:group->linkedShades)if(member==shadeId)member=0;
      ++group->configurationRevision;
    }
    ++entityEdits;++somfy.configurationRevision;
    if(entitySaveState==ModelCommitState::saved)snapshotFakeOrderFile();else if(entitySaveState==ModelCommitState::uncertain)fileKnown=false;
    edit_result_t result;result.applied=true;result.ok=entitySaveState==ModelCommitState::saved;result.id=id;result.persistence={entitySaveState,ModelCommitStage::write};return result;
  }
  if(edit.type==model_edit_type_t::room_order || edit.type==model_edit_type_t::shade_order || edit.type==model_edit_type_t::group_order) {
    assert(edit.doc.is<JsonArray>());uint8_t rank=0;
    for(auto value:edit.doc.as<JsonArray>()) {
      const uint8_t id=value.as<uint8_t>();bool found=false;
      if(edit.type==model_edit_type_t::room_order){auto *item=somfy.getRoomById(id);if(item){item->sortOrder=rank;found=true;}}
      else if(edit.type==model_edit_type_t::shade_order){auto *item=somfy.getShadeById(id);if(item){item->sortOrder=rank;found=true;}}
      else {auto *item=somfy.getGroupById(id);if(item){item->sortOrder=rank;found=true;}}
      assert(found);++rank;
    }
    ++entityEdits;++somfy.configurationRevision;
    if(entitySaveState==ModelCommitState::saved)snapshotFakeOrderFile();
    else if(entitySaveState==ModelCommitState::uncertain)fileKnown=false;
    edit_result_t result;result.applied=true;result.ok=entitySaveState==ModelCommitState::saved;
    result.persistence={entitySaveState,ModelCommitStage::write};return result;
  }
  auto object=edit.doc.as<JsonObject>();
  const bool roomOp=edit.type==model_edit_type_t::add_room||edit.type==model_edit_type_t::save_room||edit.type==model_edit_type_t::delete_room;
  const bool shadeOp=edit.type==model_edit_type_t::add_shade||edit.type==model_edit_type_t::save_shade||edit.type==model_edit_type_t::delete_shade;
  const bool adding=edit.type==model_edit_type_t::add_room||edit.type==model_edit_type_t::add_shade||edit.type==model_edit_type_t::add_group;
  const bool deleting=edit.type==model_edit_type_t::delete_room||edit.type==model_edit_type_t::delete_shade||edit.type==model_edit_type_t::delete_group;
  const char *key=roomOp?"roomId":shadeOp?"shadeId":"groupId";
  uint8_t id=object[key]|0;
  auto common=[&](auto *remote) {
    if(!object["name"].isNull())snprintf(remote->name,sizeof(remote->name),"%s",object["name"].as<const char *>());
    if(!object["remoteAddress"].isNull())remote->address=object["remoteAddress"].as<uint32_t>();
    if(!object["roomId"].isNull())remote->roomId=object["roomId"].as<uint8_t>();
    if(!object["proto"].isNull())remote->proto=radio_proto(object["proto"].as<uint8_t>());
    if(!object["bitLength"].isNull())remote->bitLength=object["bitLength"].as<uint8_t>();
    if(!object["repeats"].isNull())remote->repeats=object["repeats"].as<uint8_t>();
    if(!object["flipCommands"].isNull())remote->flipCommands=object["flipCommands"].as<bool>();
    ++remote->configurationRevision;
  };
  if(roomOp) {
    auto *room=id?somfy.getRoomById(id):nullptr;
    if(adding) {
      id=somfy.getNextRoomId();assert(id);
      for(auto &candidate:somfy.rooms)if(!candidate.roomId){room=&candidate;room->roomId=id;room->sortOrder=somfy.getMaxRoomOrder()+1;break;}
    }
    assert(room);
    if(deleting)*room={};
    else {
      if(!object["name"].isNull())snprintf(room->name,sizeof(room->name),"%s",object["name"].as<const char *>());
      if(!object["sortOrder"].isNull())room->sortOrder=object["sortOrder"].as<int8_t>();
    }
  } else if(shadeOp) {
    auto *shade=id?somfy.getShadeById(id):nullptr;
    if(adding) {
      id=somfy.getNextShadeId();assert(id!=255);const int8_t order=somfy.getMaxShadeOrder()+1;
      for(auto &candidate:somfy.shades)if(candidate.id==255){shade=&candidate;*shade={};shade->id=id;shade->sortOrder=order;shade->paired=false;shade->lastRollingCode=0;shade->gpioUp=shade->gpioDown=shade->gpioMy=255;break;}
    }
    assert(shade);
    if(deleting)*shade={};
    else {
      common(shade);
      if(!object["shadeType"].isNull())shade->shadeType=shade_types(object["shadeType"].as<uint8_t>());
      if(!object["tiltType"].isNull())shade->tiltType=tilt_types(object["tiltType"].as<uint8_t>());
      if(!object["upTime"].isNull())shade->upTime=object["upTime"].as<uint32_t>();
      if(!object["downTime"].isNull())shade->downTime=object["downTime"].as<uint32_t>();
      if(!object["tiltTime"].isNull())shade->tiltTime=object["tiltTime"].as<uint32_t>();
      if(!object["stepSize"].isNull())shade->stepSize=object["stepSize"].as<uint16_t>();
      if(!object["flipPosition"].isNull())shade->flipPosition=object["flipPosition"].as<bool>();
      if(!object["sunSensor"].isNull())shade->sunSensor=object["sunSensor"].as<bool>();
      if(!object["light"].isNull())shade->light=object["light"].as<bool>();
      if(!object["simMy"].isNull())shade->simMy=object["simMy"].as<bool>();
      for(auto field:{"gpioUp","gpioDown","gpioMy"})if(!object[field].isNull()) {
        auto &pin=!strcmp(field,"gpioUp")?shade->gpioUp:!strcmp(field,"gpioDown")?shade->gpioDown:shade->gpioMy;
        pin=object[field].as<uint8_t>();
      }
      if(!object["gpioLLTrigger"].isNull())shade->gpioFlags=object["gpioLLTrigger"].as<bool>()?1:0;
    }
  } else {
    assert(edit.type==model_edit_type_t::add_group||edit.type==model_edit_type_t::save_group||edit.type==model_edit_type_t::delete_group);
    auto *group=id?somfy.getGroupById(id):nullptr;
    if(adding) {
      id=somfy.getNextGroupId();assert(id!=255);const int8_t order=somfy.getMaxGroupOrder()+1;
      for(auto &candidate:somfy.groups)if(candidate.id==255){group=&candidate;*group={};group->id=id;group->sortOrder=order;break;}
    }
    assert(group);if(deleting)*group={};else common(group);
  }
  ++entityEdits;++somfy.configurationRevision;
  if(entitySaveState==ModelCommitState::saved)snapshotFakeOrderFile();
  else if(entitySaveState==ModelCommitState::uncertain)fileKnown=false;
  edit_result_t result;result.applied=true;result.ok=entitySaveState==ModelCommitState::saved;
  result.id=id;result.persistence={entitySaveState,ModelCommitStage::none};return result;
}
ModelCommitReport applyModelCommit(const proto_reply_to_t &,ModelCommitMode mode) {
  if(mode==ModelCommitMode::counterFileRecovery){
    if(somfy.transceiver.quarantine==CounterQuarantine::nvsUnknown){ModelCommitReport refused;refused.refusal=RestartReason::counterUncertain;return refused;}
    GitUpdateOwnerToken owner;
    if(somfy.transceiver.quarantine==CounterQuarantine::filePending){assert(!gitUpdate.busy());assert(gitUpdate.acquireExternal(GitUpdateExternalOwner::counter,owner));}
    else assert(gitUpdate.busy());
    ++learnedRetries;++entityCommits;
    if(entityCommitState==ModelCommitState::saved){snapshotFakeOrderFile();
      for(auto &remote:somfy.getShadeById(4)->linkedRemotes)if(remote.address)learnedModelAddress=remote.address;
      somfy.transceiver.quarantine=CounterQuarantine::none;somfy.transceiver.blockedIdentity=0;
    }else if(entityCommitState==ModelCommitState::uncertain)fileKnown=false;
    if(owner.generation)assert(gitUpdate.releaseExternal(owner));
    return {{entityCommitState,ModelCommitStage::write}};
  }
  ++entityCommits;
  if(entityCommitState==ModelCommitState::saved)snapshotFakeOrderFile();
  else if(entityCommitState==ModelCommitState::uncertain)fileKnown=false;
  return {{entityCommitState,ModelCommitStage::write}};
}
#include "../../src/LocalEntityRuntime.cpp"
static std::vector<shade_request_t> submitted;
static uint32_t lastOwnedTicket=1000;
static std::map<uint32_t,uint32_t> ownedAddresses;
static std::map<uint32_t,somfy_commands> ownedCommands;
struct RecordingRequests {shade_request_result_t run(const shade_request_t &r){
  if(r.command==somfy_commands::Prog||r.type==shade_request_type_t::shade_sensor||r.type==shade_request_type_t::group_sensor){assert(gitUpdate.busy());GitUpdateOwnerToken competitor;assert(!gitUpdate.acquireExternal(GitUpdateExternalOwner::manual,competitor));}
  submitted.push_back(r);return shade_request_result_t::done;
}
shade_request_result_t runOwnedRepeat(const shade_request_t &r,owned_repeat_receipt_t &receipt){
  const auto result=run(r);receipt.ticket=result==shade_request_result_t::done?++lastOwnedTicket:0;
  if(receipt.ticket){ownedAddresses[receipt.ticket]=somfy.getShadeById(r.id)->address;ownedCommands[receipt.ticket]=r.command;}return result;
}}shadeRequests;
#define ShadeRequests RecordingRequests
#include "../../src/LocalAdvancedRuntime.cpp"
#undef ShadeRequests
static LocalUi::Authorization authorization;
static LocalUi::View lastQuery;
static unsigned bands=0,presentations=0,locks=0;
static bool busBusy=false,captureProtected=true;
static unsigned displayStride=22;
static bool realisticDisplay=false,captureComplete=false;static unsigned spiRemainderUs=0,spiTickUs=0,maxSpiTickUs=0,spiBandCostUs=1500,dirtyWrites=0,maxDirtyWrites=0;
static std::vector<uint8_t> attemptedBands;static int writesBeforeBusy=-1;
#include "../ui-adapter/LoopHarness.h"
#define ShadeRequests RecordingRequests
#define snapshotModel adapterSnapshotModel
#define markControlPresented adapterMarkControlPresented
#include "../../src/LocalUiAdapter.cpp"
#undef markControlPresented
#undef snapshotModel
#undef ShadeRequests
namespace LocalUi {namespace RadioScan {
class Driver final:public ::FrequencyScan::Backend {
public:
 unsigned starts=0,stops=0;
 bool current(const ::FrequencyScan::Source &source)override{const auto auth=pollAuthorization();return auth.allowed&&auth.generation==source.session&&auth.configurationGeneration==source.configuration&&source.radioRevision==settingsRevision;}
 bool acquire(GitUpdateOwnerToken &owner)override{return somfy.transceiver.ready&&somfy.transceiver.quiescent&&!loopQueue.pending()&&modelIdleForAdministration(somfy)&&gitUpdate.acquireExternal(GitUpdateExternalOwner::configuration,owner);}
 bool owns(const GitUpdateOwnerToken &owner)override{return gitUpdate.ownsExternal(owner);}
 bool begin()override{++starts;loopScanActive=true;return true;}
 void stop()override{++stops;loopScanActive=false;}
 bool active()override{return loopScanActive;}
 bool sample(::FrequencyScan::Sample &out)override{out={433.42f,433.43f,-60,-55};return true;}
 bool release(const GitUpdateOwnerToken &owner)override{return gitUpdate.releaseExternal(owner);}
} driver;
static ::FrequencyScan::Session scanSession(driver,42);
class UiBackend final:public Backend {
public:
 SecuritySnapshot authorization()override{return pollAuthorization();}
 void activity()override{authorizationActivity();}
 ::FrequencyScan::Source capture()override{const auto auth=authorization();return {::FrequencyScan::SourceKind::lcd,0,auth.generation,auth.configurationGeneration,1,settingsRevision};}
 bool begin(const ::FrequencyScan::Source &source,FrequencyScanToken &token)override{return scanSession.start(source,authTestNow,token);}
 bool stop(const ::FrequencyScan::Source &source,const FrequencyScanToken &token)override{return scanSession.stop(token,source,authTestNow);}
 ::FrequencyScan::Snapshot status()override{return scanSession.snapshot();}
 bool peak(const ::FrequencyScan::Source &source,const FrequencyScanToken &token,float &frequency)override{return scanSession.peak(token,source,frequency);}
};
Backend &runtimeBackend(){static UiBackend backend;return backend;}
}}
LocalUi::Model LocalUi::snapshotModel(bool running,const View &view){lastQuery=view;return adapterSnapshotModel(running,view);}
void LocalUi::markControlPresented(EntityRef ref,uint64_t token,bool allowed,uint32_t generation){++presentations;adapterMarkControlPresented(ref,token,allowed,generation);}
LocalUi::SecuritySnapshot LocalUi::pollAuthorization(){
  const bool allowed=authorization.allowed(testNow);
  return {authorization.configuredMode(),allowed,authorization.stopGraceAllowed(),authorization.generation(),authorization.configurationGeneration(),authorization.retryAfter(testNow),authorization.expiredSessionGeneration()};
}
static unsigned authActivities=0;
void LocalUi::authorizationActivity(){++authActivities;authorization.activity(testNow);}
void LocalUi::lockAuthorization(){++locks;authorization.lock();}
LocalUi::AuthResult LocalUi::unlockAuthorization(Credentials &given){auto result=authorization.unlock(given,testNow);given.clear();return result;}
namespace LocalUi {namespace Updates {
struct PreviewBackend final:Backend {
  GitUpdateSnapshot value;GitUpdateBootStatus bootValue;
  unsigned checks=0,installs=0,cancels=0;uint64_t next=400,installAcceptedAt=0;uint32_t installConfiguration=0;
  PreviewBackend(){value.supported=true;value.phase=GitUpdatePhase::idle;value.reason=GitUpdateReason::none;strcpy(value.installed,"v4.0.2");}
  SecuritySnapshot authorization()override{return pollAuthorization();}
  void activity()override{authorizationActivity();}
  GitUpdateSnapshot status()override{
    auto out=value;
    if(out.installContinuation){
      const bool sameConfig=pollAuthorization().configurationGeneration==installConfiguration;
      out.installContinuation=sameConfig;
      out.installRemainingMs=sameConfig && testNow>=installAcceptedAt && testNow-installAcceptedAt<1800000?1800000-(testNow-installAcceptedAt):0;
    }
    return out;
  }
  GitUpdateBootStatus boot()override{return bootValue;}
  GitUpdateRequest check(uint32_t session,uint32_t security,uint64_t &operation)override{
    const auto auth=pollAuthorization();assert(auth.allowed && auth.generation==session && auth.configurationGeneration==security);
    ++checks;value.installContinuation=false;operation=value.operation=++next;value.busy=value.cancellable=true;value.canInstall=false;value.phase=GitUpdatePhase::checking;return GitUpdateRequest::accepted;
  }
  GitUpdateRequest install(uint64_t candidate,uint32_t session,uint32_t security,uint64_t &operation)override{
    const auto auth=pollAuthorization();assert(auth.allowed && auth.generation==session && auth.configurationGeneration==security);
    if(candidate!=value.candidate.generation)return GitUpdateRequest::invalidCandidate;
    ++installs;installAcceptedAt=testNow;installConfiguration=auth.configurationGeneration;value.installContinuation=true;operation=value.operation=++next;value.busy=value.cancellable=true;value.canInstall=false;value.phase=GitUpdatePhase::downloading;return GitUpdateRequest::accepted;
  }
  bool cancel(uint64_t operation,uint32_t,uint32_t)override{
    if(operation!=value.operation || !value.cancellable)return false;
    ++cancels;value.installContinuation=false;value.busy=value.cancellable=false;value.phase=GitUpdatePhase::cancelled;value.reason=GitUpdateReason::cancelled;return true;
  }
} previewBackend;
Backend &runtimeBackend(){return previewBackend;}
}}
void LocalUi::setCaptureProtected(bool value){captureProtected=value;}
void LocalUi::capturePoll(const uint16_t *,bool complete,uint8_t,uint32_t){if(realisticDisplay)captureComplete=complete;}
static bool send(uint8_t y,uint8_t rows,const uint8_t *data,size_t bytes){
  assert(data && rows && rows<=4 && y+rows<=170 && bytes==rows*640u);
  if(realisticDisplay)attemptedBands.push_back(y);
  if(busBusy||(realisticDisplay&&writesBeforeBusy==0))return false;
  if(realisticDisplay){if(writesBeforeBusy>0)--writesBeforeBusy;++dirtyWrites;spiTickUs+=spiBandCostUs;spiRemainderUs+=spiBandCostUs;testNow+=spiRemainderUs/1000;spiRemainderUs%=1000;}
  ++bands;return true;
}
static uint8_t lastLight=0;
static void light(uint8_t value){lastLight=value;assert(value==0 || value==4 || value==32);}
static void saveFrame(const char *directory,const char *name) {
  if(!directory) return;
  char path[512];snprintf(path,sizeof(path),"%s/%s.ppm",directory,name);
  FILE *file=fopen(path,"wb");assert(file && allocated);
  fprintf(file,"P6\n320 170\n255\n");
  const auto *pixels=static_cast<const uint16_t *>(allocated);
  for(size_t i=0;i<320*170;i++) {
    const auto p=pixels[i];const uint8_t rgb[]={uint8_t((p>>11)*255/31),uint8_t(((p>>5)&63)*255/63),uint8_t((p&31)*255/31)};
    fwrite(rgb,1,3,file);
  }
  fclose(file);
}
int main(int argc,char **argv){
  const char *preview=argc>1?argv[1]:nullptr;
  using namespace LocalUi;
  authorization.configure(AuthMode::none,"","");loopQueue.begin();
  auto &one=somfy.shades[13];one.id=7;one.address=700;one.shadeType=shade_types::shutter;one.bitLength=56;one.idle=false;strcpy(one.name,"Bedroom");
  auto &two=somfy.shades[31];two.id=22;two.address=2200;two.shadeType=shade_types::shutter;two.bitLength=56;two.idle=false;strcpy(two.name,"Bathroom");
  testPins[4]=testPins[5]=testPins[6]=1;
  StatusDisplay::Snapshot status;status.running=true;status.link=StatusDisplay::Link::connected;
  status.web=StatusDisplay::Web::listening;status.radio=StatusDisplay::Radio::ready;
  Runtime ui;ui.begin();assert(ui.failure()==GraphicsFailure::none && allocated && !ui.healthy());
  Runtime *active=&ui;
  int ab=3;
  bool holdEncoder=false;
  auto tick=[&](bool pressed=false,unsigned dt=50){spiTickUs=dirtyWrites=0;const auto previousFrame=RuntimeTestAccess::frameRevision(*active);testNow+=dt;authTestNow=testNow;RadioScan::scanSession.poll(testNow);loopQueue.drain();active->tick(testNow,status,pressed||holdEncoder,send,light);if(realisticDisplay){if(RuntimeTestAccess::frameRevision(*active)!=previousFrame)testNow+=7;maxSpiTickUs=std::max(maxSpiTickUs,spiTickUs);maxDirtyWrites=std::max(maxDirtyWrites,dirtyWrites);assert(spiTickUs<=10000 && dirtyWrites<=8);}};
  auto idle=[&](unsigned duration=140){for(unsigned t=0;t<duration;t+=2)tick(false,2);};
  auto rotate=[&](int direction){
    const int middle=direction>0?(ab==3?1:2):(ab==3?2:1);
    testPins[4]=middle>>1;testPins[5]=middle&1;tick(false,realisticDisplay?displayStride:50);
    ab=ab==3?0:3;testPins[4]=ab>>1;testPins[5]=ab&1;tick(false,realisticDisplay?displayStride:50);
  };
  auto click=[&]{tick(true);tick(true);tick(false);tick(false);tick(false,2);};
  auto back=[&]{testPins[6]=0;tick();tick();testPins[6]=1;tick();tick();};
  auto releaseHold=[&]{holdEncoder=false;tick(false);tick(false);tick(false,2);};
  auto beginHold=[&]{const auto before=submitted.size();holdEncoder=true;tick(true);tick(true);tick(true,749);assert(submitted.size()==before);tick(true,51);tick(true,2);};
  auto homeQuick=[&]{
    for(unsigned i=0;i<16 && RuntimeTestAccess::view(*active).screen!=Screen::quickHome;++i){
      if(RuntimeTestAccess::view(*active).screen==Screen::status){click();idle();}else{back();idle();}
    }
    assert(RuntimeTestAccess::view(*active).screen==Screen::quickHome);
  };
  auto selectMenu=[&](unsigned row){
    for(unsigned i=0;i<16 && RuntimeTestAccess::view(*active).selected!=row;++i){rotate(RuntimeTestAccess::view(*active).selected<row?1:-1);idle();}
    assert(RuntimeTestAccess::view(*active).selected==row);
  };
  auto openDevices=[&]{homeQuick();selectMenu(4);click();idle();assert(RuntimeTestAccess::view(*active).screen==Screen::catalogue);};
  auto openController=[&]{homeQuick();selectMenu(7);click();idle();assert(RuntimeTestAccess::view(*active).screen==Screen::controllerMenu);};
  auto openConfigure=[&](unsigned row){openController();selectMenu(0);click();idle();assert(RuntimeTestAccess::view(*active).screen==Screen::configureMenu);selectMenu(row);};
  auto openNetworkMenu=[&]{openConfigure(2);click();idle();assert(RuntimeTestAccess::view(*active).screen==Screen::networkMenu);};
  auto selectSystem=[&](unsigned row){for(unsigned i=0;i<8 && RuntimeTestAccess::system(*active).selected!=row;++i){rotate(RuntimeTestAccess::system(*active).selected<row?1:-1);idle();}assert(RuntimeTestAccess::system(*active).selected==row);};
  auto openMaintenance=[&]{openController();selectMenu(1);click();idle();assert(!strcmp(RuntimeTestAccess::system(*active).title,"Maintenance"));};
  auto openDiagnostics=[&]{openController();selectMenu(4);click();idle();assert(!strcmp(RuntimeTestAccess::system(*active).title,"Diagnostics"));};
  holdEncoder=true; // Physical encoder already held at initialization cannot arm anything.
  busBusy=true;idle(20);assert(bands==0 && captureProtected && !ui.healthy());
  busBusy=false;idle();assert(bands==43 && !captureProtected && ui.healthy());
  assert(lastQuery.screen==Screen::status && submitted.empty());releaseHold();idle();
  auto unchanged=bands;idle();assert(bands==unchanged);
#ifdef AUDIT_UI_RUNTIME_HOST_TEST
  // Inject through the real tick/debounce/frame path, never through a form.
  AuditConsole::inputActive=true;
  tick(true);tick(true);tick(false);tick(false);idle();
  assert(RuntimeTestAccess::view(ui).screen==Screen::status); // injected release overrides physical hold
  AuditConsole::injectedInput.turn=1;tick();idle();
  assert(RuntimeTestAccess::view(ui).screen==Screen::quickHome);
  assert(AuditConsole::stateReports && AuditConsole::reportedScreen==uint8_t(Screen::quickHome) && AuditConsole::reportedVisible);
  while(RuntimeTestAccess::view(ui).selected!=7){AuditConsole::injectedInput.turn=1;tick();idle();}
  AuditConsole::injectedInput.pressed=true;tick();AuditConsole::injectedInput.pressed=false;tick();idle();
  assert(RuntimeTestAccess::view(ui).screen==Screen::quickHome); // short raw pulse cannot bypass debounce
  auto auditClick=[&]{AuditConsole::injectedInput.pressed=true;tick();tick();AuditConsole::injectedInput.pressed=false;tick();tick();idle();};
  auto auditBack=[&]{AuditConsole::injectedInput.back=true;tick();tick();AuditConsole::injectedInput.back=false;tick();tick();idle();};
  auditClick();assert(RuntimeTestAccess::view(ui).screen==Screen::controllerMenu);
  assert(AuditConsole::reportedSelected==RuntimeTestAccess::view(ui).selected);
  AuditConsole::inputActive=false;back();idle();back();idle();
  assert(RuntimeTestAccess::view(ui).screen==Screen::status);
  rotate(1);idle();assert(RuntimeTestAccess::view(ui).screen==Screen::quickHome);back();idle();
  assert(RuntimeTestAccess::view(ui).screen==Screen::status); // ordinary physical encoder/back still work
#endif
  // Measured hardware costs:1.5ms/4-row SPI plus7ms full paint. Model
  // real22/32ms loop gaps; sparse selection must not cost43 loop passes.
  // Measure remaining completion time AFTER the input/first retry tick. That
  // tick may already paint/flush a bounded batch; this is not total key latency.
  // At32ms,20 dirty selection bands require about118.5ms remaining under the
  // SPI budget, so use125ms there and100ms at22ms; these are modeled bounds.
  auto settleDisplay=[&](unsigned cadence,unsigned bound,const char *label){
    const auto started=testNow;
    for(unsigned i=0;i<100&&!RuntimeTestAccess::fullyPresented(ui);++i){
      assert(!captureComplete);
      tick(false,cadence);
    }
    printf("Realistic LCD %s cadence%ums: remaining%ums, maxSPI%uus maxDirty%u\n",label,cadence,unsigned(testNow-started),maxSpiTickUs,maxDirtyWrites);fflush(stdout);
    assert(RuntimeTestAccess::fullyPresented(ui) && testNow-started<=bound);
    tick(false,cadence);assert(captureComplete);
  };
  for(unsigned cadence:{22u,32u}){
    const auto noRequests=submitted.size();displayStride=cadence;realisticDisplay=true;spiBandCostUs=1500;spiRemainderUs=0;
    maxSpiTickUs=maxDirtyWrites=0;rotate(1);assert(RuntimeTestAccess::view(ui).screen==Screen::quickHome);
    settleDisplay(cadence,cadence==22?300:400,"full menu");
    const auto beforeStep=RuntimeTestAccess::view(ui).selected;
    rotate(1);assert(RuntimeTestAccess::view(ui).selected!=beforeStep);
    settleDisplay(cadence,cadence==22?100:125,"selection step");
    const auto presented=RuntimeTestAccess::presentedSelection(ui);
    busBusy=true;rotate(-1);assert(!RuntimeTestAccess::fullyPresented(ui) && !captureComplete);
    const auto failed=RuntimeTestAccess::nextBand(ui);assert(!attemptedBands.empty()&&attemptedBands.back()==failed*4);
    for(unsigned i=0;i<3;++i){tick(false,cadence);assert(RuntimeTestAccess::nextBand(ui)==failed && !captureComplete && RuntimeTestAccess::presentedSelection(ui)==presented);}
    const auto retryAt=attemptedBands.size();busBusy=false;tick(false,cadence);
    assert(attemptedBands.size()>retryAt && attemptedBands[retryAt]==failed*4); // Exact failed band retried first.
    settleDisplay(cadence,cadence==22?100:125,"bus retry selection");
    assert(submitted.size()==noRequests);realisticDisplay=false;back();idle();assert(RuntimeTestAccess::view(ui).screen==Screen::status);
  }
  // A slower2ms callback must still yield within10ms(one bounded overshoot).
  // Zero-time callbacks independently prove the dirty-write count cap.
  for(unsigned cost:{2000u,0u}){
    realisticDisplay=true;spiBandCostUs=cost;spiRemainderUs=0;maxSpiTickUs=maxDirtyWrites=0;
    rotate(1);settleDisplay(32,400,cost?"2ms callback":"zero-cost count cap");
    assert(maxSpiTickUs<=10000 && maxDirtyWrites<=8);
    realisticDisplay=false;back();idle();assert(RuntimeTestAccess::view(ui).screen==Screen::status);
  }
  realisticDisplay=true;spiBandCostUs=1500;spiRemainderUs=0;writesBeforeBusy=2;
  const auto beforePartial=bands;rotate(1);
  assert(bands==beforePartial+2 && !RuntimeTestAccess::fullyPresented(ui) && !captureComplete);
  assert(RuntimeTestAccess::presentedScreen(ui)==Screen::status); // Two successful bands cannot bless a new menu.
  const auto blockedBand=RuntimeTestAccess::nextBand(ui);
  for(unsigned i=0;i<3;++i){tick(false,32);assert(RuntimeTestAccess::nextBand(ui)==blockedBand && !captureComplete && RuntimeTestAccess::presentedScreen(ui)==Screen::status);}
  const auto retryAfterPartial=attemptedBands.size();writesBeforeBusy=-1;tick(false,32);
  assert(attemptedBands.size()>retryAfterPartial && attemptedBands[retryAfterPartial]==blockedBand*4);
  settleDisplay(32,400,"busy after two bands");
  realisticDisplay=false;back();idle();assert(RuntimeTestAccess::view(ui).screen==Screen::status);
  unchanged=bands;
  // Health is reachable through the actual held Status input, not a test-only page.
  status.health={true,true,true,65536,32768,16384,6u*1024*1024,400,12345};
  ++status.uptimeSeconds;idle();assert(bands==unchanged); // other pages retain minute cadence
  tick(true);tick(true);tick(true,850);tick(false);tick(false);idle();
  assert(lastQuery.screen==Screen::details);
  for(unsigned i=0;i<detailsRowCount;i++){rotate(1);idle();}
  assert(RuntimeTestAccess::view(ui).detailsOffset==detailsRowCount-detailsVisibleRows);
  saveFrame(preview,"runtime-health");const auto healthBands=bands;
  status.health.internalFreeBytes+=1024;++status.uptimeSeconds;idle();
  assert(bands>healthBands && submitted.empty());
  saveFrame(preview,"runtime-health-updated");back();idle();assert(lastQuery.screen==Screen::status);
  openDevices(); // Quick -> Browse -> Devices
  assert(lastQuery.screen==Screen::catalogue);
  busBusy=true;click();idle(10);assert(lastQuery.screen==Screen::control);
  assert(presentations==0);click();assert(submitted.empty()); // never-presented STOP is blocked
  busBusy=false;idle();assert(presentations>0);
  status.recovery=BootRecoveryNotice::movement;idle();assert(lastQuery.screen==Screen::control && lastQuery.entity.id==7);
  saveFrame(preview,"runtime-rollback-waiting-control");click();
  assert(submitted.size()==1 && submitted.back().id==7 && submitted.back().command==somfy_commands::My);
  status.recovery=BootRecoveryNotice::none;
  // Retain an already-visible STOP across estimate/action repainting, but do
  // not admit a never-presented UP highlight while SPI is blocked.
  busBusy=true;++one.value.currentPos;rotate(-1);click();assert(submitted.size()==1);
  click();assert(submitted.size()==2 && submitted.back().command==somfy_commands::My);
  // A different target cannot inherit the previous target's sticky STOP.
  back();rotate(1);click();assert(lastQuery.screen==Screen::catalogue && submitted.size()==2);
  busBusy=false;idle();busBusy=true;click();idle(10);click();assert(submitted.size()==2);
  assert(lastQuery.entity.id==22);
  // Context changes while locally authorized require a new presented selection.
  ++two.address;++two.configurationRevision;++somfy.configurationRevision;idle(10);click();assert(submitted.size()==2);
  busBusy=false;back();idle();rotate(-1);idle();click();idle();
  assert(lastQuery.screen==Screen::control && lastQuery.entity.id==7);
  authorization.configure(AuthMode::pin,"","1111");click();idle();
  assert(lastQuery.screen==Screen::unlock && captureProtected && submitted.size()==2);
  busBusy=true;back();assert(captureProtected);busBusy=false;idle();assert(!captureProtected);
  click();idle();assert(lastQuery.screen==Screen::unlock && captureProtected);
  busBusy=true;rotate(1);click();
  assert(lastQuery.screen==Screen::unlock && RuntimeTestAccess::view(ui).credentialLength==0 && !authorization.allowed(testNow));
  busBusy=false;idle();for(unsigned i=0;i<4;i++)click();
  assert(RuntimeTestAccess::view(ui).credentialLength==4 && !authorization.allowed(testNow));
  busBusy=true;
  while(RuntimeTestAccess::view(*active).credentialKeyboard.selected!=RotaryKeyboard::actionOffset(RuntimeTestAccess::view(*active).credentialKeyboard)+5){rotate(1);idle();}
  click();assert(lastQuery.screen==Screen::unlock && RuntimeTestAccess::view(ui).credentialLength==4 && !authorization.allowed(testNow) && submitted.size()==2);
  busBusy=false;idle();click();idle();assert(lastQuery.screen==Screen::control && !captureProtected);
  assert(submitted.size()==2); // unlock does not replay the initial command
  // Existing authorized motion retains only the bounded 30 s expiry lease.
  testNow+=120000;ui.tick(testNow,status,false,send,light);assert(submitted.size()==2);
  click();assert(submitted.size()==3 && submitted.back().id==7);
  testNow+=31000;ui.tick(testNow,status,false,send,light);click();
  assert(submitted.size()==3);idle();assert(lastQuery.screen==Screen::unlock);
  back();one.idle=two.idle=true;idle();const auto beforeSleep=locks;
  testNow+=21000;ui.tick(testNow,status,false,send,light);assert(locks==beforeSleep+1);
  testNow+=100;ui.tick(testNow,status,false,send,light);assert(locks==beforeSleep+1);
  // Numeric movement requires the exact Apply choice to finish reaching LCD.
  authorization.configure(AuthMode::none,"","");one.idle=false;idle();
  openDevices();click();idle();
  assert(lastQuery.screen==Screen::control && lastQuery.entity.id==7);
  rotate(1);idle();rotate(1);idle();click();idle();
  assert(lastQuery.screen==Screen::actions);click();idle();
  assert(lastQuery.screen==Screen::value);const int expected=lastQuery.draftValue+1;
  rotate(1);idle();assert(lastQuery.draftValue==expected);
  const auto beforeDraft=submitted.size();click();idle();assert(lastQuery.valueChoice==2);
  busBusy=true;rotate(-1);click();assert(submitted.size()==beforeDraft);
  busBusy=false;idle();click();assert(submitted.size()==beforeDraft+1);
  assert(submitted.back().target==expected && submitted.back().type==shade_request_type_t::shade_command);
  // Explicit repeat confirmation must itself reach the physical GFX frame.
  const auto beforeRepeats=submitted.size();rotate(1);idle();rotate(1);idle();click();idle();
  assert(lastQuery.screen==Screen::actions);
  for(unsigned i=0;i<20 && lastQuery.action!=repeatsNavigation;i++){rotate(1);idle();}
  click();idle();assert(lastQuery.screen==Screen::repeatActions && lastQuery.action==Action::up);
  click();idle();assert(lastQuery.screen==Screen::repeatValue);rotate(1);idle();
  assert(lastQuery.draftValue==2 && submitted.size()==beforeRepeats);
  click();idle();assert(lastQuery.valueChoice==2);
  busBusy=true;rotate(-1);click();assert(submitted.size()==beforeRepeats);
  beginHold();assert(submitted.size()==beforeRepeats); // Initial press did not see Apply.
  busBusy=false;idle();tick(true,900);assert(submitted.size()==beforeRepeats); // Presentation recovery cannot arm a held cycle.
  releaseHold();idle();
  click();idle();assert(submitted.size()==beforeRepeats); // A tap on Apply cannot start held RF.
  saveFrame(preview,"runtime-repeat-hold-review");
  beginHold();idle();assert(submitted.size()==beforeRepeats+1);
  assert(submitted.back().repeat==2 && submitted.back().id==7 && submitted.back().command==somfy_commands::Up && submitted.back().type==shade_request_type_t::shade_command);
  assert(lastQuery.screen==Screen::control && lastQuery.action==Action::stop);
  assert(localControlPending()); // Admitted repeat still owns its exact RF job.
  saveFrame(preview,"runtime-repeat-active");
  auto txEvent=[&](uint32_t ticket,tx_event_t type){somfy_tx_event_t event;
    event.ticket=ticket;event.type=type;event.cmd=ownedCommands.count(ticket)?ownedCommands[ticket]:somfy_commands::Up;event.remoteAddress=ownedAddresses.count(ticket)?ownedAddresses[ticket]:700;event.at=testNow;
    localControlTxEvent(event);
  };
  const auto firstOwned=lastOwnedTicket;
  txEvent(firstOwned+10000,tx_event_t::finished);assert(localControlPending());
  back();assert(localControlPending() && somfy.transceiver.cancelledTicket==firstOwned);
  assert(submitted.size()==beforeRepeats+1); // Cancellation is not a MY/STOP request.
  txEvent(firstOwned,tx_event_t::aborted);assert(!localControlPending());releaseHold();idle();click();idle();
  assert(lastQuery.screen==Screen::control && lastQuery.action==Action::stop);
  auto controlFromStatus=[&]{
    for(unsigned i=0;i<10&&RuntimeTestAccess::view(ui).screen!=Screen::status;++i){back();idle();}
    assert(RuntimeTestAccess::view(ui).screen==Screen::status);
    openDevices();click();idle();
    assert(lastQuery.screen==Screen::control && lastQuery.entity.id==7);
  };
  auto admitRepeatedUp=[&](unsigned count,bool onAir=true,Action desired=Action::up,unsigned step=0,bool arm=true){
    assert(lastQuery.screen==Screen::control && lastQuery.action==Action::stop);
    const auto before=submitted.size();somfy.transceiver.cancelledTicket=0;
    rotate(1);idle();rotate(1);idle();click();idle();assert(lastQuery.screen==Screen::actions);
    for(unsigned i=0;i<20&&lastQuery.action!=repeatsNavigation;++i){rotate(1);idle();}
    click();idle();assert(lastQuery.screen==Screen::repeatActions && lastQuery.action==Action::up);
    for(unsigned i=0;i<20&&lastQuery.action!=desired;++i){rotate(1);idle();}
    assert(lastQuery.action==desired);click();idle();
    if(step){assert(lastQuery.screen==Screen::value);
      for(unsigned i=0;i<130&&unsigned(lastQuery.draftValue)<step;++i){rotate(1);idle();}
      assert(unsigned(lastQuery.draftValue)==step);click();idle();assert(lastQuery.valueChoice==2);
      rotate(-1);idle();click();idle();}
    assert(lastQuery.screen==Screen::repeatValue);
    for(unsigned i=0;i<130&&unsigned(lastQuery.draftValue)<count;++i){rotate(1);idle();}
    assert(unsigned(lastQuery.draftValue)==count);click();idle();assert(lastQuery.valueChoice==2);
    rotate(-1);idle();if(!arm)return uint32_t(0);beginHold();idle();
    assert(submitted.size()==before+1 && submitted.back().command==(desired==Action::stepUp?somfy_commands::StepUp:somfy_commands::Up) && unsigned(submitted.back().repeat)==count && submitted.back().stepSize==step && submitted.back().id==7);
    assert(localControlPending() && lastQuery.screen==Screen::control && lastQuery.action==Action::stop && RuntimeTestAccess::view(ui).repeatHeld);
    if(onAir)txEvent(lastOwnedTicket,tx_event_t::started);return lastOwnedTicket;
  };
  const bool beforeWakeOneIdle=one.idle,beforeWakeTwoIdle=two.idle;
  one.idle=two.idle=true;idle();
  admitRepeatedUp(2,true,Action::up,0,false);const auto beforeWakeHold=submitted.size();
  tick(false,21000);assert(RuntimeTestAccess::view(ui).screen==Screen::status);
  beginHold();idle();assert(submitted.size()==beforeWakeHold && lastQuery.screen==Screen::quickHome);
  releaseHold();idle();one.idle=beforeWakeOneIdle;two.idle=beforeWakeTwoIdle;idle();controlFromStatus(); // Wake-only input never revives the expired repeat review.
  for(unsigned onAir=0;onAir<2;++onAir){
    const auto ticket=admitRepeatedUp(2,onAir);const auto requests=submitted.size();
    releaseHold();idle();assert(localControlPending() && somfy.transceiver.cancelledTicket==ticket);
    assert(submitted.size()==requests);saveFrame(preview,onAir?"runtime-repeat-release-ending":"runtime-repeat-queued-ending");
    txEvent(ticket,onAir?tx_event_t::aborted:tx_event_t::dropped);idle();assert(!localControlPending());controlFromStatus();
  }
  {const auto ticket=admitRepeatedUp(2);const auto requests=submitted.size();
    txEvent(ticket,tx_event_t::finished);idle(1000);assert(!localControlPending() && submitted.size()==requests);
    assert(!RuntimeTestAccess::view(ui).repeatHeld && RuntimeTestAccess::view(ui).notice==Notice::done);
    saveFrame(preview,"runtime-repeat-held-complete");idle(800);assert(submitted.size()==requests);
    releaseHold();idle();assert(submitted.size()==requests);controlFromStatus();} // Count exhausted while held never rearms or sends STOP.
  one.bitLength=80;idle();controlFromStatus();
  {const auto ticket=admitRepeatedUp(3,true,Action::stepUp,7);const auto requests=submitted.size();
    releaseHold();idle();assert(localControlPending() && somfy.transceiver.cancelledTicket==ticket && submitted.size()==requests);
    txEvent(ticket,tx_event_t::aborted);idle();assert(!localControlPending());}
  one.bitLength=56;idle();controlFromStatus();
  // These travel through production Runtime, Adapter, FIFO and completed GFX
  // frames; only the RF request/event boundary is fake.
  for(unsigned cause=0;cause<4;++cause){
    const auto ticket=admitRepeatedUp(2);const auto requests=submitted.size();
    const auto address=one.address;uint16_t *pixels=nullptr;
    if(cause==0){++one.address;idle();} // Exact target identity changed.
    else if(cause==1){authorization.configure(AuthMode::pin,"","1111");idle();}
    else if(cause==2){pixels=RuntimeTestAccess::losePixels(ui);assert(!ui.tick(testNow,status,false,send,light));}
    else tick(false,6000); // Derived admitted-air deadline, not queue admission age.
    assert(localControlPending() && somfy.transceiver.cancelledTicket==ticket);
    assert(submitted.size()==requests); // No implicit STOP/MY or request replay.
    txEvent(ticket-1,tx_event_t::finished);assert(localControlPending());
    txEvent(ticket,tx_event_t::aborted);assert(!localControlPending());
    one.address=address;authorization.configure(AuthMode::none,"","");
    if(pixels)RuntimeTestAccess::restorePixels(ui,pixels);
    releaseHold();idle();controlFromStatus();
  }
  const auto longTicket=admitRepeatedUp(127);tick(false,6000);
  assert(localControlPending() && !somfy.transceiver.cancelledTicket); // Not the old5s admission timeout.
  const auto beforeExplicitStop=submitted.size();releaseHold();assert(submitted.size()==beforeExplicitStop);click();idle();
  assert(somfy.transceiver.cancelledTicket==longTicket && localControlPending());
  txEvent(longTicket,tx_event_t::aborted);idle();
  assert(!localControlPending() && submitted.size()==beforeExplicitStop+1 && submitted.back().command==somfy_commands::My && submitted.back().id==7 && submitted.back().repeat==-1);
  puts("Actual encoder/GFX: admitted repeat Back, identity/auth/graphics/deadline cancellation, exact terminal ownership and explicit one-shot STOP passed");
  controlFromStatus();
  // Back retracts an unsubmitted intent; no request or MY is generated.
  const auto beforePending=submitted.size();somfy.transceiver.room=false;
  idle();rotate(-1);idle();click();assert(localControlPending() && submitted.size()==beforePending);
  back();assert(!localControlPending() && submitted.size()==beforePending);
  somfy.transceiver.room=true;idle();click();idle();assert(lastQuery.screen==Screen::control);
  // STOP also retracts an idle target's queued UP, with zero idle MY.
  one.idle=true;idle();somfy.transceiver.room=false;rotate(-1);idle();click();
  assert(localControlPending() && submitted.size()==beforePending);idle();click();
  assert(!localControlPending() && submitted.size()==beforePending);
  somfy.transceiver.room=true;idle();click();assert(!localControlPending() && submitted.size()==beforePending);
  // Favourite uses the same movement controls, exact presentation gate and FIFO.
  const auto beforeFavourite=submitted.size();
  auto favouriteMenu=[&](Action desired) {
    assert(RuntimeTestAccess::view(*active).screen==Screen::control);
    rotate(1);idle();rotate(1);idle();click();idle();assert(lastQuery.screen==Screen::actions);
    for(unsigned i=0;i<unsigned(Action::count)&&RuntimeTestAccess::view(*active).action!=desired;++i){rotate(1);idle();}
    assert(RuntimeTestAccess::view(*active).action==desired);click();idle();
    assert(lastQuery.screen==Screen::favouriteObserve && captureProtected);
  };
  favouriteMenu(Action::setFavouriteHere);saveFrame(preview,"runtime-favourite-observe");
  busBusy=true;rotate(1);click();assert(lastQuery.screen==Screen::favouriteObserve && submitted.size()==beforeFavourite);
  busBusy=false;idle();click();idle();assert(lastQuery.screen==Screen::favouriteConfirm && captureProtected);
  assert(RuntimeTestAccess::view(*active).valueChoice==0);saveFrame(preview,"runtime-favourite-confirm");
  busBusy=true;rotate(1);click();assert(submitted.size()==beforeFavourite && !localControlPending());
  busBusy=false;idle();somfy.transceiver.room=false;click();idle();
  assert(localControlPending() && submitted.size()==beforeFavourite);saveFrame(preview,"runtime-favourite-pending");
  ++one.observationRevision;somfy.transceiver.room=true;idle();
  assert(!localControlPending() && submitted.size()==beforeFavourite); // No latent programming after changed observation.
  favouriteMenu(Action::setFavouriteHere);rotate(1);idle();click();idle();rotate(1);idle();click();idle();
  assert(submitted.size()==beforeFavourite+1 && submitted.back().type==shade_request_type_t::set_favorite_here);
  saveFrame(preview,"runtime-favourite-accepted");
  favouriteMenu(Action::clearFavouriteHere);rotate(1);idle();click();idle();
  assert(lastQuery.screen==Screen::favouriteObserve && submitted.size()==beforeFavourite+1); // Old favourite not known.
  back();idle();assert(!captureProtected && lastQuery.screen==Screen::control);
  one.value.myPos=one.value.currentPos;++one.observationRevision;
  favouriteMenu(Action::clearFavouriteHere);rotate(1);idle();click();idle();
  assert(lastQuery.screen==Screen::favouriteConfirm);saveFrame(preview,"runtime-favourite-clear-confirm");
  rotate(1);idle();click();idle();
  assert(submitted.size()==beforeFavourite+2 && submitted.back().type==shade_request_type_t::clear_favorite_here);
  assert(!captureProtected && lastQuery.screen==Screen::control);
  one.idle=two.idle=true; // Settings teardown case tests idle sleep.
  free(allocated);allocated=nullptr;
  authorization.configure(AuthMode::none,"","");
  Runtime settingsUi;active=&settingsUi;settingsUi.begin();ab=3;testPins[4]=testPins[5]=1;idle();
  const auto rfBeforeSettings=submitted.size();
  openConfigure(0);click();idle(); // Configure General directly
  assert(lastQuery.screen==Screen::settings && captureProtected);
  saveFrame(preview,"runtime-settings-general"); // Actual form/runtime frame
#ifdef AUDIT_UI_RUNTIME_HOST_TEST
  AuditConsole::inputActive=true;auditClick();
  assert(RuntimeTestAccess::settings(settingsUi).mode==Settings::Mode::text);
  const auto auditLength=RuntimeTestAccess::settings(settingsUi).textLength;
  busBusy=true;AuditConsole::injectedInput.turn=1;tick();auditClick();
  assert(RuntimeTestAccess::settings(settingsUi).textLength==auditLength && settingsSaves==0);
  busBusy=false;idle();
  assert(AuditConsole::reportedMode==uint8_t(Settings::Mode::text) && AuditConsole::reportedFormSelected==RuntimeTestAccess::settings(settingsUi).selected);
  auditBack();assert(RuntimeTestAccess::settings(settingsUi).mode==Settings::Mode::fields && RuntimeTestAccess::settings(settingsUi).dirty);
  auditBack();assert(RuntimeTestAccess::settings(settingsUi).mode==Settings::Mode::discard);
  AuditConsole::injectedInput.turn=1;tick();idle();auditClick();
  assert(RuntimeTestAccess::view(settingsUi).screen==Screen::configureMenu && settingsSaves==0);
  AuditConsole::inputActive=false;click();idle();
  assert(RuntimeTestAccess::settings(settingsUi).mode==Settings::Mode::fields && !RuntimeTestAccess::settings(settingsUi).dirty);
#endif
  // A changed keyboard selection has the same value/length until pressed.
  // Only its exact fully displayed metadata may authorize that press.
  click();idle();assert(RuntimeTestAccess::settings(settingsUi).mode==Settings::Mode::text);
  const auto keyboardLength=RuntimeTestAccess::settings(settingsUi).textLength;
  busBusy=true;rotate(1);click();assert(RuntimeTestAccess::settings(settingsUi).textLength==keyboardLength && settingsSaves==0);
  busBusy=false;idle();click();idle();assert(RuntimeTestAccess::settings(settingsUi).textLength==keyboardLength+1);
  saveFrame(preview,"runtime-settings-keyboard");back();idle();assert(RuntimeTestAccess::settings(settingsUi).dirty);
  back();idle();assert(RuntimeTestAccess::settings(settingsUi).mode==Settings::Mode::discard);
  click();idle();assert(RuntimeTestAccess::settings(settingsUi).mode==Settings::Mode::fields && RuntimeTestAccess::settings(settingsUi).dirty);
  back();idle();rotate(1);idle();click();idle();assert(lastQuery.screen==Screen::configureMenu && settingsSaves==0);
  click();idle();assert(RuntimeTestAccess::settings(settingsUi).mode==Settings::Mode::fields);
  busBusy=true;rotate(1);click();assert(settingsSaves==0); // unseen field cannot open
  busBusy=false;idle();click();idle(); // Broadcast editor
  rotate(1);idle();click();idle(); // true -> numeric/boolean actions
  rotate(1);idle();click();idle(); // Apply to draft
  assert(Settings::runtimeMemory().used()>0 && settingsSaves==0);
  busBusy=true;for(unsigned i=0;i<3;i++) rotate(1);click();assert(settingsSaves==0); // unseen Save blocked
  settingsRestartCause=RestartReason::queuedOperations;
  busBusy=false;idle();click();idle();assert(settingsSaves==1 && captureProtected);saveFrame(preview,"runtime-settings-saved");
  assert(RuntimeTestAccess::settings(settingsUi).restartReason==RestartReason::queuedOperations);
  // A new typed cause is a different protected result even if all other
  // fields are unchanged. Unseen acknowledgement must wait for the new frame.
  busBusy=true;RuntimeTestAccess::settingsRestartReason(settingsUi,RestartReason::counterUncertain);
  click();assert(lastQuery.screen==Screen::settings && captureProtected && settingsSaves==1);
  busBusy=false;idle();saveFrame(preview,"runtime-settings-counter-blocked");
  assert(Settings::runtimeMemory().used()==0 && settingsAllocations.empty());
  back();idle();assert(lastQuery.screen==Screen::configureMenu && !captureProtected);
  settingsRestartCause=RestartReason::none;
  click();idle();rotate(1);idle();click();idle();rotate(1);idle();click();idle();rotate(1);idle();click();idle();
  assert(Settings::runtimeMemory().used()>0);
  testNow+=21000;authTestNow=testNow;settingsUi.tick(testNow,status,false,send,light);idle();
  assert(Settings::runtimeMemory().used()==0 && settingsAllocations.empty() && lastQuery.screen==Screen::status);
  assert(submitted.size()==rfBeforeSettings);
  // Inject loss after an active draft, rather than only startup allocation failure.
  openConfigure(0);click();idle();
  assert(lastQuery.screen==Screen::settings);rotate(1);idle();click();idle();
  rotate(1);idle();click();idle();rotate(1);idle();click();idle();
  assert(Settings::runtimeMemory().used()>0);
  auto *savedPixels=RuntimeTestAccess::losePixels(settingsUi);
  const auto locksBeforeLoss=locks;
  assert(!settingsUi.tick(testNow,status,true,send,light));
  assert(locks==locksBeforeLoss+1 && Settings::runtimeMemory().used()==0 && settingsAllocations.empty());
  assert(RuntimeTestAccess::view(settingsUi).screen==Screen::status && RuntimeTestAccess::noPresentation(settingsUi));
  for(unsigned i=0;i<3;i++) assert(!settingsUi.tick(++testNow,status,true,send,light));
  assert(locks==locksBeforeLoss+1); // No repeated authorization generation churn.
  RuntimeTestAccess::restorePixels(settingsUi,savedPixels);busBusy=true;
  assert(settingsUi.tick(testNow,status,true,send,light) && captureProtected);
  testNow+=1000;authTestNow=testNow;settingsUi.tick(testNow,status,true,send,light);
  assert(RuntimeTestAccess::view(settingsUi).screen==Screen::quickHome);
  assert(RuntimeTestAccess::noPresentation(settingsUi) && captureProtected);
  busBusy=false;tick(false);idle();assert(!captureProtected);
  // An old presented target and its queued start cannot survive resource loss.
  openDevices();click();idle();
  assert(lastQuery.screen==Screen::control && lastQuery.openedToken);
  Command old{lastQuery.entity,lastQuery.openedToken,Action::up};
  old.authGeneration=pollAuthorization().generation;
  const auto beforeGraphicsCommand=submitted.size();
  somfy.transceiver.room=false;rotate(-1);idle();click();assert(localControlPending());
  const auto oldIntentToken=lastLocalToken;
  savedPixels=RuntimeTestAccess::losePixels(settingsUi);
  assert(!settingsUi.tick(testNow,status,false,send,light));
  assert(!localControlPending() && submitted.size()==beforeGraphicsCommand);
  old.authGeneration=pollAuthorization().generation;
  assert(dispatch(old)==DispatchResult::stale);
  RuntimeTestAccess::restorePixels(settingsUi,savedPixels);somfy.transceiver.room=true;
  idle();assert(RuntimeTestAccess::view(settingsUi).screen==Screen::quickHome);
  assert(RuntimeTestAccess::view(settingsUi).notice==Notice::none && !captureProtected);
  openDevices();click();idle();
  assert(lastQuery.openedToken>old.token);
  old.authGeneration=pollAuthorization().generation;
  assert(dispatch(old)==DispatchResult::stale && submitted.size()==beforeGraphicsCommand);
  somfy.transceiver.room=false;rotate(-1);idle();click();
  assert(localControlPending() && lastLocalToken>oldIntentToken);
  back();somfy.transceiver.room=true;idle();assert(!localControlPending() && submitted.size()==beforeGraphicsCommand);
  // Actual encoder -> Administration -> Room form -> shared local backend.
  somfy.rooms[5]={4,0,"Office"};somfy.rooms[9]={12,1,"Office"};++somfy.configurationRevision;
  openConfigure(6); // Quick entries precedes entity administration.
  busBusy=true;rotate(1);click();assert(lastQuery.screen==Screen::configureMenu);
  busBusy=false;idle();click();
  assert(lastQuery.screen==Screen::administration && captureProtected);
  busBusy=true;click();assert(!strcmp(RuntimeTestAccess::administration(settingsUi).title,"Administration"));
  busBusy=false;idle();click();idle();
  assert(!strcmp(RuntimeTestAccess::administration(settingsUi).title,"Rooms"));
  busBusy=true;rotate(1);click();assert(entityEdits==0 && !strcmp(RuntimeTestAccess::administration(settingsUi).title,"Rooms"));
  busBusy=false;idle();click();idle();saveFrame(preview,"runtime-room-edit");
  assert(!strcmp(RuntimeTestAccess::administration(settingsUi).title,"Edit Room 4"));
  rotate(1);idle();click();idle();rotate(1);idle();click();idle();rotate(-1);idle();click();idle();
  assert(Settings::runtimeMemory().used()>0 && entityEdits==0);
  busBusy=true;rotate(1);click();assert(entityEdits==0); // unseen Save never edits
  busBusy=false;idle();entitySaveState=ModelCommitState::failed;click();idle();
  assert(entityEdits==1 && RuntimeTestAccess::administration(settingsUi).notice==EntityAdmin::Notice::appliedNotSaved);
  assert(RuntimeTestAccess::administration(settingsUi).id==4 && Settings::runtimeMemory().used()==0);
  saveFrame(preview,"runtime-room-save-failed");
  rotate(1);idle();click();idle();assert(entityCommits==1 && entityEdits==1);
  click();idle();assert(!strcmp(RuntimeTestAccess::administration(settingsUi).title,"Rooms"));
  click();idle();for(unsigned i=0;i<4;i++){rotate(1);idle();}click();idle(); // Delete confirmation
  assert(RuntimeTestAccess::administration(settingsUi).mode==EntityAdmin::Mode::confirm);
  click();idle();assert(entityEdits==1 && somfy.getRoomById(4)); // default Cancel
  click();idle();rotate(1);idle();entitySaveState=ModelCommitState::saved;click();idle();
  assert(entityEdits==2 && !somfy.getRoomById(4));click();idle();
  // Add is also an explicit field Apply then Save, with a fresh stable ID.
  assert(RuntimeTestAccess::administration(settingsUi).id==0);click();idle();
  assert(!strcmp(RuntimeTestAccess::administration(settingsUi).title,"Add Room 1"));
  rotate(1);idle();click();idle();rotate(1);idle();click();idle();rotate(-1);idle();click();idle();
  assert(entityEdits==2);rotate(1);idle();click();idle();
  assert(entityEdits==3 && somfy.getRoomById(1));saveFrame(preview,"runtime-room-added");
  click();idle();assert(RuntimeTestAccess::administration(settingsUi).id==1);
  back();idle();back();idle();assert(lastQuery.screen==Screen::configureMenu);
  assert(Settings::runtimeMemory().used()==0 && submitted.size()==beforeGraphicsCommand);
  homeQuick();back();idle();assert(lastQuery.screen==Screen::status);
  auto openRoomDraft=[&] {
    openConfigure(7);
    click();idle();click();idle();rotate(1);idle();click();idle();
    assert(!strcmp(RuntimeTestAccess::administration(settingsUi).title,"Edit Room 12"));
    rotate(1);idle();click();idle();rotate(1);idle();click();idle();rotate(-1);idle();click();idle();
    assert(Settings::runtimeMemory().used()>0 && entityEdits==3);
  };
  openRoomDraft();savedPixels=RuntimeTestAccess::losePixels(settingsUi);
  assert(!settingsUi.tick(testNow,status,false,send,light));
  assert(Settings::runtimeMemory().used()==0 && settingsAllocations.empty() && entityEdits==3);
  RuntimeTestAccess::restorePixels(settingsUi,savedPixels);idle();
  assert(RuntimeTestAccess::view(settingsUi).screen==Screen::quickHome && !captureProtected);
  openRoomDraft();testNow+=21000;authTestNow=testNow;
  settingsUi.tick(testNow,status,false,send,light);
  assert(RuntimeTestAccess::view(settingsUi).screen==Screen::status);
  assert(Settings::runtimeMemory().used()==0 && settingsAllocations.empty() && entityEdits==3);
  assert(somfy.getRoomById(12)->sortOrder==1 && submitted.size()==beforeGraphicsCommand);
  // Standalone Device/Group journeys use the actual Controller, Runtime,
  // Administration, Form and production backend with fake model/file effects.
  click();idle();assert(lastQuery.screen==Screen::quickHome);back();idle(); // Wake opens Quick without an action.
  auto &editable=somfy.shades[25];assert(editable.id==255);
  editable.id=4;editable.address=1004;editable.bitLength=56;editable.repeats=1;
  editable.paired=false;editable.idle=true;editable.lastRollingCode=44;strcpy(editable.name,"Office device");
  auto &editableGroup=somfy.groups[12];assert(editableGroup.id==255);
  editableGroup.id=4;editableGroup.address=504;editableGroup.bitLength=56;editableGroup.repeats=1;
  editableGroup.linkedShades[7]=31;strcpy(editableGroup.name,"Office group"); // missing member remains administrable
  ++somfy.configurationRevision;somfy.transceiver.ready=false;
  auto openAdministration=[&](unsigned type) {
    if(lastQuery.screen==Screen::quickHome){back();idle();}
    assert(lastQuery.screen==Screen::status);openConfigure(7);
    click();idle();assert(lastQuery.screen==Screen::administration && captureProtected);
    if(type) {
      busBusy=true;for(unsigned i=0;i<type;i++){rotate(1);idle();}click();
      assert(!strcmp(RuntimeTestAccess::administration(settingsUi).title,"Administration"));
      busBusy=false;idle();
    }
    click();idle();
  };
  auto closeAdministration=[&] {
    back();idle();back();idle();assert(lastQuery.screen==Screen::configureMenu);
    homeQuick();back();idle();assert(lastQuery.screen==Screen::status && !captureProtected);
  };
  for(unsigned type=1;type<3;type++) {
    const unsigned fields=type==1?21:7,repeatsField=type==1?16:6;
    const auto editsBefore=entityEdits,commitsBefore=entityCommits;
    openAdministration(type);
    assert(!strcmp(RuntimeTestAccess::administration(settingsUi).title,type==1?"Devices":"Groups"));
    busBusy=true;rotate(1);click();assert(entityEdits==editsBefore);
    assert(!strcmp(RuntimeTestAccess::administration(settingsUi).title,type==1?"Devices":"Groups"));
    busBusy=false;idle();click();idle();
    assert(!strcmp(RuntimeTestAccess::administration(settingsUi).title,type==1?"Edit Shade 4":"Edit Group 4"));
    for(unsigned i=0;i<repeatsField;i++){rotate(1);idle();}
    click();idle();rotate(1);idle();click();idle();rotate(-1);idle();click();idle();
    assert(Settings::runtimeMemory().used()>0 && entityEdits==editsBefore);
    busBusy=true;for(unsigned i=repeatsField;i<fields;i++){rotate(1);idle();}click();
    assert(RuntimeTestAccess::administration(settingsUi).mode==EntityAdmin::Mode::fields && entityEdits==editsBefore);
    busBusy=false;idle();click();idle();
    assert(RuntimeTestAccess::administration(settingsUi).mode==EntityAdmin::Mode::confirm);
    click();idle();assert(entityEdits==editsBefore); // Save Cancel is harmless.
    click();idle();busBusy=true;rotate(1);click();assert(entityEdits==editsBefore);
    busBusy=false;idle();entitySaveState=ModelCommitState::failed;click();idle();
    assert(entityEdits==editsBefore+1 && RuntimeTestAccess::administration(settingsUi).id==4);
    assert(RuntimeTestAccess::administration(settingsUi).notice==EntityAdmin::Notice::appliedNotSaved);
    if(type==1)assert(editable.repeats==2 && editable.lastRollingCode==44 && !editable.paired);
    else assert(editableGroup.repeats==2 && editableGroup.linkedShades[7]==31);
    saveFrame(preview,type==1?"runtime-device-file-failed":"runtime-group-file-failed");
    rotate(1);idle();click();idle();assert(entityCommits==commitsBefore+1 && entityEdits==editsBefore+1);
    click();idle();assert(RuntimeTestAccess::administration(settingsUi).id==4);
    click();idle();for(unsigned i=0;i<fields+2;i++){rotate(1);idle();}click();idle();
    assert(RuntimeTestAccess::administration(settingsUi).mode==EntityAdmin::Mode::confirm);
    click();idle();assert(entityEdits==editsBefore+1); // Delete Cancel.
    click();idle();rotate(1);idle();entitySaveState=ModelCommitState::saved;click();idle();
    assert(entityEdits==editsBefore+2);
    assert(type==1?!somfy.getShadeById(4):!somfy.getGroupById(4));
    click();idle();assert(RuntimeTestAccess::administration(settingsUi).id==0);
    click();idle();assert(RuntimeTestAccess::administration(settingsUi).id==1);
    for(unsigned i=0;i<fields;i++){rotate(1);idle();}click();idle();
    assert(RuntimeTestAccess::administration(settingsUi).mode==EntityAdmin::Mode::confirm);
    assert(entityEdits==editsBefore+2);rotate(1);idle();click();idle();
    assert(entityEdits==editsBefore+3 && RuntimeTestAccess::administration(settingsUi).id==1);
    if(type==1)assert(somfy.getShadeById(1) && !somfy.getShadeById(1)->paired && somfy.getShadeById(1)->gpioUp==255);
    else assert(somfy.getGroupById(1) && somfy.getGroupById(1)->address!=somfy.getShadeById(1)->address);
    saveFrame(preview,type==1?"runtime-device-added":"runtime-group-added");
    click();idle();assert(RuntimeTestAccess::administration(settingsUi).id==1);
    closeAdministration();
    assert(Settings::runtimeMemory().used()==0 && settingsAllocations.empty());
    assert(submitted.size()==beforeGraphicsCommand);
  }
  auto &otherGroup=somfy.groups[15];assert(otherGroup.id==255);otherGroup.id=16;otherGroup.address=5016;otherGroup.bitLength=56;otherGroup.sortOrder=2;strcpy(otherGroup.name,"Duplicate");++somfy.configurationRevision;
  snapshotFakeOrderFile(); // Initialize this offline filesystem fixture.
  // Reorder is appended after Add and all configured IDs; existing entry
  // indices and stable targets keep their meaning. Every operation here uses
  // actual encoder -> Controller -> Runtime -> Administration -> Order/backend.
  const auto rfBeforeOrders=submitted.size();
  for(unsigned type=0;type<3;type++) {
    const auto kind=EntityAdmin::Kind(type);
    EntityAdmin::OrderIds before{},slots{};uint8_t count=0;bool normalize=false;
    assert(EntityAdmin::runtimeBackend().readOrder(kind,before,slots,count,normalize)&&count>=2);
    const auto editsBefore=entityEdits,commitsBefore=entityCommits;const auto rfBefore=submitted.size();
    openAdministration(type);
    for(unsigned i=0;i<count;i++){rotate(1);idle();}
    busBusy=true;rotate(1);click();
    assert(!strcmp(RuntimeTestAccess::administration(settingsUi).title,type==0?"Rooms":type==1?"Devices":"Groups"));
    busBusy=false;idle();click();idle();
    assert(strstr(RuntimeTestAccess::administration(settingsUi).title,"Reorder"));
    saveFrame(preview,type==0?"runtime-room-reorder":type==1?"runtime-device-reorder":"runtime-group-reorder");
    click();idle();rotate(1);idle();back();idle();assert(RuntimeTestAccess::administration(settingsUi).id==before[0]);
    click();idle();rotate(1);idle();click();idle();assert(RuntimeTestAccess::administration(settingsUi).dirty && entityEdits==editsBefore);
    for(unsigned i=1;i<count-1;i++){rotate(1);idle();}
    busBusy=true;rotate(1);click();assert(RuntimeTestAccess::administration(settingsUi).mode==EntityAdmin::Mode::fields && entityEdits==editsBefore);
    busBusy=false;idle();click();idle();assert(RuntimeTestAccess::administration(settingsUi).mode==EntityAdmin::Mode::confirm);
    click();idle();assert(entityEdits==editsBefore); // Default Cancel.
    click();idle();busBusy=true;rotate(1);click();assert(entityEdits==editsBefore);
    busBusy=false;idle();entitySaveState=ModelCommitState::failed;click();idle();
    assert(entityEdits==editsBefore+1 && RuntimeTestAccess::administration(settingsUi).notice==EntityAdmin::Notice::appliedNotSaved);
    EntityAdmin::OrderIds applied{},appliedSlots{};uint8_t appliedCount=0;bool compact=false;
    assert(EntityAdmin::runtimeBackend().readOrder(kind,applied,appliedSlots,appliedCount,compact));
    assert(applied[0]==before[1] && applied[1]==before[0] && !compact);
    assert(fileKnown && fileCounts[type]==count && !memcmp(fileOrder[type],before,count));
    saveFrame(preview,"runtime-order-file-failed");
    rotate(1);idle();click();idle();assert(entityCommits==commitsBefore+1 && entityEdits==editsBefore+1);
    assert(fileKnown && fileCounts[type]==appliedCount && !memcmp(fileOrder[type],applied,appliedCount));
    click();idle();assert(RuntimeTestAccess::administration(settingsUi).id==before[0]);
    closeAdministration();assert(submitted.size()==rfBefore && Settings::runtimeMemory().used()==0 && settingsAllocations.empty());
  }
  // A full 32-device list still reaches Reorder (Add remains full). A legacy
  // rank127 can then be compacted explicitly without an artificial move.
  for(uint8_t i=0;i<32;i++){auto &shade=somfy.shades[i];shade={};shade.id=i+1;shade.address=2000+i;shade.bitLength=56;shade.sortOrder=i;shade.idle=true;strcpy(shade.name,"Duplicate");}
  ++somfy.configurationRevision;
  openAdministration(1);for(unsigned i=0;i<33;i++){rotate(1);idle();}click();idle();
  for(unsigned i=0;i<31;i++){rotate(1);idle();}
  assert(RuntimeTestAccess::administration(settingsUi).id==32 && RuntimeTestAccess::administration(settingsUi).total==34);
  saveFrame(preview,"runtime-order-full32");back();idle();closeAdministration();
  somfy.shades[31]={};somfy.shades[30].sortOrder=127;++somfy.configurationRevision;
  const auto beforeNormalize=entityEdits;
  openAdministration(1);for(unsigned i=0;i<32;i++){rotate(1);idle();}click();idle();
  assert(RuntimeTestAccess::administration(settingsUi).dirty);
  for(unsigned i=0;i<31;i++){rotate(1);idle();}click();idle();rotate(1);idle();entitySaveState=ModelCommitState::saved;click();idle();
  assert(entityEdits==beforeNormalize+1 && somfy.getMaxShadeOrder()==30 && fileKnown && fileRanks[1][30]==30);
  click();idle();for(unsigned i=0;i<31;i++){rotate(-1);idle();}click();idle();
  assert(!strcmp(RuntimeTestAccess::administration(settingsUi).title,"Add Shade 32") && entityEdits==beforeNormalize+1);
  back();idle();closeAdministration();
  auto openOrderDraft=[&] {
    openAdministration(1);for(unsigned i=0;i<32;i++){rotate(1);idle();}click();idle();
    click();idle();rotate(1);idle();click();idle();assert(RuntimeTestAccess::administration(settingsUi).dirty);
  };
  openOrderDraft();savedPixels=RuntimeTestAccess::losePixels(settingsUi);
  assert(!settingsUi.tick(testNow,status,false,send,light));assert(RuntimeTestAccess::view(settingsUi).screen==Screen::status && RuntimeTestAccess::noPresentation(settingsUi));
  RuntimeTestAccess::restorePixels(settingsUi,savedPixels);idle();
  openAdministration(1);for(unsigned i=0;i<32;i++){rotate(1);idle();}click();idle();assert(!RuntimeTestAccess::administration(settingsUi).dirty);back();idle();closeAdministration();
  openOrderDraft();testNow+=21000;authTestNow=testNow;settingsUi.tick(testNow,status,false,send,light);idle();
  assert(RuntimeTestAccess::view(settingsUi).screen==Screen::status);click();idle();assert(lastQuery.screen==Screen::quickHome);back();idle(); // Wake only.
  openAdministration(1);for(unsigned i=0;i<32;i++){rotate(1);idle();}click();idle();assert(!RuntimeTestAccess::administration(settingsUi).dirty);back();idle();closeAdministration();
  assert(submitted.size()==rfBeforeOrders && Settings::runtimeMemory().used()==0 && settingsAllocations.empty());
  // Actual encoder -> protected System: unseen launch/confirm never restart.
  openController();selectMenu(0);
  busBusy=true;rotate(1);click();assert(lastQuery.screen==Screen::controllerMenu && !systemRestarts);
  busBusy=false;idle();busBusy=true;click();assert(lastQuery.screen==Screen::system && captureProtected);
  click();assert(!strcmp(RuntimeTestAccess::system(settingsUi).title,"Maintenance"));
  busBusy=false;idle();selectSystem(1);click();idle();assert(!strcmp(RuntimeTestAccess::system(settingsUi).title,"Restart controller?"));
  click();idle();assert(!systemRestarts); // Cancel is the default.
  click();idle();busBusy=true;rotate(1);click();assert(!systemRestarts);
  busBusy=false;idle();click();idle();assert(systemRestarts==1);
  assert(!strcmp(RuntimeTestAccess::system(settingsUi).title,"Restart scheduled"));saveFrame(preview,"runtime-system-restart");
  systemRestartStatus.phase=RestartPhase::cancelled;systemRestartStatus.reason=RestartReason::queuedOperations;idle();
  assert(!strcmp(RuntimeTestAccess::system(settingsUi).title,"Restart stopped"));saveFrame(preview,"runtime-system-stopped");
  back();idle();selectSystem(2);click();idle();assert(!strcmp(RuntimeTestAccess::system(settingsUi).title,"Browser files"));
  click();idle();saveFrame(preview,"runtime-system-browser-backup");assert(systemRestarts==1);
  back();idle();back();idle(); // System menu, Restart remains index0.
  const auto beforeWifiSaves=settingsSaves;const auto beforeWifiRF=submitted.size();
  openNetworkMenu();selectMenu(1);click();idle();
  assert(!strcmp(RuntimeTestAccess::system(settingsUi).title,"WiFi scan") && !wifiScanFeed().pending());
  click();idle();assert(lastQuery.screen==Screen::networkMenu); // default Cancel
  click();idle();busBusy=true;rotate(1);click();assert(!wifiScanFeed().pending());
  busBusy=false;idle();click();idle();assert(wifiScanFeed().pending());
  struct Networks final:WifiScanReader {
    uint16_t count()const override{return 17;}
    bool read(uint16_t index,WifiScanNetwork&row)const override {
      row={};memset(row.ssid,'W',29);strcpy(row.ssid+29,"END");row.bssid[5]=index;return true;
    }
  } networks;
  wifiScanFeed().finish(wifiScanFeed().token(),true,networks);idle();
  assert(RuntimeTestAccess::system(settingsUi).total==17);click();idle();
  auto wifiReview=RuntimeTestAccess::system(settingsUi);assert(wifiReview.mode==EntityAdmin::Mode::wifiReview);
  assert(strlen(wifiReview.rows[2].value)==32 && strstr(wifiReview.rows[2].value,"END"));
  saveFrame(preview,"runtime-wifi-review-wide");click();idle(); // default Cancel, no setting change
  assert(RuntimeTestAccess::view(settingsUi).screen==Screen::system && settingsSaves==beforeWifiSaves);
  click();idle();busBusy=true;rotate(1);click();assert(RuntimeTestAccess::view(settingsUi).screen==Screen::system);
  busBusy=false;idle();click();idle();assert(RuntimeTestAccess::view(settingsUi).screen==Screen::settings);
  assert(!strcmp(RuntimeTestAccess::settings(settingsUi).title,"Network"));
  assert(!strcmp(RuntimeTestAccess::settings(settingsUi).rows[1].value,wifiReview.rows[2].value));
  assert(RuntimeTestAccess::settings(settingsUi).rows[1].dirty && !wifiScanFeed().pending());
  assert(settingsSaves==beforeWifiSaves && submitted.size()==beforeWifiRF && captureProtected);
  saveFrame(preview,"runtime-wifi-network-draft");back();idle();assert(RuntimeTestAccess::settings(settingsUi).mode==Settings::Mode::discard);rotate(1);idle();click();idle();assert(RuntimeTestAccess::view(settingsUi).screen==Screen::networkMenu);
  assert(Settings::runtimeMemory().used()==0);
  auto openWifiRequest=[&](){
    openNetworkMenu();selectMenu(1);click();idle();rotate(1);idle();click();idle();assert(wifiScanFeed().pending());
  };
  openWifiRequest();const auto cancelledWifi=wifiScanFeed().token();back();idle();assert(!wifiScanFeed().pending() && Settings::runtimeMemory().used()==0);
  wifiScanFeed().finish(cancelledWifi,true,networks);idle();assert(lastQuery.screen==Screen::networkMenu);back();idle();
  openWifiRequest();++settingsRevision;idle();assert(!wifiScanFeed().pending() && Settings::runtimeMemory().used()==0);
  back();idle();back();idle();
  openWifiRequest();testNow+=30001;authTestNow=testNow;settingsUi.tick(testNow,status,false,send,light);idle();
  assert(!wifiScanFeed().pending() && Settings::runtimeMemory().used()==0);click();idle(); // wake only if display slept
  if(RuntimeTestAccess::view(settingsUi).screen==Screen::status){click();idle();}
  else {back();idle();back();idle();}
  openWifiRequest();savedPixels=RuntimeTestAccess::losePixels(settingsUi);
  assert(!settingsUi.tick(testNow,status,false,send,light));assert(!wifiScanFeed().pending() && Settings::runtimeMemory().used()==0);
  RuntimeTestAccess::restorePixels(settingsUi,savedPixels);idle();homeQuick();
  // Real System child, protected presentation, exact-owned scan context and
  // separate Radio settings draft; RF boundaries remain fake.
  for(auto &shade:somfy.shades)shade.idle=true;
  for(auto &group:somfy.groups){group.direction=0;group.lastFrame.valid=false;}
  const bool priorRadioReady=somfy.transceiver.ready;somfy.transceiver.ready=true;somfy.transceiver.quiescent=true;
  auto &wired=somfy.shades[0];wired.id=6;wired.address=600;wired.proto=radio_proto::GP_Relay;wired.idle=true;
  openDiagnostics();selectSystem(2);click();idle();
  const auto scanStarts=RadioScan::driver.starts;const auto scanSaves=settingsSaves;
  assert(!strcmp(RuntimeTestAccess::system(settingsUi).rows[0].label,"Cancel")&&captureProtected);
  saveFrame(preview,"runtime-radio-start-confirm");click();idle();assert(RadioScan::driver.starts==scanStarts);
  click();idle();busBusy=true;rotate(1);click();assert(RadioScan::driver.starts==scanStarts);
  busBusy=false;idle();click();idle();assert(loopScanActive&&gitUpdate.busy());
  assert(!strcmp(RuntimeTestAccess::system(settingsUi).rows[0].label,"Stop scan"));
  const auto scanRequests=submitted.size();loop_item_t wiredRequest;wiredRequest.request.id=6;wiredRequest.request.command=somfy_commands::Down;
  loop_item_t refusedRf;refusedRf.request.id=7;refusedRf.request.type=shade_request_type_t::unpair;
  assert(loopQueue.submit(refusedRf)&&loopQueue.submit(wiredRequest));idle();
  assert(submitted.size()==scanRequests+1&&submitted.back().id==6&&loopScanActive);
  saveFrame(preview,"runtime-radio-scanning");click();idle();assert(!loopScanActive&&!gitUpdate.busy());
  assert(!strcmp(RuntimeTestAccess::system(settingsUi).rows[0].label,"Close"));saveFrame(preview,"runtime-radio-review");
  busBusy=true;rotate(1);click();assert(settingsSaves==scanSaves);
  busBusy=false;idle();click();idle();assert(lastQuery.screen==Screen::settings);
  assert(!strcmp(RuntimeTestAccess::settings(settingsUi).title,"Radio")&&RuntimeTestAccess::settings(settingsUi).dirty&&settingsSaves==scanSaves);
  saveFrame(preview,"runtime-radio-settings-draft");back();idle();assert(RuntimeTestAccess::settings(settingsUi).mode==Settings::Mode::discard);rotate(1);idle();click();idle();
  assert(lastQuery.screen==Screen::system && !strcmp(RuntimeTestAccess::system(settingsUi).title,"Diagnostics") && RuntimeTestAccess::system(settingsUi).selected==2);
  // Back discards the staged change without saving.
  assert(settingsSaves==scanSaves);somfy.transceiver.ready=priorRadioReady;
  if(lastQuery.screen==Screen::system){back();idle();}
  const auto beforeDiagnosticsRequests=submitted.size();
  // Diagnostics receives copied observations only; it never changes RX mode.
  openDiagnostics();selectSystem(1);click();idle();
  assert(!strcmp(RuntimeTestAccess::system(settingsUi).title,"Receive log") && !receiveHub().active());
  click();idle();assert(!strcmp(RuntimeTestAccess::system(settingsUi).title,"Diagnostics"));
  click();idle();busBusy=true;rotate(1);click();assert(!receiveHub().active());
  busBusy=false;idle();click();idle();assert(receiveHub().active() && captureProtected);
  unsigned int diagnosticPulses[]={4294967295u,10,300};ReceiveObservation diagnosticFrame;
  diagnosticFrame.address=0x123456;diagnosticFrame.rollingCode=432;diagnosticFrame.command=2;
  assert(receiveHub().publish(diagnosticFrame,{diagnosticPulses,3,false}));idle();click();idle();
  assert(!receiveHub().active() && !strcmp(RuntimeTestAccess::system(settingsUi).title,"Received frame"));
  assert(submitted.size()==beforeDiagnosticsRequests && settingsSaves==beforeWifiSaves && captureProtected);
  saveFrame(preview,"runtime-receive-diagnostics");back();idle();
  assert(RuntimeTestAccess::systemContains(settingsUi,"123456"));
  busBusy=true;back();
  assert(Settings::runtimeMemory().used()==0 && !receiveHub().active());
  assert(!RuntimeTestAccess::systemContains(settingsUi,"123456"));
  busBusy=false;idle();back();idle();
  auto beginDiagnostics=[&](){
    openDiagnostics();selectSystem(1);click();idle();rotate(1);idle();click();idle();
    assert(receiveHub().active());
    assert(receiveHub().publish(diagnosticFrame,{diagnosticPulses,3,false}));idle();
    assert(RuntimeTestAccess::systemContains(settingsUi,"123456"));
  };
  for(unsigned reason=0;reason<3;++reason){
    beginDiagnostics();
    if(reason==0){lockAuthorization();idle();}
    else if(reason==1){
      savedPixels=RuntimeTestAccess::losePixels(settingsUi);
      assert(!settingsUi.tick(testNow,status,false,send,light));
      RuntimeTestAccess::restorePixels(settingsUi,savedPixels);idle();
    }else{testNow+=120001;authTestNow=testNow;settingsUi.tick(testNow,status,false,send,light);idle();}
    assert(!receiveHub().active() && Settings::runtimeMemory().used()==0);
    assert(!RuntimeTestAccess::systemContains(settingsUi,"123456"));
  }
  homeQuick();
  assert(submitted.size()==beforeDiagnosticsRequests && settingsSaves==beforeWifiSaves);
  // Standalone updates: actual navigation, shared presentation proof and full
  // release identity. Only the service boundary is scripted here.
  auto openUpdates=[&](){
    openMaintenance();selectSystem(0);click();idle();
    assert(!strcmp(RuntimeTestAccess::system(settingsUi).title,"System updates"));
  };
  authorization.configure(AuthMode::pin,"","1111");Credentials installLogin;strcpy(installLogin.secret,"1111");
  assert(authorization.unlock(installLogin,testNow)==AuthResult::unlocked);installLogin.clear();
  auto &update=Updates::previewBackend;openUpdates();assert(!update.checks && captureProtected);
  saveFrame(preview,"runtime-updates-home");click();idle();assert(update.checks==1 && !update.installs);
  saveFrame(preview,"runtime-updates-checking");
  const auto updateCancelsBeforeSleep=update.cancels;const auto updateActivityBeforeSleep=authActivities;
  tick(false,21000);idle();assert(RuntimeTestAccess::view(settingsUi).screen==Screen::system && update.value.busy && update.cancels==updateCancelsBeforeSleep && authActivities==updateActivityBeforeSleep && lastLight!=0);
  click();idle();assert(update.value.busy && update.cancels==updateCancelsBeforeSleep); // DIM wake only, never Cancel.
  update.value.busy=update.value.cancellable=false;update.value.phase=GitUpdatePhase::available;update.value.canInstall=true;
  update.value.candidate.generation=600;strcpy(update.value.candidate.version,"v4294967295.4294967295.4294967295");idle();
  auto updateReview=RuntimeTestAccess::system(settingsUi);assert(updateReview.mode==EntityAdmin::Mode::updateReview && updateReview.selected==0);
  assert(!strcmp(updateReview.rows[2].value,update.value.candidate.version));saveFrame(preview,"runtime-updates-confirm-wide");
  click();idle();assert(!update.installs);rotate(1);idle();click();idle();busBusy=true;rotate(1);click();assert(!update.installs);
  busBusy=false;idle();click();idle();assert(update.installs==1 && update.value.busy);saveFrame(preview,"runtime-updates-downloading");
  const auto installActivity=authActivities;tick(false,120001);idle();
  assert(RuntimeTestAccess::view(settingsUi).screen==Screen::system && update.value.busy && update.cancels==updateCancelsBeforeSleep && authActivities==installActivity && lastLight!=0 && !pollAuthorization().allowed);
  click();idle();assert(update.value.busy && update.cancels==updateCancelsBeforeSleep && !pollAuthorization().allowed); // DIM wake does not unlock other controls.
  const auto lockedInstallView=RuntimeTestAccess::system(settingsUi);
  assert(!strcmp(lockedInstallView.rows[1].label,"Back to unlock menus") && strstr(lockedInstallView.hint,"Install continues") && !strstr(lockedInstallView.hint,"Back cancels"));
  saveFrame(preview,"runtime-updates-locked-install");
  const auto retryActivity=authActivities;update.value.phase=GitUpdatePhase::retryWaiting;
  update.value.retryCount=2;update.value.retryDelayMs=5000;idle();
  auto retryView=RuntimeTestAccess::system(settingsUi);assert(!strcmp(retryView.title,"Waiting to retry") && !strcmp(retryView.rows[0].label,"Retry 2 in 5s"));
  for(const auto &row:retryView.rows)assert(!strchr(row.label,'%'));
  assert(strstr(retryView.hint,"Install continues") && !strstr(retryView.hint,"Back cancels"));
  saveFrame(preview,"runtime-updates-retry-wait");
  std::vector<uint16_t> retryPixels(static_cast<uint16_t*>(allocated),static_cast<uint16_t*>(allocated)+320*170);
  update.value.retryCount=3;idle();assert(memcmp(retryPixels.data(),allocated,320*170*sizeof(uint16_t)));
  retryPixels.assign(static_cast<uint16_t*>(allocated),static_cast<uint16_t*>(allocated)+320*170);
  update.value.retryDelayMs=9000;idle();assert(memcmp(retryPixels.data(),allocated,320*170*sizeof(uint16_t)));
  assert(update.installs==1 && update.cancels==updateCancelsBeforeSleep && authActivities==retryActivity && !pollAuthorization().allowed);
  update.value.retryDelayMs=0;update.value.phase=GitUpdatePhase::downloading;idle();
  const auto continuingActivity=authActivities;const auto beforeInstallDeadline=update.installAcceptedAt+1800000-30000;
  assert(testNow<beforeInstallDeadline);tick(false,unsigned(beforeInstallDeadline-testNow));idle();
  assert(update.value.busy && update.cancels==updateCancelsBeforeSleep && authActivities==continuingActivity && !pollAuthorization().allowed);
  click();idle();assert(update.value.busy && update.cancels==updateCancelsBeforeSleep && !pollAuthorization().allowed); // Wake remains operation-scoped, not a new login.
  // The highlighted Back row detaches without cancelling or granting authority.
  const auto lockedGeneration=pollAuthorization().generation;
  assert(!strcmp(RuntimeTestAccess::system(settingsUi).rows[1].label,"Back to unlock menus"));
  click();idle();assert(Settings::runtimeMemory().used()==0 && update.cancels==updateCancelsBeforeSleep && update.value.busy && !pollAuthorization().allowed && pollAuthorization().generation==lockedGeneration);
  const auto resumedChecks=update.checks,resumedInstalls=update.installs;const auto continuedOperation=update.value.operation;
  back();idle();assert(!RuntimeTestAccess::updatesActive(settingsUi) && update.value.busy && update.cancels==updateCancelsBeforeSleep); // Acknowledge the retained locked status.
  homeQuick();openController();selectMenu(1);click();idle();assert(lastQuery.screen==Screen::unlock && captureProtected);
  rotate(1);for(unsigned i=0;i<4;++i)click();while(RuntimeTestAccess::view(*active).credentialKeyboard.selected!=RotaryKeyboard::actionOffset(RuntimeTestAccess::view(*active).credentialKeyboard)+5){rotate(1);idle();}click();idle();
  assert(pollAuthorization().allowed && lastQuery.screen==Screen::controllerMenu);click();idle();selectSystem(0);click();idle();
  assert(RuntimeTestAccess::updatesActive(settingsUi) && update.value.operation==continuedOperation && update.value.busy && update.checks==resumedChecks && update.installs==resumedInstalls);
  saveFrame(preview,"runtime-updates-resumed");click();idle();assert(update.cancels==1 && !update.value.busy);
  back();idle();assert(Settings::runtimeMemory().used()==0);back();idle();authorization.configure(AuthMode::none,"","");
  assert(submitted.size()==beforeDiagnosticsRequests && settingsSaves==beforeWifiSaves);
  auto admitInstallFixture=[&]{
    authorization.configure(AuthMode::none,"","");update.value.reason=GitUpdateReason::none;update.value.phase=GitUpdatePhase::available;
    update.value.busy=update.value.cancellable=update.value.installContinuation=false;update.value.canInstall=true;
    update.next+=1000;update.value.candidate.generation=++update.next;strcpy(update.value.candidate.version,"v4.9.0");
    openUpdates();rotate(1);idle();click();idle();assert(RuntimeTestAccess::system(settingsUi).selected==0);
    rotate(1);idle();click();idle();assert(update.value.busy && update.value.installContinuation);
  };
  for(unsigned reason=0;reason<3;++reason){
    admitInstallFixture();const auto priorCancel=update.cancels,priorInstall=update.installs;auto priorActivity=authActivities;
    if(reason==0){tick(false,900000);idle();rotate(1);idle();rotate(1);idle();priorActivity=authActivities;tick(false,unsigned(update.installAcceptedAt+1800001-testNow));}
    else if(reason==1){authorization.configurationChanged();idle();}
    else{authorization.configure(AuthMode::pin,"","####");idle();}
    idle();assert(update.cancels==priorCancel+1 && !update.value.busy && update.installs==priorInstall && authActivities==priorActivity);
    assert(RuntimeTestAccess::view(settingsUi).screen==Screen::system && captureProtected && Settings::runtimeMemory().used()==0);
    if(reason==0)saveFrame(preview,"runtime-updates-install-deadline");
    authorization.configure(AuthMode::none,"","");
  }
  for(bool manual:{false,true}){
    update.value.busy=update.value.cancellable=true;update.value.manual=manual;update.value.installContinuation=manual;update.value.phase=manual?GitUpdatePhase::downloading:GitUpdatePhase::checking;
    update.installAcceptedAt=testNow;update.installConfiguration=pollAuthorization().configurationGeneration;update.value.operation=++update.next;
    const auto foreignChecks=update.checks,foreignInstalls=update.installs,foreignCancels=update.cancels;
    openUpdates();assert(!strcmp(RuntimeTestAccess::system(settingsUi).title,"System updates"));back();idle();
    assert(update.value.busy && update.checks==foreignChecks && update.installs==foreignInstalls && update.cancels==foreignCancels);
    update.value.busy=update.value.cancellable=update.value.manual=update.value.installContinuation=false;
  }
  // Slow terminal results remain in the update context until explicit acknowledgement.
  update.value.reason=GitUpdateReason::none;openUpdates();click();idle();
  update.value.busy=update.value.cancellable=false;update.value.phase=GitUpdatePhase::error;update.value.reason=GitUpdateReason::transport;idle();
  const auto terminalCancels=update.cancels,terminalChecks=update.checks;const auto terminalActivity=authActivities;
  tick(false,21000);idle();assert(RuntimeTestAccess::view(settingsUi).screen==Screen::system && update.cancels==terminalCancels && authActivities==terminalActivity);
  assert(strstr(RuntimeTestAccess::system(settingsUi).hint,"download failed"));click();idle();assert(update.checks==terminalChecks);back();idle();back();idle();
  // No periodic activity renews the 120-second child authority. Expiry cancels
  // reversible work but keeps a private read-only outcome; a wake sends nothing.
  update.value.reason=GitUpdateReason::none;openUpdates();click();idle();const auto expiryCancels=update.cancels,expiryChecks=update.checks;
  const auto expiryActivity=authActivities;tick(false,120001);idle();
  assert(RuntimeTestAccess::view(settingsUi).screen==Screen::system && update.cancels==expiryCancels+1 && !update.value.busy && authActivities==expiryActivity);
  const auto expiryView=RuntimeTestAccess::system(settingsUi);assert(expiryView.notice==EntityAdmin::Notice::expired && expiryView.mode==EntityAdmin::Mode::updateStatus && captureProtected && Settings::runtimeMemory().used()==0);
  const auto expiryInstalls=update.installs;const auto expiryAuthorization=pollAuthorization();
  click();idle();assert(update.checks==expiryChecks && update.cancels==expiryCancels+1 && RuntimeTestAccess::updatesActive(settingsUi)); // First DIM press only wakes.
  saveFrame(preview,"runtime-updates-expired");
  const auto acknowledged=RuntimeTestAccess::system(settingsUi);assert(acknowledged.selected==2 && !strcmp(acknowledged.rows[2].label,"Back: System"));
  click();idle(); // Fresh encoder press acknowledges the completed private frame.
  assert(!RuntimeTestAccess::updatesActive(settingsUi) && update.checks==expiryChecks && update.installs==expiryInstalls && update.cancels==expiryCancels+1 && Settings::runtimeMemory().used()==0);
  assert(pollAuthorization().generation==expiryAuthorization.generation && pollAuthorization().allowed==expiryAuthorization.allowed);back();idle();
  // Graphics loss still tears down an owned reversible operation exactly once.
  update.value.reason=GitUpdateReason::none;openUpdates();click();idle();const auto lossCancels=update.cancels;
  savedPixels=RuntimeTestAccess::losePixels(settingsUi);assert(!settingsUi.tick(testNow,status,false,send,light));
  assert(update.cancels==lossCancels+1 && Settings::runtimeMemory().used()==0);RuntimeTestAccess::restorePixels(settingsUi,savedPixels);idle();openMaintenance();selectSystem(1);
  click();idle(); // default Restart confirmation

  savedPixels=RuntimeTestAccess::losePixels(settingsUi);
  assert(!settingsUi.tick(testNow,status,false,send,light));assert(RuntimeTestAccess::view(settingsUi).screen==Screen::status && RuntimeTestAccess::noPresentation(settingsUi));
  RuntimeTestAccess::restorePixels(settingsUi,savedPixels);idle();assert(systemRestarts==1);
  // Advanced metadata: actual encoder -> Controller -> Runtime -> lazy Form
  // -> production backend -> real configuration lease. No RF endpoint is used.
  for(auto &shade:somfy.shades)shade={};for(auto &group:somfy.groups)group={};
  auto &advancedShade=somfy.shades[25];advancedShade.id=4;advancedShade.address=1004;
  advancedShade.paired=false;advancedShade.idle=true;advancedShade.lastRollingCode=123;strcpy(advancedShade.name,"Office device");
  advancedShade.linkedRemotes[6].address=2748;advancedShade.linkedRemotes[6].lastRollingCode=55;
  auto &advancedGroup=somfy.groups[12];advancedGroup.id=4;advancedGroup.address=504;
  advancedGroup.linkedShades[0]=4;advancedGroup.linkedShades[31]=88;strcpy(advancedGroup.name,"Office group");
  somfy.repeaters[6]=3567;somfy.transceiver.quiescent=true;++somfy.configurationRevision;
  snapshotFakeOrderFile();assert(!filePairedMarker);const auto rfBeforeAdvanced=submitted.size();
  auto openAdvanced=[&](unsigned type,bool memberLinks=true) {
    openAdministration(type);
    const auto entries=RuntimeTestAccess::administration(settingsUi);
    const unsigned advance=(entries.total-1+entries.total-entries.selected)%entries.total;
    assert(advance);for(unsigned i=1;i<advance;i++){rotate(1);idle();}
    busBusy=true;rotate(1);click();assert(!strcmp(RuntimeTestAccess::administration(settingsUi).title,type==1?"Devices":"Groups"));
    busBusy=false;idle();click();idle();
    const auto openedAdvanced=RuntimeTestAccess::administration(settingsUi);
    if(strcmp(openedAdvanced.title,type==1?"Advanced devices":"Advanced groups"))fprintf(stderr,"Advanced navigation: type=%u title=%s selected=%u total=%u id=%u\n",type,openedAdvanced.title,openedAdvanced.selected,openedAdvanced.total,openedAdvanced.id);
    assert(!strcmp(RuntimeTestAccess::administration(settingsUi).title,type==1?"Advanced devices":"Advanced groups"));
    click();idle();assert(RuntimeTestAccess::administration(settingsUi).id==4 && Settings::runtimeMemory().used()>0);
    if(type==2&&memberLinks){click();idle();} // Explicit Member links from the group menu.
  };
  const auto metadataBefore=entityEdits,metadataCommitsBefore=entityCommits;
  openAdvanced(1);assert(!strcmp(RuntimeTestAccess::administration(settingsUi).rows[0].label,"Paired marker"));
  // Menu already seen, confirmation + selected Apply remain unseen while SPI
  // is blocked. Neither unseen activation can become a metadata mutation.
  busBusy=true;click();rotate(1);click();assert(entityEdits==metadataBefore && !advancedShade.paired && captureProtected);
  busBusy=false;idle();entitySaveState=ModelCommitState::failed;click();idle();
  assert(entityEdits==metadataBefore+1 && advancedShade.paired && !filePairedMarker);
  assert(RuntimeTestAccess::administration(settingsUi).mode==EntityAdmin::Mode::result && Settings::runtimeMemory().used()>0);
  saveFrame(preview,"runtime-advanced-file-failed");
  busBusy=true;rotate(-1);click();assert(entityCommits==metadataCommitsBefore);
  busBusy=false;idle();entityCommitState=ModelCommitState::saved;click();idle();
  assert(entityEdits==metadataBefore+1 && entityCommits==metadataCommitsBefore+1 && filePairedMarker && Settings::runtimeMemory().used()==0);
  click();idle();assert(!strcmp(RuntimeTestAccess::administration(settingsUi).title,"Devices") && RuntimeTestAccess::administration(settingsUi).id==4);closeAdministration();
  entitySaveState=ModelCommitState::saved;
  openAdvanced(1);rotate(1);idle();click();idle();assert(!strcmp(RuntimeTestAccess::administration(settingsUi).rows[0].value,"Code 55"));
  click();idle();assert(!strcmp(RuntimeTestAccess::administration(settingsUi).rows[1].label,"Remove #2748"));
  const auto remoteBefore=entityEdits;click();idle();assert(entityEdits==remoteBefore && advancedShade.linkedRemotes[6].address==2748); // default Cancel
  click();idle();rotate(1);idle();click();idle();assert(!advancedShade.linkedRemotes[6].address && advancedShade.lastRollingCode==123);
  click();idle();closeAdministration();
  openAdvanced(2);assert(RuntimeTestAccess::administration(settingsUi).total==33);
  const auto memberBefore=entityEdits;click();idle();
  assert(RuntimeTestAccess::administration(settingsUi).mode==EntityAdmin::Mode::membershipReview && RuntimeTestAccess::administration(settingsUi).selected==0);
  assert(entityEdits==memberBefore && advancedGroup.linkedShades[0]==4);
  saveFrame(preview,"runtime-membership-present-review");click();idle(); // Cancel, preserve physical slot0.
  for(unsigned i=0;i<31;++i){rotate(1);idle();}click();idle();
  assert(!strcmp(RuntimeTestAccess::administration(settingsUi).rows[1].label,"Remove ALL #88 entries"));
  saveFrame(preview,"runtime-membership-missing-review");
  rotate(1);idle();click();idle();assert(!advancedGroup.linkedShades[31] && advancedGroup.linkedShades[0]==4 && entityEdits==memberBefore+1);
  assert(!strcmp(RuntimeTestAccess::administration(settingsUi).title,"Membership saved"));
  click();idle();closeAdministration();
  auto &memberCandidate=somfy.shades[31];memberCandidate.id=9;memberCandidate.address=1099;
  memberCandidate.proto=radio_proto::RTW;memberCandidate.bitLength=80;memberCandidate.paired=false;memberCandidate.idle=true;memberCandidate.gpioRelease=0;
  strcpy(memberCandidate.name,"New device");++somfy.configurationRevision;
  openAdvanced(2);for(unsigned i=0;i<32;++i){rotate(1);idle();}click();idle();click();idle();
  auto addReview=RuntimeTestAccess::administration(settingsUi);
  assert(addReview.mode==EntityAdmin::Mode::membershipReview&&addReview.selected==0&&strstr(addReview.rows[1].value,"RTW 80b"));
  saveFrame(preview,"runtime-membership-add-review");
  entitySaveState=ModelCommitState::failed;const auto addsBefore=entityEdits,filesBefore=entityCommits;
  rotate(1);idle();click();idle();assert(entityEdits==addsBefore+1&&advancedGroup.linkedShades[1]==9&&!memberCandidate.paired);
  assert(!strcmp(RuntimeTestAccess::administration(settingsUi).title,"File save failed"));
  saveFrame(preview,"runtime-membership-file-failed");rotate(-1);idle();click();idle();
  assert(entityEdits==addsBefore+1&&entityCommits==filesBefore+1&&!strcmp(RuntimeTestAccess::administration(settingsUi).title,"Membership saved"));
  click();idle();closeAdministration();entitySaveState=ModelCommitState::saved;
  advancedGroup.linkedShades[30]=advancedGroup.linkedShades[31]=255;++somfy.configurationRevision;
  openAdvanced(2);for(unsigned i=0;i<31;++i){rotate(1);idle();}click();idle();
  auto placeholders=RuntimeTestAccess::administration(settingsUi);
  assert(placeholders.mode==EntityAdmin::Mode::membershipReview&&strstr(placeholders.rows[1].label,"#255"));
  saveFrame(preview,"runtime-membership-unused-review");rotate(1);idle();click();idle();
  assert(!advancedGroup.linkedShades[30]&&!advancedGroup.linkedShades[31]&&advancedGroup.linkedShades[0]==4&&advancedGroup.linkedShades[1]==9);
  assert(submitted.size()==rfBeforeAdvanced);click();idle();closeAdministration();
  // Restore fixture preconditions for the existing counter/learning journeys.
  memberCandidate.id=255;advancedGroup.linkedShades[1]=0;++somfy.configurationRevision;snapshotFakeOrderFile();
  openAdministration(3);assert(!strcmp(RuntimeTestAccess::administration(settingsUi).title,"More"));click();idle();
  assert(!strcmp(RuntimeTestAccess::administration(settingsUi).title,"Repeaters"));click();idle();rotate(1);idle();
  const auto repeaterBefore=entityEdits;somfy.transceiver.quiescent=false;click();idle();
  assert(entityEdits==repeaterBefore && somfy.repeaters[6]==3567 && RuntimeTestAccess::administration(settingsUi).mode==EntityAdmin::Mode::validation);
  saveFrame(preview,"runtime-advanced-radio-busy");somfy.transceiver.quiescent=true;
  click();idle();assert(RuntimeTestAccess::administration(settingsUi).selected==0);rotate(1);idle();click();idle();
  assert(!somfy.repeaters[6] && entityEdits==repeaterBefore+1);click();idle();assert(!strcmp(RuntimeTestAccess::administration(settingsUi).title,"More"));closeAdministration();
  openAdvanced(1);const auto quarantineEdits=entityEdits;somfy.transceiver.blockedIdentity=1004;
  click();idle();rotate(1);idle();click();idle();
  assert(RuntimeTestAccess::administration(settingsUi).mode==EntityAdmin::Mode::validation && entityEdits==quarantineEdits && advancedShade.paired);
  assert(!strcmp(RuntimeTestAccess::administration(settingsUi).hint,"Counter recovery is required for this identity."));
  saveFrame(preview,"runtime-advanced-counter-blocked");rotate(1);idle();click();idle();
  assert(somfy.transceiver.blockedIdentity==1004 && entityEdits==quarantineEdits);closeAdministration();
  somfy.transceiver.blockedIdentity=0; // Fixture recovery, never a metadata effect.
  // Graphics loss and sleep discard the lazily allocated object, including
  // exact contexts. Reopening captures a fresh session with no latent action.
  openAdvanced(1);const auto noMoreEdits=entityEdits;savedPixels=RuntimeTestAccess::losePixels(settingsUi);
  assert(!settingsUi.tick(testNow,status,false,send,light) && RuntimeTestAccess::noPresentation(settingsUi));
  assert(Settings::runtimeMemory().used()==0 && settingsAllocations.empty());RuntimeTestAccess::restorePixels(settingsUi,savedPixels);idle();
  openAdvanced(1);testNow+=21000;authTestNow=testNow;settingsUi.tick(testNow,status,false,send,light);
  assert(RuntimeTestAccess::view(settingsUi).screen==Screen::status && Settings::runtimeMemory().used()==0 && settingsAllocations.empty());
  assert(entityEdits==noMoreEdits && submitted.size()==rfBeforeAdvanced && !gitUpdate.busy() && advancedShade.lastRollingCode==123);
  click();idle();assert(lastQuery.screen==Screen::quickHome);back();idle(); // Wake opens Quick without a motor action.
  // Explicit remote commands use the actual protected form, RF-aware FIFO and
  // production adapter; only the hardware request sink records calls.
  advancedShade.proto=radio_proto::RTS;advancedShade.bitLength=56;++somfy.configurationRevision;
  auto openRemoteActual=[&](){openAdvanced(1);for(unsigned i=0;i<5;++i){rotate(1);idle();}click();idle();};
  openRemoteActual();assert(!strcmp(RuntimeTestAccess::administration(settingsUi).rows[0].label,"Up + Down"));
  for(unsigned i=0;i<4;++i){rotate(1);idle();}
  busBusy=true;click();rotate(1);click();assert(submitted.size()==rfBeforeAdvanced);
  busBusy=false;idle();saveFrame(preview,"runtime-remote-prog-confirm");
  assert(RuntimeTestAccess::administration(settingsUi).selected==1); // Rotation changed draft only; unseen activation was refused.
  somfy.transceiver.room=false;click();idle();
  assert(submitted.size()==rfBeforeAdvanced);assert(!strcmp(RuntimeTestAccess::administration(settingsUi).rows[0].label,"Cancel queued request"));
  saveFrame(preview,"runtime-remote-pending");back();idle();somfy.transceiver.room=true;idle();assert(submitted.size()==rfBeforeAdvanced);
  closeAdministration();
  openRemoteActual();for(unsigned i=0;i<4;++i){rotate(1);idle();}click();idle();rotate(1);idle();click();idle();
  assert(submitted.size()==rfBeforeAdvanced); // Metadata fixture deliberately has unavailable radio.
  assert(!strcmp(RuntimeTestAccess::administration(settingsUi).rows[0].label,"Request refused"));
  saveFrame(preview,"runtime-remote-refused");click();idle();closeAdministration();
  somfy.transceiver.ready=true;
  openRemoteActual();for(unsigned i=0;i<4;++i){rotate(1);idle();}click();idle();rotate(1);idle();click();idle();
  assert(submitted.size()==rfBeforeAdvanced+1&&submitted.back().command==somfy_commands::Prog&&submitted.back().id==4);
  assert(!strcmp(RuntimeTestAccess::administration(settingsUi).rows[0].label,"Request accepted"));
  saveFrame(preview,"runtime-remote-accepted");click();idle();closeAdministration();somfy.transceiver.ready=false;
  // Learn is explicit observation -> protected review -> one bounded FIFO
  // marker -> checked shared-domain outcome. No receive event links itself.
  advancedShade.proto=radio_proto::RTS;advancedShade.bitLength=56;++somfy.configurationRevision;
  const auto rfBeforeLearn=submitted.size();const auto savesBeforeLearn=settingsSaves;unsigned olderLearnRequests=0;
  auto openLearnActual=[&](){
    openAdvanced(1);rotate(1);idle();rotate(1);idle();click();idle();
    assert(!strcmp(RuntimeTestAccess::administration(settingsUi).rows[1].label,"Begin observation") && !receiveHub().active());
  };
  auto beginLearnActual=[&](){openLearnActual();rotate(1);idle();click();idle();assert(receiveHub().active());};
  auto heardLearn=[&](uint32_t address,uint16_t code,uint8_t protocol=0,uint8_t bits=56,somfy_commands command=somfy_commands::Up){
    ReceiveObservation observation;observation.observedAt=authTestNow;observation.address=address;observation.rollingCode=code;observation.protocol=protocol;observation.bits=bits;
    observation.command=uint8_t(command);observation.rssi=-44;observation.flags=ReceiveObservation::validFlag;
    assert(receiveHub().publish(observation));idle();
  };
  openLearnActual();click();idle();assert(!receiveHub().active());click();idle();
  busBusy=true;rotate(1);click();assert(!receiveHub().active());busBusy=false;idle();click();idle();assert(receiveHub().active());
  heardLearn(0xee0042,0,2,80,somfy_commands::Sensor);assert(!learnedAttempts);click();idle();
  auto reviewedLearn=RuntimeTestAccess::administration(settingsUi);assert(reviewedLearn.mode==EntityAdmin::Mode::learnReview && reviewedLearn.selected==0);
  assert(strstr(reviewedLearn.rows[2].value,"RTV 80-bit Sensor") && strstr(reviewedLearn.rows[2].label,"Code 0"));
  saveFrame(preview,"runtime-learn-sensor-review");click();idle();assert(!learnedAttempts);click();idle();
  busBusy=true;rotate(1);click();assert(!learnedAttempts);busBusy=false;idle();
  afterLearnApply=[](){busBusy=true;};click();
  assert(!RuntimeTestAccess::systemContains(settingsUi,"15597634")); // Applied result cannot retain the old air frame behind blocked SPI.
  busBusy=false;idle();
  assert(learnedAttempts==1 && learnedLinks==1 && learnedWrites==1 && learnedModelAddress==0xee0042);
  assert(advancedShade.proto==radio_proto::RTS && advancedShade.bitLength==56 && advancedShade.lastRollingCode==123);
  assert(!strcmp(RuntimeTestAccess::administration(settingsUi).title,"Remote linked") && !receiveHub().active());
  assert(submitted.size()==rfBeforeLearn && settingsSaves==savesBeforeLearn && captureProtected);saveFrame(preview,"runtime-learn-linked");
  click();idle();closeAdministration();assert(Settings::runtimeMemory().used()==0);
  // Applied file failure must offer only a current-file retry, never linking
  // or counter persistence again. Its default action remains Close.
  entitySaveState=ModelCommitState::failed;beginLearnActual();heardLearn(0xee0043,42);click();idle();rotate(1);idle();click();idle();
  auto failedLearn=RuntimeTestAccess::administration(settingsUi);assert(failedLearn.selected==1 && strstr(failedLearn.title,"file not saved"));
  assert(learnedLinks==2 && learnedWrites==2 && learnedModelAddress==0xee0042 && !receiveHub().active());saveFrame(preview,"runtime-learn-file-failed");
  busBusy=true;rotate(-1);click();assert(!learnedRetries);busBusy=false;idle();entityCommitState=ModelCommitState::saved;click();idle();
  assert(learnedRetries==1 && learnedLinks==2 && learnedWrites==2 && learnedModelAddress==0xee0043);click();idle();closeAdministration();entitySaveState=ModelCommitState::saved;
  // A held older command is synthetic queue backpressure, not a Learn action.
  // Back retires the exact marker before its private Session is freed.
  beginLearnActual();heardLearn(0xee0044,43);click();idle();rotate(1);idle();
  loop_item_t heldLearn;heldLearn.request.type=shade_request_type_t::unpair;heldLearn.request.id=4;somfy.transceiver.suspended=true;assert(loopQueue.submit(heldLearn));
  click();assert(loopQueue.pending()==2 && learnedAttempts==2);busBusy=true;back();
  assert(!receiveHub().active() && Settings::runtimeMemory().used()==0);
  assert(!RuntimeTestAccess::systemContains(settingsUi,"15597636"));
  busBusy=false;somfy.transceiver.suspended=false;idle();++olderLearnRequests;
  assert(learnedAttempts==2 && learnedWrites==2 && submitted.size()==rfBeforeLearn+olderLearnRequests);closeAdministration();
  beginLearnActual();heardLearn(0xee0047,46);click();idle();rotate(1);idle();
  somfy.transceiver.suspended=true;assert(loopQueue.submit(heldLearn));click();busBusy=true;tick(false,5000);
  assert(learnedAttempts==2 && !receiveHub().active() && Settings::runtimeMemory().used()==0);
  assert(!RuntimeTestAccess::systemContains(settingsUi,"15597639"));
  busBusy=false;somfy.transceiver.suspended=false;idle();++olderLearnRequests;back();idle();closeAdministration();
  beginLearnActual();heardLearn(0xee0048,47);savedPixels=RuntimeTestAccess::losePixels(settingsUi);
  assert(!settingsUi.tick(testNow,status,false,send,light));assert(!receiveHub().active() && Settings::runtimeMemory().used()==0);
  assert(!RuntimeTestAccess::systemContains(settingsUi,"15597640"));
  RuntimeTestAccess::restorePixels(settingsUi,savedPixels);idle();
  // A retained counter newer than the selected observation is refused by the
  // shared checked domain; the LCD does not replace selection with that code.
  beginLearnActual();heardLearn(0xee0045,10);click();idle();learnedCodes[0xee0045]=11;rotate(1);idle();click();idle();
  assert(learnedAttempts==3 && learnedWrites==2 && learnedLinks==2 && strstr(RuntimeTestAccess::administration(settingsUi).hint,"newer"));
  click();idle();back();idle();closeAdministration();
  // An uncertain attempted NVS write has one Close action and no retry/reset.
  beginLearnActual();heardLearn(0xee0046,45);click();idle();learnedInjected=CounterDomainState::nvsUncertain;rotate(1);idle();click();idle();
  auto uncertainLearn=RuntimeTestAccess::administration(settingsUi);assert(!strcmp(uncertainLearn.title,"Counter uncertain") && uncertainLearn.rowCount==1);
  assert(learnedAttempts==4 && learnedWrites==3 && learnedLinks==2 && !receiveHub().active());saveFrame(preview,"runtime-learn-counter-uncertain");
  click();idle();closeAdministration();assert(learnedRetries==1 && Settings::runtimeMemory().used()==0 && settingsAllocations.empty());
  assert(settingsSaves==savesBeforeLearn && submitted.size()==rfBeforeLearn+olderLearnRequests);
  // Fixture reset only; production recovery never clears unknown NVS state.
  somfy.transceiver.quarantine=CounterQuarantine::none;somfy.transceiver.blockedIdentity=0;
  // Primary-code correction uses the same actual Advanced/Form/Backend/FIFO
  // lifecycle, but a synthetic checked-storage endpoint (never physical RF).
  learnedCodes[advancedShade.address]=advancedShade.lastRollingCode;
  advancedShade.linkedRemotes[6].address=advancedShade.address;advancedShade.linkedRemotes[6].lastRollingCode=advancedShade.lastRollingCode;
  ++somfy.configurationRevision;const auto rfBeforePrimary=submitted.size();
  auto openPrimaryActual=[&](){
    openAdvanced(1);for(unsigned i=0;i<2;i++){rotate(1);idle();}
    const auto reads=primaryReads;busBusy=true;rotate(1);click();assert(primaryReads==reads);busBusy=false;idle();click();idle();
    assert(RuntimeTestAccess::administration(settingsUi).mode==EntityAdmin::Mode::counterReview&&RuntimeTestAccess::administration(settingsUi).selected==0&&captureProtected);
  };
  auto editPrimaryActual=[&](bool change=true){
    rotate(1);idle();click();idle();if(change){rotate(-1);idle();}assert(!primaryAttempts||primaryWrites==primaryAttempts);
    click();idle();assert(!strcmp(RuntimeTestAccess::administration(settingsUi).rows[1].label,"Apply to draft"));rotate(1);idle();click();idle();
  };
  openPrimaryActual();click();idle();closeAdministration();assert(!primaryAttempts);
  openPrimaryActual();editPrimaryActual();auto primaryReview=RuntimeTestAccess::administration(settingsUi);
  assert(primaryReview.selected==0&&!strcmp(primaryReview.rows[1].label,"Set code: 122")&&strstr(primaryReview.rows[2].value,"2 matching aliases"));
  saveFrame(preview,"runtime-primary-review");click();idle();assert(!primaryAttempts);editPrimaryActual();
  busBusy=true;rotate(1);click();assert(!primaryAttempts);busBusy=false;idle();afterPrimaryApply=[](){busBusy=true;};click();
  assert(!RuntimeTestAccess::systemContains(settingsUi,"Live 123"));busBusy=false;idle();
  assert(primaryAttempts==1&&primaryWrites==1&&advancedShade.lastRollingCode==122&&advancedShade.linkedRemotes[6].lastRollingCode==122&&learnedCodes[advancedShade.address]==122);
  assert(!strcmp(RuntimeTestAccess::administration(settingsUi).title,"Counter saved"));saveFrame(preview,"runtime-primary-saved");click();idle();closeAdministration();
  openPrimaryActual();editPrimaryActual(false);assert(RuntimeTestAccess::administration(settingsUi).notice==EntityAdmin::Notice::unchanged);
  assert(primaryAttempts==1&&primaryWrites==1);click();idle();closeAdministration();
  openPrimaryActual();editPrimaryActual();entitySaveState=ModelCommitState::failed;rotate(1);idle();click();idle();
  auto primaryFile=RuntimeTestAccess::administration(settingsUi);assert(primaryFile.selected==1&&!strcmp(primaryFile.rows[0].label,"Retry model file save")&&primaryAttempts==2&&primaryWrites==2);
  saveFrame(preview,"runtime-primary-file-failed");const auto retryBeforePrimary=learnedRetries;
  busBusy=true;rotate(-1);click();assert(learnedRetries==retryBeforePrimary);busBusy=false;idle();click();idle();
  assert(learnedRetries==retryBeforePrimary+1&&primaryAttempts==2&&primaryWrites==2&&somfy.transceiver.quarantine==CounterQuarantine::none);
  click();idle();closeAdministration();entitySaveState=ModelCommitState::saved;
  openPrimaryActual();editPrimaryActual();primaryInjected=CounterDomainState::nvsUncertain;rotate(1);idle();click();idle();
  assert(primaryAttempts==3&&primaryWrites==3&&advancedShade.lastRollingCode==121);
  assert(!strcmp(RuntimeTestAccess::administration(settingsUi).title,"Counter uncertain")&&RuntimeTestAccess::administration(settingsUi).rowCount==1);
  saveFrame(preview,"runtime-primary-uncertain");click();idle();closeAdministration();
  assert(!receiveHub().active()&&Settings::runtimeMemory().used()==0&&submitted.size()==rfBeforePrimary);
  // Fixture reset only: unknown NVS is never cleared by this production flow.
  somfy.transceiver.quarantine=CounterQuarantine::none;somfy.transceiver.blockedIdentity=0;primaryInjected=CounterDomainState::saved;
  somfy.transceiver.ready=true;
  // Read-only live status uses the existing protected administration frame.
  somfy.transceiver.quarantine=CounterQuarantine::none;somfy.transceiver.blockedIdentity=0;
  advancedShade.flipPosition=false;advancedShade.tiltType=tilt_types::integrated;advancedShade.sunSensor=true;advancedShade.statusFlags=0x21;
  advancedShade.value.currentPos=35.5f;advancedShade.value.target=80;advancedShade.value.myPos=20;advancedShade.value.myTiltPos=25;
  ++somfy.configurationRevision;const auto statusEdits=entityEdits;const auto statusRequests=submitted.size();
  openAdvanced(1);for(unsigned i=0;i<4;++i){rotate(1);idle();}click();idle();
  assert(RuntimeTestAccess::administration(settingsUi).total==entityStatusRows&&captureProtected);saveFrame(preview,"runtime-status-identity");
  for(unsigned i=0;i<4;++i){rotate(1);idle();}
  assert(!strcmp(RuntimeTestAccess::administration(settingsUi).rows[1].value,"35.5%"));saveFrame(preview,"runtime-status-positions");
  busBusy=true;click();assert(entityEdits==statusEdits&&submitted.size()==statusRequests);busBusy=false;idle();
  for(unsigned i=0;i<4;++i){rotate(1);idle();}saveFrame(preview,"runtime-status-favourite");
  for(unsigned i=0;i<4;++i){rotate(1);idle();}saveFrame(preview,"runtime-status-weather");
  back();idle();back();idle();closeAdministration();assert(entityEdits==statusEdits&&submitted.size()==statusRequests);
  openAdvanced(2,false);rotate(1);idle();click();idle();for(unsigned i=0;i<4;++i){rotate(1);idle();}
  assert(!strcmp(RuntimeTestAccess::administration(settingsUi).rows[1].value,"Varies by member"));saveFrame(preview,"runtime-status-group");
  back();idle();back();idle();closeAdministration();
  openDiagnostics();selectSystem(0);click();idle();assert(!strcmp(RuntimeTestAccess::system(settingsUi).title,"Network details")&&captureProtected);
  assert(!strcmp(RuntimeTestAccess::system(settingsUi).rows[1].value,"Test network"));saveFrame(preview,"runtime-status-network");
  back();idle();selectSystem(3);click();idle();
  assert(!strcmp(RuntimeTestAccess::system(settingsUi).title,"Copy receive log"));saveFrame(preview,"runtime-status-copy-guidance");
  homeQuick();back();idle();
  assert(!RuntimeTestAccess::systemContains(settingsUi,"Test network")&&entityEdits==statusEdits&&submitted.size()==statusRequests);
  // Fixture recovery only; no live RF/NVS. Full protected repeater learn journey.
  somfy.transceiver.quarantine=CounterQuarantine::none;somfy.transceiver.blockedIdentity=0;somfy.transceiver.quiescent=true;
  for(auto &address:somfy.repeaters)address=0;++somfy.configurationRevision;snapshotFakeOrderFile();
  const auto rfBeforeRepeater=submitted.size();const auto editsBeforeRepeater=entityEdits;
  const auto counterWritesBeforeRepeater=learnedWrites+primaryWrites;
  auto openRepeaterStart=[&](bool empty){openAdministration(3);click();idle();
    assert(!strcmp(RuntimeTestAccess::administration(settingsUi).title,"Repeaters"));
    if(!empty){rotate(-1);idle();}click();idle();
    assert(!strcmp(RuntimeTestAccess::administration(settingsUi).rows[1].label,"Begin observation")&&RuntimeTestAccess::administration(settingsUi).selected==0);
  };
  openRepeaterStart(true);assert(!receiveHub().active());saveFrame(preview,"runtime-repeater-start");
  busBusy=true;rotate(1);click();assert(!receiveHub().active());busBusy=false;idle();click();idle();assert(receiveHub().active());
  ReceiveObservation repeaterFrame;repeaterFrame.address=0xbe0051;repeaterFrame.rollingCode=700;repeaterFrame.protocol=0;repeaterFrame.bits=56;
  repeaterFrame.flags=ReceiveObservation::validFlag;repeaterFrame.observedAt=testNow;
  assert(receiveHub().publish(repeaterFrame));idle();assert(entityEdits==editsBeforeRepeater&&submitted.size()==rfBeforeRepeater);
  click();idle();assert(RuntimeTestAccess::administration(settingsUi).selected==0&&captureProtected);
  assert(!strcmp(RuntimeTestAccess::administration(settingsUi).rows[1].label,"Add repeater"));saveFrame(preview,"runtime-repeater-review");
  busBusy=true;rotate(1);click();assert(entityEdits==editsBeforeRepeater);busBusy=false;idle();entitySaveState=ModelCommitState::failed;click();idle();
  assert(entityEdits==editsBeforeRepeater+1&&somfy.repeaters[0]==repeaterFrame.address&&!fileRepeaterAddress&&!receiveHub().active());
  assert(RuntimeTestAccess::administration(settingsUi).mode==EntityAdmin::Mode::result&&RuntimeTestAccess::administration(settingsUi).selected==1);
  assert(submitted.size()==rfBeforeRepeater&&learnedWrites+primaryWrites==counterWritesBeforeRepeater);saveFrame(preview,"runtime-repeater-file-failed");
  const auto commitsBeforeRepeaterRetry=entityCommits;busBusy=true;rotate(-1);click();assert(entityCommits==commitsBeforeRepeaterRetry);
  busBusy=false;idle();entityCommitState=ModelCommitState::saved;click();idle();
  assert(entityCommits==commitsBeforeRepeaterRetry+1&&entityEdits==editsBeforeRepeater+1&&fileRepeaterAddress==repeaterFrame.address);
  click();idle();closeAdministration();entitySaveState=ModelCommitState::saved;
  // Duplicate observation remains reviewable but cannot repeat the addition.
  openRepeaterStart(false);rotate(1);idle();click();idle();repeaterFrame.observedAt=testNow;++repeaterFrame.rollingCode;
  assert(receiveHub().publish(repeaterFrame));idle();click();idle();rotate(1);idle();click();idle();
  assert(!strcmp(RuntimeTestAccess::administration(settingsUi).title,"Repeater not added"));
  assert(strstr(RuntimeTestAccess::administration(settingsUi).hint,"already a repeater"));saveFrame(preview,"runtime-repeater-duplicate");
  assert(entityEdits==editsBeforeRepeater+1&&submitted.size()==rfBeforeRepeater&&learnedWrites+primaryWrites==counterWritesBeforeRepeater);
  back();idle();assert(!strcmp(RuntimeTestAccess::administration(settingsUi).title,"Repeaters")&&!receiveHub().active());
  back();idle();assert(!strcmp(RuntimeTestAccess::administration(settingsUi).title,"More"));
  closeAdministration();assert(!receiveHub().active()&&Settings::runtimeMemory().used()==0);
  // Full physical-input/display guide with fake RF and existing marker writer.
  advancedShade.paired=false;filePairedMarker=false;++somfy.configurationRevision;
  const auto rfBeforeGuide=submitted.size();
  const auto editsBeforeGuide=entityEdits;
  openAdvanced(1);for(unsigned i=0;i<6;++i){rotate(1);idle();}click();idle();
  rotate(1);idle();click();idle(); // Pair intent
  rotate(1);idle();click();idle(); // operator observed motor learn mode
  rotate(1);idle();rotate(1);idle();click();idle(); // draft review, default Cancel
  auto guideReview=RuntimeTestAccess::administration(settingsUi);
  assert(guideReview.mode==EntityAdmin::Mode::programmingReview&&guideReview.selected==0&&captureProtected);
  assert(submitted.size()==rfBeforeGuide&&entityEdits==editsBeforeGuide);saveFrame(preview,"runtime-programming-send-review");
  busBusy=true;rotate(1);click();assert(submitted.size()==rfBeforeGuide);busBusy=false;idle();click();idle();
  assert(submitted.size()==rfBeforeGuide+1&&submitted.back().type==shade_request_type_t::shade_command&&submitted.back().command==somfy_commands::Prog&&submitted.back().repeat==1);
  assert(!advancedShade.paired&&entityEdits==editsBeforeGuide);
  assert(!strcmp(RuntimeTestAccess::administration(settingsUi).rows[2].label,"Request accepted"));saveFrame(preview,"runtime-programming-observe");
  rotate(1);idle();click();idle(); // operator verified intended result, still no marker write
  assert(!advancedShade.paired&&entityEdits==editsBeforeGuide&&RuntimeTestAccess::administration(settingsUi).selected==0);
  saveFrame(preview,"runtime-programming-marker-review");
  busBusy=true;rotate(1);click();assert(!advancedShade.paired&&entityEdits==editsBeforeGuide);busBusy=false;idle();click();idle();
  assert(advancedShade.paired&&filePairedMarker&&entityEdits==editsBeforeGuide+1&&submitted.size()==rfBeforeGuide+1);
  click();idle();closeAdministration();
  advancedShade.shadeType=shade_types::garage1;++somfy.configurationRevision;
  openAdvanced(1);for(unsigned i=0;i<6;++i){rotate(1);idle();}click();idle();rotate(1);idle();click();idle();rotate(1);idle();click();idle();
  rotate(1);idle();rotate(1);idle();click();idle();
  assert(strstr(RuntimeTestAccess::administration(settingsUi).hint,"move"));saveFrame(preview,"runtime-programming-one-button-review");
  back();idle();back();idle();closeAdministration();assert(submitted.size()==rfBeforeGuide+1);
  advancedShade.shadeType=shade_types::shutter;++somfy.configurationRevision;
  // Fixture recovery only. Exercise actual group guide with fake RF/model I/O.
  somfy.transceiver.quarantine=CounterQuarantine::none;somfy.transceiver.blockedIdentity=0;somfy.transceiver.ready=true;
  advancedGroup.proto=advancedShade.proto;advancedGroup.bitLength=advancedShade.bitLength;
  advancedGroup.direction=0;advancedGroup.lastFrame={};for(auto &member:advancedGroup.linkedShades)member=0;
  advancedShade.idle=true;++somfy.configurationRevision;snapshotFakeOrderFile();
  const auto rfBeforeGroupGuide=submitted.size();const auto editsBeforeGroupGuide=entityEdits;
  openAdvanced(2,false);for(unsigned i=0;i<3;++i){rotate(1);idle();}click();idle(); // appended programming guide
  rotate(1);idle();click();idle();click();idle(); // Pair intent, select device
  rotate(1);idle();click();idle(); // review LCD open memory, default Cancel
  assert(RuntimeTestAccess::administration(settingsUi).mode==EntityAdmin::Mode::groupProgrammingReview);
  assert(RuntimeTestAccess::administration(settingsUi).selected==0&&submitted.size()==rfBeforeGroupGuide);
  saveFrame(preview,"runtime-group-programming-memory-review");
  busBusy=true;rotate(1);click();assert(submitted.size()==rfBeforeGroupGuide);busBusy=false;idle();click();idle();
  assert(submitted.size()==rfBeforeGroupGuide+1&&submitted.back().type==shade_request_type_t::shade_command&&submitted.back().id==4&&submitted.back().command==somfy_commands::Prog&&submitted.back().repeat==40);
  assert(entityEdits==editsBeforeGroupGuide);
  rotate(1);idle();click();idle(); // operator observed learn mode
  assert(RuntimeTestAccess::administration(settingsUi).selected==0&&strstr(RuntimeTestAccess::administration(settingsUi).hint,"Other motors"));
  saveFrame(preview,"runtime-group-programming-broadcast-review");
  busBusy=true;rotate(1);click();assert(submitted.size()==rfBeforeGroupGuide+1);busBusy=false;idle();click();idle();
  assert(submitted.size()==rfBeforeGroupGuide+2&&submitted.back().type==shade_request_type_t::group_command&&submitted.back().id==4&&submitted.back().repeat==1);
  assert(entityEdits==editsBeforeGroupGuide&&!advancedGroup.linkedShades[0]);saveFrame(preview,"runtime-group-programming-observe");
  rotate(1);idle();click();idle(); // intended result verified, still no list edit
  assert(RuntimeTestAccess::administration(settingsUi).selected==0&&entityEdits==editsBeforeGroupGuide);
  saveFrame(preview,"runtime-group-programming-list-review");
  busBusy=true;rotate(1);click();assert(entityEdits==editsBeforeGroupGuide);busBusy=false;idle();click();idle();
  assert(entityEdits==editsBeforeGroupGuide+1&&advancedGroup.linkedShades[0]==4&&submitted.size()==rfBeforeGroupGuide+2);
  click();idle();closeAdministration();
  // External learn-mode observation skips the device request, never group review.
  openAdvanced(2,false);for(unsigned i=0;i<3;++i){rotate(1);idle();}click();idle();rotate(-1);idle();click();idle();click();idle();
  rotate(-1);idle();click();idle();assert(strstr(RuntimeTestAccess::administration(settingsUi).hint,"Other motors"));
  back();idle();closeAdministration();assert(submitted.size()==rfBeforeGroupGuide+2&&entityEdits==editsBeforeGroupGuide+1);
  // Actual encoder/GFX/auth/Advanced/FIFO path, with a typed recording sink.
  const auto sensorsBefore=submitted.size();
  openAdvanced(1);for(unsigned i=0;i<7;++i){rotate(1);idle();}click();idle();
  assert(!strcmp(RuntimeTestAccess::administration(settingsUi).rows[0].label,"Sunny"));
  click();idle();for(unsigned i=0;i<3;++i){rotate(1);idle();}click();idle();
  const auto sensorReview=RuntimeTestAccess::administration(settingsUi);
  assert(sensorReview.mode==EntityAdmin::Mode::sensorReview&&sensorReview.selected==0&&captureProtected);
  saveFrame(preview,"runtime-sensor-confirm");
  busBusy=true;rotate(1);click();assert(submitted.size()==sensorsBefore);busBusy=false;idle();click();idle();
  assert(submitted.size()==sensorsBefore+1&&submitted.back().type==shade_request_type_t::shade_sensor);
  assert(submitted.back().sunny==0&&submitted.back().windy==-1&&submitted.back().repeat==1);
  assert(!strcmp(RuntimeTestAccess::administration(settingsUi).rows[0].value,"Request accepted"));
  saveFrame(preview,"runtime-sensor-result");click();idle();closeAdministration();
  // Actual runtime refusal checks stay ahead of the typed domain sink.
  auto &sensorBackend=Advanced::runtimeBackend();const Advanced::Target sensorTarget{Advanced::Scope::devices,4};
  Advanced::Context sensorContext;assert(sensorBackend.capture(sensorTarget,sensorContext));
  const auto sensorAuth=sensorBackend.authorization();const auto sensorHistory=sensorBackend.revision();
  Advanced::SensorSimulation sensorDraft;sensorDraft.windy=1;sensorDraft.sunny=1;sensorDraft.repeats=127;
  struct SensorWitness final:LearnPending{
    LearnQueueReason reason=LearnQueueReason::ready;LearnQueueReason current(uint64_t)override{return reason;}
    void execute(uint64_t)override{assert(false);}void cancelled(LearnQueueReason)override{assert(false);}
  }sensorWitness;
  const auto beforeSensorRefusals=submitted.size();GitUpdateOwnerToken sensorCompetitor;
  assert(gitUpdate.acquireExternal(GitUpdateExternalOwner::manual,sensorCompetitor));
  assert(sensorBackend.sendSensor(sensorTarget,sensorContext,sensorAuth,sensorHistory,sensorDraft,sensorWitness).admission==EntityAdmin::Admission::busy);
  assert(gitUpdate.releaseExternal(sensorCompetitor));
  advancedShade.idle=false;assert(sensorBackend.checkSensorContext(sensorTarget,sensorContext,sensorAuth,sensorHistory)==EntityAdmin::Admission::busy);advancedShade.idle=true;
  somfy.transceiver.suspended=true;assert(sensorBackend.checkSensorContext(sensorTarget,sensorContext,sensorAuth,sensorHistory)==EntityAdmin::Admission::busy);somfy.transceiver.suspended=false;
  somfy.transceiver.quiescent=false;
  assert(sensorBackend.sendSensor(sensorTarget,sensorContext,sensorAuth,sensorHistory,sensorDraft,sensorWitness).admission==EntityAdmin::Admission::busy);somfy.transceiver.quiescent=true;
  sensorWitness.reason=LearnQueueReason::expired;
  assert(sensorBackend.sendSensor(sensorTarget,sensorContext,sensorAuth,sensorHistory,sensorDraft,sensorWitness).admission==EntityAdmin::Admission::stale);sensorWitness.reason=LearnQueueReason::ready;
  advancedShade.proto=radio_proto::GP_Remote;assert(!sensorBackend.supportsSensor(sensorTarget));advancedShade.proto=radio_proto::RTS;
  assert(submitted.size()==beforeSensorRefusals&&!gitUpdate.busy());
  // Group flags use the same shared typed path, with exact membership context.
  const Advanced::Target sensorGroup{Advanced::Scope::groups,4};Advanced::Context groupSensorContext;assert(sensorBackend.capture(sensorGroup,groupSensorContext));
  assert(sensorBackend.sendSensor(sensorGroup,groupSensorContext,sensorAuth,sensorHistory,sensorDraft,sensorWitness).result==shade_request_result_t::done);
  assert(submitted.size()==beforeSensorRefusals+1&&submitted.back().type==shade_request_type_t::group_sensor&&submitted.back().repeat==127);
  assert(submitted.back().sunny==1&&submitted.back().windy==1);advancedGroup.linkedShades[31]=88;
  assert(sensorBackend.sendSensor(sensorGroup,groupSensorContext,sensorAuth,sensorHistory,sensorDraft,sensorWitness).admission==EntityAdmin::Admission::stale);
  // Existing classifier treats a missing idle link as repairable; movement
  // evidence alongside that missing member makes motor state unknown.
  advancedGroup.direction=1;assert(sensorBackend.capture(sensorGroup,groupSensorContext));
  assert(sensorBackend.checkSensorContext(sensorGroup,groupSensorContext,sensorAuth,sensorHistory)==EntityAdmin::Admission::busy);advancedGroup.linkedShades[31]=0;advancedGroup.direction=0;
  assert(submitted.size()==beforeSensorRefusals+1&&!gitUpdate.busy());
  // Quick access acceptance starts from an otherwise idle sparse model. The
  // three legacy suggestions are navigation only, never an RF shortcut.
  for(auto &shade:somfy.shades)shade.id=255;
  for(auto &group:somfy.groups)group.id=255;
  auto &quickOne=somfy.shades[13];quickOne.id=1;quickOne.address=101;quickOne.paired=true;quickOne.idle=true;quickOne.bitLength=56;quickOne.shadeType=shade_types::shutter;strcpy(quickOne.name,"Same room");
  auto &quickTwo=somfy.shades[31];quickTwo.id=2;quickTwo.address=102;quickTwo.paired=true;quickTwo.idle=true;quickTwo.bitLength=56;quickTwo.shadeType=shade_types::shutter;strcpy(quickTwo.name,"Same room");
  auto &quickBoth=somfy.groups[15];quickBoth.id=1;quickBoth.address=201;quickBoth.bitLength=56;quickBoth.direction=0;quickBoth.lastFrame={};memset(quickBoth.linkedShades,0,sizeof(quickBoth.linkedShades));quickBoth.linkedShades[0]=1;quickBoth.linkedShades[1]=2;strcpy(quickBoth.name,"Both rooms");
  ++somfy.configurationRevision;authorization.configure(AuthMode::none,"","");somfy.transceiver.ready=true;
  free(allocated);allocated=nullptr;Runtime quickUi;active=&quickUi;ab=3;testPins[4]=testPins[5]=testPins[6]=1;quickUi.begin();idle();
  const auto beforeQuickNavigation=submitted.size();const auto writesAtQuickBoot=quickWrites;assert(quickWrites==0);
  assert(RuntimeTestAccess::view(quickUi).screen==Screen::quickHome && !captureProtected);
  auto selectQuick=[&](unsigned choice){
    assert(RuntimeTestAccess::view(quickUi).screen==Screen::quickHome);
    for(unsigned i=0;i<12&&RuntimeTestAccess::view(quickUi).selected!=choice;++i){rotate(RuntimeTestAccess::view(quickUi).selected<choice?1:-1);idle();}
    assert(RuntimeTestAccess::view(quickUi).selected==choice);
  };
  const EntityRef expectedQuick[]={{EntityKind::group,1},{EntityKind::device,1},{EntityKind::device,2}};
  saveFrame(preview,"runtime-quick-home");
  strcpy(quickOne.name,"Master Bedroom");strcpy(quickTwo.name,"Master Bathroom");idle();
  saveFrame(preview,"runtime-quick-home-example");
  strcpy(quickOne.name,"Same room");strcpy(quickTwo.name,"Same room");idle();
  for(unsigned slot=0;slot<3;++slot){
    selectQuick(slot);click();idle();
    assert(lastQuery.screen==Screen::control && lastQuery.entity==expectedQuick[slot] && lastQuery.action==Action::stop && submitted.size()==beforeQuickNavigation);
    assert(RuntimeTestAccess::view(quickUi).quickOrigin);back();idle();
    assert(lastQuery.screen==Screen::quickHome && submitted.size()==beforeQuickNavigation && quickWrites==writesAtQuickBoot);
  }
  // System opens directly from the first destination page.
  // Home navigation must not allocate a private preferences draft or send RF.
  selectQuick(7);click();idle();
  assert(!RuntimeTestAccess::view(quickUi).quickEditing && lastQuery.screen==Screen::controllerMenu && submitted.size()==beforeQuickNavigation && quickWrites==writesAtQuickBoot);
  saveFrame(preview,"runtime-controller-menu");
  selectMenu(0);click();idle();assert(lastQuery.screen==Screen::configureMenu);saveFrame(preview,"runtime-configure-menu");
  const auto navigationSaves=settingsSaves;
  for(unsigned row:{0u,1u,3u,4u}){
    selectMenu(row);click();idle();assert(lastQuery.screen==Screen::settings && captureProtected);
    back();idle();assert(lastQuery.screen==Screen::configureMenu && RuntimeTestAccess::view(quickUi).selected==row);
  }
  selectMenu(2);click();idle();assert(lastQuery.screen==Screen::networkMenu);saveFrame(preview,"runtime-network-menu");
  selectMenu(0);click();idle();assert(lastQuery.screen==Screen::settings && !strcmp(RuntimeTestAccess::settings(quickUi).title,"Network") && captureProtected);
  back();idle();assert(lastQuery.screen==Screen::networkMenu && RuntimeTestAccess::view(quickUi).selected==0);back();idle();
  assert(lastQuery.screen==Screen::configureMenu && RuntimeTestAccess::view(quickUi).selected==2);
  selectMenu(5);click();idle();assert(lastQuery.screen==Screen::brightness);back();idle();assert(lastQuery.screen==Screen::configureMenu && RuntimeTestAccess::view(quickUi).selected==5);
  selectMenu(5);click();idle();assert(lastQuery.screen==Screen::brightness);
  click();idle();click();idle();click();idle(); // Explicit Display Save, unchanged values.
  assert(lastQuery.screen==Screen::configureMenu && RuntimeTestAccess::view(quickUi).selected==5);
  for(unsigned cause=0;cause<2;++cause){
    selectMenu(5);click();idle();assert(lastQuery.screen==Screen::brightness);
    if(cause==0)authorization.configurationChanged();else authorization.configure(AuthMode::pin,"","1111");
    idle();assert(lastQuery.screen==Screen::configureMenu && RuntimeTestAccess::view(quickUi).selected==5);
    authorization.configure(AuthMode::none,"","");idle();
    assert(settingsSaves==navigationSaves && submitted.size()==beforeQuickNavigation && quickWrites==writesAtQuickBoot);
  }
  selectMenu(6);saveFrame(preview,"runtime-configure-quick-entries");click();idle();assert(lastQuery.screen==Screen::quickManage && captureProtected);back();idle();assert(lastQuery.screen==Screen::configureMenu && RuntimeTestAccess::view(quickUi).selected==6);
  selectMenu(7);click();idle();assert(lastQuery.screen==Screen::administration && captureProtected);back();idle();assert(lastQuery.screen==Screen::configureMenu && RuntimeTestAccess::view(quickUi).selected==7);
  back();idle();assert(lastQuery.screen==Screen::controllerMenu);
  selectMenu(1);click();idle();assert(!strcmp(RuntimeTestAccess::system(quickUi).title,"Maintenance") && RuntimeTestAccess::system(quickUi).total==3);saveFrame(preview,"runtime-maintenance-menu");back();idle();assert(lastQuery.screen==Screen::controllerMenu && RuntimeTestAccess::view(quickUi).selected==1);
  selectMenu(2);click();idle();assert(lastQuery.screen==Screen::status);back();idle();assert(lastQuery.screen==Screen::controllerMenu && RuntimeTestAccess::view(quickUi).selected==2);
  selectMenu(3);click();idle();assert(lastQuery.screen==Screen::details);saveFrame(preview,"runtime-about");back();idle();assert(lastQuery.screen==Screen::controllerMenu && RuntimeTestAccess::view(quickUi).selected==3);
  selectMenu(4);click();idle();assert(!strcmp(RuntimeTestAccess::system(quickUi).title,"Diagnostics") && RuntimeTestAccess::system(quickUi).total==4);saveFrame(preview,"runtime-diagnostics-menu");selectSystem(3);saveFrame(preview,"runtime-diagnostics-copy-log");back();idle();assert(lastQuery.screen==Screen::controllerMenu && RuntimeTestAccess::view(quickUi).selected==4);
  back();idle();assert(lastQuery.screen==Screen::quickHome && RuntimeTestAccess::view(quickUi).selected==7);
  assert(settingsSaves==navigationSaves && submitted.size()==beforeQuickNavigation && quickWrites==writesAtQuickBoot);
  for(unsigned row=4;row<7;++row){selectQuick(row);click();idle();assert(lastQuery.screen==Screen::catalogue);saveFrame(preview,row==4?"runtime-devices":row==5?"runtime-groups":"runtime-rooms");back();idle();assert(lastQuery.screen==Screen::quickHome && RuntimeTestAccess::view(quickUi).selected==row);}
  selectQuick(3);click();idle();assert(lastQuery.screen==Screen::quickManage && captureProtected);back();idle();assert(lastQuery.screen==Screen::quickHome && RuntimeTestAccess::view(quickUi).selected==3);
  // Room filtering combines devices and groups without conflating equal IDs.
  auto *mixedRoom=static_cast<decltype(&somfy.rooms[0])>(nullptr);
  for(auto &room:somfy.rooms)if(!room.roomId){mixedRoom=&room;break;}
  assert(mixedRoom);*mixedRoom={14,2,"Kitchen"};
  quickOne.roomId=quickBoth.roomId=14;quickTwo.roomId=0;++somfy.configurationRevision;idle();
  selectQuick(6);click();idle();
  assert(lastQuery.screen==Screen::catalogue && lastQuery.catalogue==CatalogueKind::rooms);
  auto currentCatalogueEntry=[&]{const auto model=adapterSnapshotModel(true,RuntimeTestAccess::view(quickUi));assert(model.pageValid && model.page.cursor>=model.page.offset && model.page.cursor-model.page.offset<model.page.count);return model.page.rows[model.page.cursor-model.page.offset];};
  for(unsigned i=0;i<16 && currentCatalogueEntry().id!=14;i++){rotate(1);idle();}
  assert(currentCatalogueEntry().id==14);const auto mixedRoomCursor=RuntimeTestAccess::view(quickUi).cursor;
  click();idle();assert(lastQuery.catalogue==CatalogueKind::roomContents && lastQuery.room==14);
  auto mixedPage=adapterSnapshotModel(true,RuntimeTestAccess::view(quickUi));
  assert(mixedPage.pageValid && mixedPage.page.total==2 && mixedPage.page.count==2);
  assert(mixedPage.page.rows[0].id==1 && mixedPage.page.rows[1].id==1 && mixedPage.page.rows[0].group!=mixedPage.page.rows[1].group);
  for(unsigned i=0;i<2;i++) {
    const auto entry=currentCatalogueEntry();const auto roomChildCursor=RuntimeTestAccess::view(quickUi).cursor;
    click();idle();assert(lastQuery.screen==Screen::control && lastQuery.entity==(EntityRef{entry.group?EntityKind::group:EntityKind::device,1}));
    back();idle();assert(lastQuery.screen==Screen::catalogue && lastQuery.catalogue==CatalogueKind::roomContents && lastQuery.room==14 && RuntimeTestAccess::view(quickUi).cursor==roomChildCursor);
    if(!i){rotate(1);idle();}
  }
  saveFrame(preview,"runtime-room-mixed-contents");back();idle();
  assert(lastQuery.catalogue==CatalogueKind::rooms && RuntimeTestAccess::view(quickUi).cursor==mixedRoomCursor && currentCatalogueEntry().id==14);
  back();idle();assert(lastQuery.screen==Screen::quickHome && RuntimeTestAccess::view(quickUi).selected==6 && submitted.size()==beforeQuickNavigation);
  // Live labels change without repinning; identity changes never retarget a pin.
  strcpy(quickOne.name,"Renamed device");++somfy.configurationRevision;idle();
  auto quickProjection=adapterSnapshotModel(true,RuntimeTestAccess::view(quickUi));
  assert(!strcmp(quickProjection.page.rows[1].name,"Renamed device"));
  selectQuick(1);++quickOne.address;++somfy.configurationRevision;idle();click();idle();
  assert(lastQuery.screen!=Screen::control && submitted.size()==beforeQuickNavigation && quickWrites==writesAtQuickBoot);
  --quickOne.address;++somfy.configurationRevision;idle();
  quickOne.id=255;++somfy.configurationRevision;idle();click();idle();
  assert(lastQuery.screen!=Screen::control && submitted.size()==beforeQuickNavigation);quickOne.id=1;++somfy.configurationRevision;idle();
  // RF unavailability does not block navigation or independent wired control.
  quickTwo.proto=radio_proto::GP_Relay;somfy.transceiver.ready=false;somfy.transceiver.suspended=true;loopScanActive=true;
  selectQuick(2);click();idle();assert(lastQuery.screen==Screen::control && lastQuery.entity==expectedQuick[2] && lastQuery.action==Action::stop && submitted.size()==beforeQuickNavigation);
  rotate(-1);idle();click();idle();assert(submitted.size()==beforeQuickNavigation+1 && submitted.back().id==2 && submitted.back().command==somfy_commands::Up);
  back();idle();loopScanActive=false;somfy.transceiver.suspended=false;somfy.transceiver.ready=true;quickTwo.proto=radio_proto::RTS;
  const auto quickRfBaseline=submitted.size();
  auto manageQuick=[&]{openConfigure(6);click();idle();assert(lastQuery.screen==Screen::quickManage && RuntimeTestAccess::view(quickUi).quickEditing && captureProtected);};
  auto manageRow=[&](unsigned row){assert(lastQuery.screen==Screen::quickManage);for(unsigned i=0;i<5&&RuntimeTestAccess::view(quickUi).selected!=row;++i){rotate(RuntimeTestAccess::view(quickUi).selected<row?1:-1);idle();}};
  auto quickAction=[&](unsigned row){assert(lastQuery.screen==Screen::quickActions);for(unsigned i=0;i<6&&RuntimeTestAccess::view(quickUi).selected!=row;++i){rotate(RuntimeTestAccess::view(quickUi).selected<row?1:-1);idle();}click();idle();};
  auto saveQuick=[&]{manageRow(3);click();idle();assert(lastQuery.screen==Screen::quickSave && RuntimeTestAccess::view(quickUi).valueChoice==0 && captureProtected);rotate(1);idle();click();idle();};
  // Private management may not be captured while its first frame is blocked.
  openConfigure(6);busBusy=true;click();assert(captureProtected && lastQuery.screen==Screen::quickManage);
  busBusy=false;idle();saveFrame(preview,"runtime-quick-manage");
  manageRow(0);click();idle();quickAction(4); // Move group down in draft only.
  assert(RuntimeTestAccess::view(quickUi).quickDraft.at(0)->ref==expectedQuick[1] && quickWrites==writesAtQuickBoot);
  back();idle();assert(lastQuery.screen==Screen::configureMenu && quickWrites==writesAtQuickBoot);homeQuick();
  manageQuick();manageRow(0);click();idle();quickAction(4);saveQuick();
  assert(quickWrites==writesAtQuickBoot+1 && quickStored.at(0)->ref==expectedQuick[1] && quickStored.at(1)->ref==expectedQuick[0]);
  for(unsigned i=0;i<4&&lastQuery.screen!=Screen::quickHome;++i){back();idle();}
  assert(lastQuery.screen==Screen::quickHome && submitted.size()==quickRfBaseline);
  // Removal, protected picker and explicit pin/save keep typed group/device IDs.
  manageQuick();manageRow(1);click();idle();quickAction(2);
  assert(RuntimeTestAccess::view(quickUi).quickDraft.at(1)->empty());saveQuick();assert(quickStored.at(1)->empty());
  for(unsigned i=0;i<4&&lastQuery.screen!=Screen::quickHome;++i){back();idle();}
  manageQuick();manageRow(1);click();idle();quickAction(1);
  assert(lastQuery.screen==Screen::catalogue && RuntimeTestAccess::view(quickUi).quickPicking && captureProtected);
  saveFrame(preview,"runtime-quick-group-picker");click();idle();
  assert(lastQuery.screen==Screen::quickManage && !RuntimeTestAccess::view(quickUi).quickPicking && captureProtected && RuntimeTestAccess::view(quickUi).quickDraft.at(1)->ref==expectedQuick[0]);
  saveQuick();assert(quickStored.at(1)->ref==expectedQuick[0]);
  for(unsigned i=0;i<4&&lastQuery.screen!=Screen::quickHome;++i){back();idle();}
  const auto beforeDuplicateWrites=quickWrites;manageQuick();manageRow(2);click();idle();quickAction(0);
  assert(lastQuery.screen==Screen::catalogue && captureProtected);click();idle();
  assert(lastQuery.screen==Screen::quickManage && RuntimeTestAccess::view(quickUi).quickDraft.at(2)->ref==expectedQuick[2] && RuntimeTestAccess::view(quickUi).notice==Notice::refused);
  back();idle();assert(quickWrites==beforeDuplicateWrites && submitted.size()==quickRfBaseline);homeQuick();
  const auto savedQuickWrites=quickWrites;
  // Configuration, restore, auth replacement and idle timeout never persist a draft.
  for(unsigned cause=0;cause<4;++cause){
    manageQuick();manageRow(0);click();idle();quickAction(4);
    if(cause==0){++somfy.configurationRevision;idle();}
    else if(cause==1){++somfy.restoreRevision;idle();}
    else if(cause==2){authorization.configure(AuthMode::pin,"","1111");idle();authorization.configure(AuthMode::none,"","");idle();}
    else {tick(false,21000);idle();}
    assert(quickWrites==savedQuickWrites && submitted.size()==quickRfBaseline);
    for(unsigned i=0;i<8&&RuntimeTestAccess::view(quickUi).screen!=Screen::quickHome;++i){if(RuntimeTestAccess::view(quickUi).screen==Screen::status){click();idle();}else{back();idle();}}
    assert(RuntimeTestAccess::view(quickUi).screen==Screen::quickHome);
  }
  // A gesture already down during revocation must still be consumed through
  // release; only the next separate press may enter private management.
  manageQuick();holdEncoder=true;tick(true);tick(true);
  ++somfy.configurationRevision;idle();assert(lastQuery.screen==Screen::configureMenu);
  releaseHold();idle();assert(lastQuery.screen==Screen::configureMenu && quickWrites==savedQuickWrites && submitted.size()==quickRfBaseline);
  // Failed storage never claims a successful preference replacement or RF action.
  const auto durableFirst=quickStored.at(0)->ref;quickWriteResult=QuickPreferenceSave::failed;
  manageQuick();manageRow(0);click();idle();quickAction(4);saveQuick();
  assert(quickStored.at(0)->ref==durableFirst && submitted.size()==quickRfBaseline && RuntimeTestAccess::view(quickUi).notice==Notice::quickSaveFailed);
  quickWriteResult=QuickPreferenceSave::saved;

  for(const auto readState:{QuickAccessLoad::valid,QuickAccessLoad::invalid,QuickAccessLoad::unreadable}){
    // Saved empty/bad/unreadable storage never becomes legacy suggestions or
    // an automatic replacement preference write.
    quickReadState=readState;quickStored={};const auto noWrites=quickWrites;
    free(allocated);allocated=nullptr;Runtime emptyQuick;active=&emptyQuick;ab=3;testPins[4]=testPins[5]=testPins[6]=1;
    emptyQuick.begin();idle();assert(RuntimeTestAccess::view(emptyQuick).screen==Screen::quickHome);
    const auto emptyProjection=adapterSnapshotModel(true,RuntimeTestAccess::view(emptyQuick));
    assert(emptyProjection.quickStatus==readState);for(unsigned i=0;i<3;++i)assert(emptyProjection.quickAccess.at(i)->empty());
    click();idle();assert(lastQuery.screen==Screen::quickHome && submitted.size()==quickRfBaseline && quickWrites==noWrites);
  }
  free(allocated);allocated=nullptr;hasPsram=false;
  Runtime missing;missing.begin();assert(missing.failure()==GraphicsFailure::missing && !missing.tick(testNow,status,false,send,light) && !missing.healthy());
  hasPsram=true;allocateOK=false;Runtime failed;failed.begin();assert(failed.failure()==GraphicsFailure::allocation && !failed.tick(testNow,status,false,send,light) && !failed.healthy());
  puts("Production runtime/adapter: dynamic navigation, presented target/STOP, context change, unlock, capture transitions, 30 s lease, standalone settings/room/device/group/reorder/Advanced metadata admin, presentation/wipe and fallback passed.");
}
