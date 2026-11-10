#pragma once
#include <stdint.h>
#include "LocalCredentialEditor.h"
#include "LocalCatalogue.h"
#include "LocalTargetContext.h"
#include "LocalControlPolicy.h"
#include "LocalControlMenu.h"
#include "LocalValueEditor.h"
#include "LocalQuickAccessNvs.h"
namespace LocalUi {
constexpr uint8_t detailsRowCount=14, detailsVisibleRows=6;
constexpr uint8_t homeRows=8, administrationHomeIndex=6, systemHomeIndex=7;
enum class Screen : uint8_t { status, home, catalogue, roomKinds, control, actions, value, repeatActions, repeatValue, details, brightness, unlock, settings, administration, system, favouriteObserve, favouriteConfirm, quickHome, quickManage, quickActions, quickSave };
using Action=ControlAction;
enum class Notice : uint8_t { none, preview, queued, refused, idle, changed, wait, saved, saveFailed,
  unlockRequired, unlockWrong, unlockWait, unlockUnavailable, unlocked, done, partial, busy, cancelled, valueUnavailable, expired, stopNotQueued, holdToSend, quickSaved, quickSaveFailed, quickSaveUnknown, quickUnavailable };
enum class DispatchResult : uint8_t { queued, done, partial, refused, stale, busy, denied, idle, cancelled, expired, cancelledBusy };
struct Target {
  EntityRef ref;
  char name[21]={};
  uint32_t identity=0, capabilities=0;
  uint64_t revision=0, token=0, favouriteRevision=0;
  ControlDescriptor descriptor;
  uint8_t movingCount=0;
  bool pendingRelayTilt=false,lightUnavailable=false,favouriteReady=false,favouriteClearReady=false;
  int8_t position=-1, tiltPosition=-1;
  bool exists=false, valid=false, moving=false, tiltMoving=false, stopLease=false;
};
struct Model {
  CataloguePage page;
  QuickAccess quickAccess;
  QuickAccessLoad quickStatus=QuickAccessLoad::unreadable;
  uint64_t quickRevision=0;
  bool quickReady=false;
  Target target;
  CatalogueKind catalogue=CatalogueKind::devices;
  uint8_t room=allRooms;
  uint64_t pageToken=0, modelRevision=0, restoreRevision=0;
  bool pageValid=false, ready=false, controlsEnabled=false, anyMoving=false, controlPending=false;
  AuthMode authMode=AuthMode::unavailable;
  bool authorized=false;
  uint32_t authGeneration=0, authConfigurationGeneration=0, retryAfterMs=0, expiredSessionGeneration=0;
};
static_assert(sizeof(Model)<=512,"LCD snapshot owns only one target and three catalogue rows");
struct DisplayPreferences {
  uint8_t brightness=32;
  uint16_t dimSeconds=10, offSeconds=20;
};
struct FormInput { int8_t turn=0; bool activate=false, back=false; };
struct View {
  Screen screen=Screen::status;
  uint8_t selected=0, cursor=0, anchorId=0;
  bool anchored=false;
  CatalogueKind catalogue=CatalogueKind::devices;
  uint8_t room=allRooms;
  char roomName[21]={};
  EntityRef entity;
  uint64_t openedToken=0, favouriteRevision=0;
  bool authorizedSelection=false;
  uint32_t selectionAuthGeneration=0;
  uint8_t level=32, setting=0, detailsOffset=0;
  uint16_t dimSeconds=10, offSeconds=20;
  Action action=Action::stop;
  int16_t draftValue=0;
  uint8_t valueChoice=0, repeatStep=0;
  bool repeating=false, repeatHeld=false;
  QuickAccess quickDraft;
  bool quickEditing=false, quickPicking=false, quickOrigin=false;
  uint8_t quickSlot=0;
  Notice notice=Notice::none;
  uint16_t credentialChoice=0;
  uint8_t credentialLength=0, credentialMaximum=0;
  bool credentialUser=false, credentialByte=false;
  uint32_t retryAfterMs=0;
};
struct Command {
  EntityRef ref;
  uint64_t token=0;
  Action action=Action::stop;
  int8_t value=-1;
  uint8_t step=0;
  // Active session source, or the original expired source for scoped STOP.
  uint32_t authGeneration=0;
  int8_t repeat=-1;
  uint64_t favouriteRevision=0;
};
struct QuickPreferenceRequest {
  QuickAccess value;
  uint64_t modelRevision=0,restoreRevision=0,quickRevision=0;
  uint32_t authGeneration=0,authConfigurationGeneration=0;
};
class Controller {
  View state;
  QuickPreferenceRequest quickGuard;
  bool quickEnabled=false,quickLanded=false,quickSavePending=false;
  void discardQuick();
  void bindEntry(const Model &,EntityRef,const CatalogueEntry &,bool shortcut);
  Command pending, repeatWitness;
  bool repeatPress=false, repeatArmed=false;
  uint32_t repeatObserved=0, repeatConfiguration=0;
  uint64_t repeatModel=0, repeatRestore=0;
  bool repeatCurrent(const Model &) const;
  CredentialEditor editor;
  ValueEditor valueEditor;
  uint32_t valueAuthorizationGeneration=0, valueConfigurationGeneration=0;
  AuthMode editorMode=AuthMode::none;
  Screen afterUnlock=Screen::control;
  DisplayPreferences brightnessBefore;
  uint32_t brightnessConfigurationGeneration=0, editorActivity=0, editorConfigurationGeneration=0;
  uint32_t chosenIdentity=0;
  uint64_t chosenRevision=0, chosenModelRevision=0, chosenRestoreRevision=0;
  bool bindTarget=false, authorizeAfterUnlock=false, authActivity=false, cancelPending=false;
  FormInput formInput;
  void editorView();
  void valueView();
  void cancelValue();
  void beginUnlock(uint32_t,const Model &,Screen);
  void submitControl(uint32_t,const Model &,bool,Action,int8_t value=-1,uint8_t step=0,int8_t repeat=-1);
  bool hasCommand=false, started=false, raw=false, stable=false;
  bool consumed=false, held=false, asleep=false, dimmed=false, savePending=false;
  bool backRaw=false, backStable=false, backConsumed=false;
  uint32_t backSince=0, rawSince=0, pressedSince=0, activity=0, noticeSince=0;
  void touch(uint32_t now);
  void goBack();
  void shortPress(uint32_t now,const Model &,bool visible);
  void openCatalogue(CatalogueKind,uint8_t room);
public:
  void tick(uint32_t now,const Model &,int8_t turn,bool pressed,bool visible,bool backPressed=false,bool keepNavigationContext=false);
  // No writes/domain calls. The Runtime separately locks authorization, cancels
  // admitted queue work and discards its Form/presentation state on graphics loss.
  void discardTransient();
  const View &view() const { return state; }
  bool takeQuickSave(QuickPreferenceRequest &out) {
    if(!quickSavePending)return false;
    out=quickGuard;out.value=state.quickDraft;quickSavePending=false;return true;
  }
  void quickSaveResult(uint32_t,QuickPreferenceSave);
  uint8_t brightness() const { return asleep?0:dimmed?(state.level<4?1:4):state.level; }
  bool take(Command &out);
  bool takeCancellation() {const bool value=cancelPending;cancelPending=false;return value;}
  void result(uint32_t now,DispatchResult);
  void brightnessResult(uint32_t now,bool saved);
  void unlockResult(uint32_t now,AuthResult);
  bool takeCredentials(Credentials &out) { return editor.take(out); }
  bool takeActivity() { const bool touched=authActivity; authActivity=false; return touched; }
  bool takeFormInput(FormInput &out) {
    if(!formInput.turn && !formInput.activate && !formInput.back) return false;
    out=formInput; formInput={}; return true;
  }
  void openSettingsFromSystem() {
    if(state.screen==Screen::system){formInput={};state.screen=Screen::settings;}
  }
  void closeForm() {
    formInput={};state.selected=state.screen==Screen::administration?administrationHomeIndex:state.screen==Screen::system?systemHomeIndex:3;
    state.screen=Screen::home;
  }
  void setBrightness(uint8_t value);
  uint8_t preference() const { return state.level; }
  DisplayPreferences preferences() const { return {state.level,state.dimSeconds,state.offSeconds}; }
  void setPreferences(const DisplayPreferences &);
  bool takePreferences(DisplayPreferences &value) {
    if(!savePending) return false;
    value=preferences(); savePending=false; return true;
  }
};
class Encoder {
  uint8_t previous=3;
  int8_t steps=0;
public:
  int8_t sample(uint8_t ab);
};
} // namespace LocalUi
