#include "LocalUi.h"
#include <stdlib.h>
namespace LocalUi {
namespace {
constexpr uint8_t levels[] = {2, 4, 8, 16, 32, 64, 128, 255};
constexpr uint16_t delays[] = {0, 5, 10, 15, 20, 30, 60, 120, 300};
} // namespace
void Controller::setBrightness(uint8_t value) {
  state.level = 32;
  for(auto level : levels)
    if(level == value)
      state.level = value;
}
void Controller::setPreferences(const DisplayPreferences &p) {
  setBrightness(p.brightness);
  state.dimSeconds = 10;
  state.offSeconds = 20;
  for(auto value : delays) {
    if(value == p.dimSeconds)
      state.dimSeconds = value;
    if(value == p.offSeconds)
      state.offSeconds = value;
  }
}
void Controller::discardTransient() {
  // Save consumption is the existing synchronous live-application boundary;
  // a reported persistence failure still leaves those live values applied.
  const auto keep=state.screen==Screen::brightness || savePending?brightnessBefore:preferences();
  editor.cancel();valueEditor.cancel();
  state=View{};pending=repeatWitness=Command{};formInput={};
  repeatPress=repeatArmed=false;repeatObserved=repeatConfiguration=0;
  repeatModel=repeatRestore=0;quickGuard={};quickEnabled=quickLanded=quickSavePending=false;
  editorMode=AuthMode::none;afterUnlock=Screen::control;
  valueAuthorizationGeneration=valueConfigurationGeneration=0;
  brightnessConfigurationGeneration=editorActivity=editorConfigurationGeneration=0;
  chosenIdentity=0;chosenRevision=chosenModelRevision=chosenRestoreRevision=0;
  bindTarget=authorizeAfterUnlock=authActivity=cancelPending=false;
  hasCommand=started=raw=stable=consumed=held=asleep=dimmed=savePending=false;
  backRaw=backStable=backConsumed=false;
  backSince=rawSince=pressedSince=activity=noticeSince=0;
  brightnessBefore=keep;setPreferences(keep);
}
void Controller::editorView() {
  state.credentialChoice = editor.selected();
  state.credentialLength = editor.length();
  state.credentialMaximum = editor.maximum();
  state.credentialUser = editor.usernameField();
  state.credentialByte = editor.byteMode();
}
void Controller::unlockResult(uint32_t now, AuthResult result) {
  editor.cancel();
  state.action = Action::stop;
  if(result == AuthResult::unlocked) {
    state.screen = afterUnlock;
    authorizeAfterUnlock=true;
    state.notice = Notice::unlocked;
  } else {
    editor.begin(editorMode);
    state.notice = result == AuthResult::locked ? Notice::unlockWait :
                   result == AuthResult::unavailable ? Notice::unlockUnavailable : Notice::unlockWrong;
  }
  editorActivity = noticeSince = now;
  editorView();
}
void Controller::goBack() {
  repeatArmed=false;state.repeatHeld=false;
  cancelPending=true;
  hasCommand=false; state.notice=Notice::none; state.action=Action::stop;
  if(state.screen==Screen::quickHome) {state.screen=Screen::status;}
  else if(state.screen==Screen::quickActions || state.screen==Screen::quickSave) {state.screen=Screen::quickManage;state.selected=state.quickSlot;}
  else if(state.screen==Screen::quickManage) {discardQuick();if(state.routed)closeForm();else {state.screen=Screen::quickHome;state.selected=4;}}
  else if(state.screen==Screen::catalogue && state.quickPicking) {state.quickPicking=false;state.screen=Screen::quickActions;state.selected=0;}
  else if(state.screen==Screen::controllerMenu) {state.screen=Screen::quickHome;state.selected=4;}
  else if(state.screen==Screen::configureMenu) {state.screen=Screen::controllerMenu;state.selected=0;}
  else if(state.screen==Screen::networkMenu) {state.screen=Screen::configureMenu;state.selected=2;}
  else if((state.screen==Screen::details || state.screen==Screen::status) && state.routed)closeForm();
  else if(state.screen==Screen::home && quickEnabled) {state.screen=Screen::quickHome;state.selected=3;}
  else if(state.screen==Screen::unlock) { editor.cancel(); editorView(); state.screen=afterUnlock; }
  else if(state.screen==Screen::brightness) { setPreferences(brightnessBefore); savePending=false; if(state.routed)closeForm();else {state.screen=Screen::home;state.selected=4;} }
  else if(state.screen==Screen::control) {state.screen=state.quickOrigin?Screen::quickHome:Screen::catalogue;if(state.quickOrigin)state.selected=state.quickSlot;}
  else if(state.screen==Screen::catalogue) {
    if(state.catalogue!=CatalogueKind::rooms && state.room!=allRooms) { state.screen=Screen::roomKinds; state.selected=0; }
    else { state.screen=Screen::home; state.selected=state.catalogue==CatalogueKind::rooms?0:state.catalogue==CatalogueKind::devices?1:2; }
  } else if(state.screen==Screen::roomKinds) openCatalogue(CatalogueKind::rooms,allRooms);
  else if(state.screen==Screen::settings || state.screen==Screen::administration || state.screen==Screen::system) formInput.back=true;
  else if(state.screen==Screen::actions || state.screen==Screen::value || state.screen==Screen::repeatActions || state.screen==Screen::repeatValue || state.screen==Screen::favouriteObserve || state.screen==Screen::favouriteConfirm) {cancelValue();state.screen=Screen::control;}
  else state.screen=Screen::status;
}
void Controller::touch(uint32_t now) {
  activity = now;
  asleep = dimmed = false;
}
void Controller::openCatalogue(CatalogueKind kind,uint8_t room) {
  state.screen=Screen::catalogue; state.catalogue=kind; state.room=room;
  state.cursor=0; state.anchored=false;
}
void Controller::beginUnlock(uint32_t now,const Model &m,Screen returning) {
  afterUnlock=returning; editorMode=m.authMode;
  editorConfigurationGeneration=m.authConfigurationGeneration;
  editor.begin(editorMode); editorActivity=now;
  state.screen=Screen::unlock; state.action=Action::stop;
  state.notice=Notice::unlockRequired; editorView();
}
void Controller::valueView() {
  char text[12];valueEditor.preview(text,sizeof(text));
  state.draftValue=int16_t(strtol(text,nullptr,10));
}
void Controller::cancelValue() {
  valueEditor.cancel();state.draftValue=0;state.valueChoice=0;state.repeating=false;state.repeatStep=0;state.favouriteRevision=0;
}
void Controller::submitControl(uint32_t now,const Model &m,bool visible,Action action,int8_t value,uint8_t step,int8_t repeat) {
  const auto &t=m.target;
  if(!t.exists || t.ref!=state.entity || !state.openedToken || t.token!=state.openedToken)
    state.notice=Notice::changed;
  else if(!m.authorized && !(action==Action::stop && t.stopLease && m.expiredSessionGeneration)) beginUnlock(now,m,Screen::control);
  else if(!visible) state.notice=Notice::wait;
  else if(!(t.capabilities & controlActionBit(action))) state.notice=Notice::refused;
  else if((action==Action::stop && !t.moving && !m.controlPending) || (action==Action::tiltStop && !t.tiltMoving)) state.notice=Notice::idle;
  else if(!m.controlsEnabled) state.notice=Notice::preview;
  else if(!m.ready) state.notice=Notice::refused;
  else {
    pending={state.entity,state.openedToken,action,value,step,m.authorized?m.authGeneration:m.expiredSessionGeneration,repeat};
    if(favouriteProgramming(action)) pending.favouriteRevision=state.favouriteRevision;
    hasCommand=true;state.notice=Notice::queued;
  }
  if(state.screen!=Screen::unlock) state.screen=Screen::control;
  state.action=defaultControlAction(t.capabilities);noticeSince=now;
}
void Controller::shortPress(uint32_t now,const Model &m,bool visible) {
  switch(state.screen) {
  case Screen::quickHome:
    if(!visible){state.notice=Notice::wait;break;}
    if(state.selected<3) {
      const auto *binding=m.quickAccess.at(state.selected);
      const auto &entry=m.page.rows[state.selected];
      if(!binding || binding->empty() || !entry.revision || entry.id!=binding->ref.id || entry.identity!=binding->identity)
        {state.notice=Notice::quickUnavailable;break;}
      state.quickSlot=state.selected;bindEntry(m,binding->ref,entry,true);
    } else if(state.selected==3){state.screen=Screen::home;state.selected=0;}
    else {state.screen=Screen::controllerMenu;state.selected=0;}
    break;
  case Screen::controllerMenu:
    if(!visible){state.notice=Notice::wait;break;}
    if(state.selected==0){state.screen=Screen::configureMenu;state.selected=0;}
    else if(state.selected==2 || state.selected==3){state.routed=true;state.returnScreen=Screen::controllerMenu;state.returnSelection=state.selected;state.detailsOffset=0;state.screen=state.selected==2?Screen::status:Screen::details;}
    else if(!m.authorized)beginUnlock(now,m,Screen::controllerMenu);
    else {state.routed=true;state.returnScreen=Screen::controllerMenu;state.returnSelection=state.selected;state.systemSection=state.selected==1?1:2;state.systemSelection=0;state.screen=Screen::system;formInput={};}
    break;
  case Screen::configureMenu:
  case Screen::networkMenu: {
    if(!visible){state.notice=Notice::wait;break;}
    const bool network=state.screen==Screen::networkMenu;
    if(!network && state.selected==2){state.screen=Screen::networkMenu;state.selected=0;break;}
    if(!m.authorized){beginUnlock(now,m,state.screen);break;}
    state.routed=true;state.returnScreen=state.screen;state.returnSelection=state.selected;formInput={};
    if(network && state.selected==1){state.systemSection=3;state.screen=Screen::system;}
    else if(!network && state.selected==5){brightnessBefore=preferences();brightnessConfigurationGeneration=m.authConfigurationGeneration;state.screen=Screen::brightness;state.setting=0;}
    else if(!network && state.selected==6){state.quickDraft=m.quickAccess;state.quickEditing=true;state.quickPicking=false;quickGuard={m.quickAccess,m.modelRevision,m.restoreRevision,m.quickRevision,m.authGeneration,m.authConfigurationGeneration};state.screen=Screen::quickManage;state.selected=0;}
    else if(!network && state.selected==7)state.screen=Screen::administration;
    else {const uint8_t sections[]={0,3,1,2,4};state.settingsSection=network?1:sections[state.selected];state.screen=Screen::settings;}
    break;
  }
  case Screen::quickManage:
    if(!visible){state.notice=Notice::wait;break;}
    if(state.selected<3){state.quickSlot=state.selected;state.screen=Screen::quickActions;state.selected=0;}
    else if(state.selected==3){state.screen=Screen::quickSave;state.valueChoice=0;}
    else {discardQuick();if(state.routed)closeForm();else {state.screen=Screen::quickHome;state.selected=4;}}
    break;
  case Screen::quickActions:
    if(!visible){state.notice=Notice::wait;break;}
    if(state.selected<2){state.quickPicking=true;openCatalogue(state.selected?CatalogueKind::groups:CatalogueKind::devices,allRooms);}
    else {
      if(state.selected==2)state.quickDraft.remove(state.quickSlot);
      else if(state.selected==3 && state.quickSlot) {state.quickDraft.move(state.quickSlot,state.quickSlot-1);--state.quickSlot;}
      else if(state.selected==4 && state.quickSlot<2) {state.quickDraft.move(state.quickSlot,state.quickSlot+1);++state.quickSlot;}
      state.screen=Screen::quickManage;state.selected=state.quickSlot;
    }
    break;
  case Screen::quickSave:
    if(!visible)state.notice=Notice::wait;
    else if(!state.valueChoice){state.screen=Screen::quickManage;state.selected=3;}
    else {quickSavePending=true;state.notice=Notice::queued;}
    break;
  case Screen::unlock:
    if(editor.press()==EditorResult::cancelled) state.screen=afterUnlock;
    editorView(); break;
  case Screen::status: state.screen=quickEnabled?Screen::quickHome:Screen::home; state.selected=0; break;
  case Screen::details: if(state.routed)closeForm();else state.screen=Screen::status; break;
  case Screen::home:
    if(state.selected<3) {
      openCatalogue(state.selected==0?CatalogueKind::rooms:state.selected==1?CatalogueKind::devices:CatalogueKind::groups,allRooms);
    } else if(state.selected==3) {
      if(!m.authorized) beginUnlock(now,m,Screen::home);
      else { state.screen=Screen::settings; formInput={}; }
    } else if(state.selected==4) {
      if(!m.authorized) beginUnlock(now,m,Screen::home);
      else {
        brightnessBefore=preferences(); brightnessConfigurationGeneration=m.authConfigurationGeneration;
        state.screen=Screen::brightness; state.setting=0;
      }
    } else if(state.selected==administrationHomeIndex) {
      if(!visible) state.notice=Notice::wait;
      else if(!m.authorized) beginUnlock(now,m,Screen::home);
      else {state.screen=Screen::administration;formInput={};}
    } else if(state.selected==systemHomeIndex) {
      if(!visible) state.notice=Notice::wait;
      else if(!m.authorized) beginUnlock(now,m,Screen::home);
      else {state.screen=Screen::system;formInput={};}
    } else state.screen=Screen::status;
    break;
  case Screen::catalogue: {
    if(!visible) { state.notice=Notice::wait; break; }
    const unsigned row=state.cursor>=m.page.offset?state.cursor-m.page.offset:catalogueRows;
    if(!m.pageValid || m.catalogue!=state.catalogue || m.room!=state.room || row>=m.page.count) {
      state.notice=Notice::changed; break;
    }
    const auto &entry=m.page.rows[row];
    if(state.catalogue==CatalogueKind::rooms) {
      state.room=entry.id; memcpy(state.roomName,entry.name,sizeof(state.roomName));
      state.screen=Screen::roomKinds; state.selected=0;
    } else {
      const EntityRef ref{state.catalogue==CatalogueKind::devices?EntityKind::device:EntityKind::group,entry.id};
      if(state.quickPicking) {
        const auto edited=state.quickDraft.assign(state.quickSlot,ref,entry.identity);
        state.quickPicking=false;state.screen=Screen::quickManage;state.selected=state.quickSlot;
        if(edited!=QuickAccessEdit::changed && edited!=QuickAccessEdit::unchanged)state.notice=Notice::refused;
      } else bindEntry(m,ref,entry,false);
    }
    break;
  }
  case Screen::roomKinds:
    openCatalogue(state.selected?CatalogueKind::groups:CatalogueKind::devices,state.room); break;
  case Screen::settings:
  case Screen::administration:
  case Screen::system:
    if(visible) formInput.activate=true; else state.notice=Notice::wait;
    break;
  case Screen::brightness:
    if(state.setting<2) ++state.setting;
    else if(!visible) state.notice=Notice::wait;
    else {savePending=true;if(state.routed)closeForm();else {state.screen=Screen::home;state.selected=4;}}
    break;
  case Screen::control:
    if(state.action==Action::count) {
      if(!m.target.exists || m.target.ref!=state.entity || !state.openedToken || m.target.token!=state.openedToken) state.notice=Notice::changed;
      else if(!visible) state.notice=Notice::wait;
      else { const auto menu=controlMenu(m.target.capabilities,false,repeatableControls(m.target.descriptor)!=0);
        if(menu.count) {state.screen=Screen::actions;state.selected=0;state.action=menu.actions[0];}
      }
    } else submitControl(now,m,visible,state.action);
    break;
  case Screen::actions:
  case Screen::repeatActions: {
    const bool repeating=state.screen==Screen::repeatActions;
    const auto menu=repeating?repeatMenu(m.target.descriptor):controlMenu(m.target.capabilities,false,repeatableControls(m.target.descriptor)!=0);
    if(!m.target.exists || m.target.token!=state.openedToken || m.target.ref!=state.entity) state.notice=Notice::changed;
    else if(!visible) state.notice=Notice::wait;
    else if(state.selected>=menu.count || menu.actions[state.selected]!=state.action) state.notice=Notice::changed;
    else if(!m.authorized) beginUnlock(now,m,Screen::control);
    else if(state.action==repeatsNavigation) {
      const auto repeated=repeatMenu(m.target.descriptor);
      if(repeated.count) {state.screen=Screen::repeatActions;state.selected=0;state.action=repeated.actions[0];}
    } else if(repeating && !controlIsStep(state.action)) {
      if(valueEditor.beginInteger(1,1,127,1,now)) {
        state.screen=Screen::repeatValue;state.repeating=true;state.valueChoice=0;state.repeatStep=0;valueView();
        valueAuthorizationGeneration=m.authGeneration;valueConfigurationGeneration=m.authConfigurationGeneration;
      }
    } else if(favouriteProgramming(state.action)) {
      state.screen=Screen::favouriteObserve;state.valueChoice=0;state.favouriteRevision=0;
      valueAuthorizationGeneration=m.authGeneration;valueConfigurationGeneration=m.authConfigurationGeneration;
    }
    else if(controlNeedsValue(state.action)) {

      const bool step=controlIsStep(state.action);
      const int initial=step?1:state.action==Action::position?m.target.position:m.target.tiltPosition;
      if(initial<0 || !valueEditor.beginInteger(initial,step?1:0,step?127:100,1,now)) state.notice=Notice::valueUnavailable;
      else {
        state.screen=Screen::value;state.repeating=repeating;state.valueChoice=0;valueView();
        valueAuthorizationGeneration=m.authGeneration;valueConfigurationGeneration=m.authConfigurationGeneration;
      }
    } else submitControl(now,m,visible,state.action);
    break;
  }
  case Screen::favouriteObserve:
    if(!visible) state.notice=Notice::wait;
    else if(!state.valueChoice) {cancelValue();state.screen=Screen::control;state.action=Action::stop;}
    else if(m.controlPending || m.anyMoving || !m.target.favouriteReady) state.notice=Notice::busy;
    else if(!m.target.favouriteRevision ||
            (state.action==Action::clearFavouriteHere && !m.target.favouriteClearReady)) state.notice=Notice::refused;
    else {state.favouriteRevision=m.target.favouriteRevision;state.screen=Screen::favouriteConfirm;state.valueChoice=0;state.notice=Notice::none;}
    break;
  case Screen::favouriteConfirm:
    if(!visible) state.notice=Notice::wait;
    else if(!state.valueChoice) {cancelValue();state.screen=Screen::control;state.action=Action::stop;}
    else {submitControl(now,m,visible,state.action);cancelValue();}
    break;
  case Screen::value:
  case Screen::repeatValue:
    if(!visible) state.notice=Notice::wait;
    else if(state.valueChoice==0) { state.valueChoice=2;valueEditor.setStep(1,now); }
    else if(state.valueChoice==2) {cancelValue();state.screen=Screen::control;state.action=Action::stop;}
    else if(state.screen==Screen::repeatValue) {
      state.notice=Notice::holdToSend;
    } else {
      int64_t number=0;valueEditor.apply(now);
      if(valueEditor.takeNumber(number)) {
        const auto action=state.action;
        if(state.repeating && state.screen==Screen::value) {
          state.repeatStep=uint8_t(number);
          valueEditor.beginInteger(1,1,127,1,now);state.screen=Screen::repeatValue;state.valueChoice=0;valueView();
        } else {
          const bool repeated=state.screen==Screen::repeatValue;const auto step=state.repeatStep;
          cancelValue();
          submitControl(now,m,visible,action,repeated?-1:controlIsStep(action)?-1:int8_t(number),
            repeated?step:controlIsStep(action)?uint8_t(number):0,repeated?int8_t(number):-1);
        }
      } else {cancelValue();state.screen=Screen::control;state.action=Action::stop;state.notice=Notice::changed;}
    }
    break;
  }
  if(state.notice!=Notice::none) noticeSince=now;
}
void Controller::discardQuick() {
 state.quickDraft={};state.quickEditing=state.quickPicking=false;quickGuard={};quickSavePending=false;
}
void Controller::bindEntry(const Model &m,EntityRef ref,const CatalogueEntry &entry,bool shortcut) {
 state.entity=ref;chosenIdentity=entry.identity;chosenRevision=entry.revision;
 chosenModelRevision=m.modelRevision;chosenRestoreRevision=m.restoreRevision;
 state.openedToken=0;bindTarget=true;state.screen=Screen::control;state.action=Action::stop;
 state.authorizedSelection=m.authorized;state.selectionAuthGeneration=m.authGeneration;state.quickOrigin=shortcut;
}
void Controller::quickSaveResult(uint32_t now,QuickPreferenceSave result) {
 state.notice=result==QuickPreferenceSave::saved?Notice::quickSaved:
   result==QuickPreferenceSave::unknown?Notice::quickSaveUnknown:Notice::quickSaveFailed;
 if(result==QuickPreferenceSave::saved){discardQuick();if(state.routed)closeForm();else {state.screen=Screen::quickHome;state.selected=0;}}
 else state.valueChoice=0;
 noticeSince=now;
}
bool Controller::repeatCurrent(const Model &m) const {
  return m.ready && m.controlsEnabled && m.authorized && m.authGeneration==repeatWitness.authGeneration &&
    m.authConfigurationGeneration==repeatConfiguration && m.modelRevision==repeatModel &&
    m.restoreRevision==repeatRestore && m.target.exists && m.target.ref==repeatWitness.ref &&
    m.target.token==repeatWitness.token && state.entity==repeatWitness.ref &&
    state.openedToken==repeatWitness.token;
}
void Controller::tick(uint32_t now, const Model &m, int8_t turn, bool pressed, bool visible,
                      bool backPressed,bool keepNavigationContext) {
  if(!started) {
    started = true;
    activity = now;
    raw = stable = pressed;
    consumed = pressed;
    rawSince = pressedSince = now;
    backRaw = backStable = backPressed;
    backConsumed = backPressed;
    backSince = now;
  }
  if(m.quickReady) {
    quickEnabled=true;
    if(!quickLanded) {
      quickLanded=true;
      if(state.screen==Screen::status && !m.anyMoving && !m.controlPending){state.screen=Screen::quickHome;state.selected=0;}
    }
  }
  if(state.quickEditing && (!m.authorized || m.authGeneration!=quickGuard.authGeneration ||
     m.authConfigurationGeneration!=quickGuard.authConfigurationGeneration ||
     m.modelRevision!=quickGuard.modelRevision || m.restoreRevision!=quickGuard.restoreRevision || m.quickRevision!=quickGuard.quickRevision)) {
    discardQuick();if(state.routed)closeForm();else {state.screen=Screen::quickHome;state.selected=4;}
    state.notice=m.authorized?Notice::changed:Notice::unlockRequired;noticeSince=now;consumed=consumed || pressed || raw || stable;
  }
  state.retryAfterMs=m.retryAfterMs;
  if(state.screen==Screen::catalogue && m.pageValid && m.catalogue==state.catalogue && m.room==state.room) {
    state.cursor=m.page.cursor;
    const unsigned row=state.cursor>=m.page.offset?state.cursor-m.page.offset:catalogueRows;
    state.anchored=row<m.page.count;
    if(state.anchored) state.anchorId=m.page.rows[row].id;
  }
  if(state.screen==Screen::control && bindTarget && m.target.exists && m.target.ref==state.entity) {
    if(m.target.identity==chosenIdentity && m.target.revision==chosenRevision &&
       m.modelRevision==chosenModelRevision && m.restoreRevision==chosenRestoreRevision)
      state.openedToken=m.target.token;
    else state.notice=Notice::changed;
    const auto primary=controlMenu(m.target.capabilities,true,repeatableControls(m.target.descriptor)!=0);
    if(primary.index(state.action)<0 && primary.count) state.action=primary.actions[0];
    bindTarget=false;
  }
  if(state.screen==Screen::control && m.target.ref==state.entity && m.target.token==state.openedToken) {
    const auto menu=controlMenu(m.target.capabilities,true,repeatableControls(m.target.descriptor)!=0);
    if(menu.index(state.action)<0) state.action=defaultControlAction(m.target.capabilities);
  }
  if(authorizeAfterUnlock && m.authorized) {
    if(m.target.ref==state.entity && m.target.token==state.openedToken) {
      state.authorizedSelection=true; state.selectionAuthGeneration=m.authGeneration;
    }
    authorizeAfterUnlock=false;
  }
  if(state.screen==Screen::favouriteObserve || state.screen==Screen::favouriteConfirm) {
    const bool changed=m.target.ref!=state.entity || m.target.token!=state.openedToken ||
      !(m.target.capabilities&controlActionBit(state.action)) ||
      (state.screen==Screen::favouriteConfirm && (!m.target.favouriteReady || m.controlPending || m.anyMoving ||
       !state.favouriteRevision || state.favouriteRevision!=m.target.favouriteRevision));
    if(changed || !m.authorized || m.authGeneration!=valueAuthorizationGeneration ||
       m.authConfigurationGeneration!=valueConfigurationGeneration) {
      cancelValue();state.screen=Screen::control;state.action=Action::stop;
      state.notice=changed?Notice::changed:Notice::unlockRequired;noticeSince=now;consumed=true;
    }
  }
  if(state.screen==Screen::value || state.screen==Screen::repeatValue) {
    valueEditor.tick(now);
    const bool changed=m.target.ref!=state.entity || m.target.token!=state.openedToken;
    if(changed || !m.authorized || m.authGeneration!=valueAuthorizationGeneration ||
       m.authConfigurationGeneration!=valueConfigurationGeneration || !valueEditor.active()) {
      cancelValue();state.screen=Screen::control;state.action=Action::stop;
      state.notice=changed?Notice::changed:!m.authorized?Notice::unlockRequired:Notice::cancelled;noticeSince=now;
      consumed=true;
    }
  }
  if(state.screen == Screen::brightness &&
     (!m.authorized || m.authConfigurationGeneration != brightnessConfigurationGeneration)) {
    setPreferences(brightnessBefore);
    savePending = false;
    if(state.routed)closeForm();else {state.screen=Screen::home;state.selected=4;}
    state.notice = Notice::unlockRequired;
    noticeSince = now;
    consumed = consumed || pressed || raw || stable;
  }
  if(state.screen == Screen::unlock &&
     (m.authMode != editorMode || m.authConfigurationGeneration != editorConfigurationGeneration ||
      uint32_t(now - editorActivity) >= 30000)) {
    goBack();
    state.notice = Notice::unlockRequired;
    noticeSince = now;
    consumed = true;
  }
  // Stay awake while ANY configured target is estimated moving, even when the
  // user navigates away. A held button never extends this timer by itself.
  const bool moving=m.anyMoving;
  if(moving)
    touch(now);
  // Critical update context may dim, but never enters security sleep or
  // refreshes activity/authority merely because an update is still running.
  const bool keepUpdate=keepNavigationContext && state.screen==Screen::system;
  if(keepUpdate)asleep=false;
  if(!keepUpdate && state.offSeconds && uint32_t(now - activity) >= uint32_t(state.offSeconds) * 1000)
    asleep = true;
  else if(state.dimSeconds && uint32_t(now - activity) >= uint32_t(state.dimSeconds) * 1000)
    dimmed = true;
  if(state.notice != Notice::none && uint32_t(now - noticeSince) >= 2500)
    state.notice = Notice::none;
  if(repeatPress) {
    const bool changed=!repeatCurrent(m) || int32_t(now-repeatObserved)<0;
    const bool draftChanged=repeatArmed && (state.screen!=Screen::repeatValue ||
      state.valueChoice!=1 || state.action!=repeatWitness.action ||
      state.draftValue!=repeatWitness.repeat || state.repeatStep!=repeatWitness.step || !visible || m.controlPending);
    if(changed || draftChanged || turn || backPressed || asleep || dimmed) {
      repeatArmed=false;consumed=true;hasCommand=false;
      if(state.repeatHeld) cancelPending=true;
      state.repeatHeld=false;
    }
    repeatObserved=now;
    // Release is observed before debounce: bounce can only cancel early.
    // Keep the gesture consumed through stable release, even after completion.
    if(!pressed) {
      if(repeatArmed && !state.repeatHeld) {state.notice=Notice::holdToSend;noticeSince=now;}
      if(state.repeatHeld) cancelPending=true;
      repeatArmed=false;state.repeatHeld=false;consumed=true;held=true;
    }
  }
  if(backPressed != backRaw) {
    backRaw = backPressed;
    backSince = now;
  }
  if(backRaw != backStable && uint32_t(now - backSince) >= 40) {
    backStable = backRaw;
    if(backStable) {
      const bool waking = asleep || dimmed;
      touch(now);
      authActivity = true;
      editorActivity = now;
      if(!backConsumed && !waking)
        goBack();
      backConsumed = true;
    } else {
      backConsumed = false;
      if(!pressed && !stable && !raw)
        consumed = false;
    }
  }
  if(backRaw || backStable) {
    consumed = true;
    return;
  }
  if(turn && (stable || raw || pressed))
    consumed = true;
  if(pressed != raw) {
    if(pressed && state.screen==Screen::repeatValue && state.valueChoice==1) {
      repeatPress=true;repeatObserved=now;repeatArmed=false;
      const bool allowed=!consumed && !stable && !asleep && !dimmed && visible &&
        m.authorized && m.controlsEnabled && m.ready && !m.controlPending &&
        m.target.exists && m.target.ref==state.entity && m.target.token==state.openedToken &&
        (m.target.capabilities&controlActionBit(state.action)) && state.draftValue>=1 && state.draftValue<=127;
      if(allowed) {
        repeatWitness={state.entity,state.openedToken,state.action,-1,state.repeatStep,m.authGeneration,int8_t(state.draftValue)};
        repeatConfiguration=m.authConfigurationGeneration;repeatModel=m.modelRevision;repeatRestore=m.restoreRevision;
        repeatArmed=true;
      } else {consumed=true;state.notice=visible?Notice::refused:Notice::wait;noticeSince=now;}
    }
    raw = pressed;
    rawSince = now;
  }
  if(raw != stable && uint32_t(now - rawSince) >= 40) {
    stable = raw;
    if(stable) {
      pressedSince = now;
      held = false;
      if(asleep || dimmed) {
        consumed = true;
        if(quickEnabled && !m.anyMoving && !m.controlPending && state.screen==Screen::status){state.screen=Screen::quickHome;state.selected=0;}
      }
      touch(now);
      authActivity = true;
      editorActivity = now;
    } else {
      if(!consumed && !held)
        shortPress(now, m, visible);
      consumed = held = false;repeatPress=repeatArmed=false;
    }
  }
  if(stable && !consumed && !held && uint32_t(now - pressedSince) >= 800) {
    held = true;
    if(repeatPress) {
      consumed=true;
      if(repeatArmed && repeatCurrent(m) && visible && state.screen==Screen::repeatValue &&
         state.valueChoice==1 && state.action==repeatWitness.action &&
         state.draftValue==repeatWitness.repeat && state.repeatStep==repeatWitness.step) {
        const auto request=repeatWitness;
        cancelValue();
        submitControl(now,m,true,request.action,-1,request.step,request.repeat);
        state.repeatHeld=hasCommand;repeatArmed=false;
      }
    } else if(state.screen == Screen::status) {
      state.screen = Screen::details;
      state.detailsOffset = 0;
    } else
      goBack();
  }
  if(turn) {
    const bool waking = asleep || dimmed;
    touch(now);
    authActivity = true;
    editorActivity = now;
    if(stable || raw) {
      consumed = true;
      return;
    }
    if(waking) {
      if(quickEnabled && !m.anyMoving && !m.controlPending && state.screen==Screen::status){state.screen=Screen::quickHome;state.selected=0;}
      return;
    }
    state.notice = Notice::none;
    if(state.screen == Screen::unlock) {
      editor.turn(turn);
      editorView();
    } else if(state.screen == Screen::details) {
      int offset = int(state.detailsOffset) + (turn > 0 ? 1 : -1);
      const int last = detailsRowCount - detailsVisibleRows;
      state.detailsOffset = offset < 0 ? 0 : offset > last ? last : offset;
    } else if(state.screen == Screen::status) {
      state.screen = quickEnabled?Screen::quickHome:Screen::home;
      state.selected = 0;
    } else if(state.screen==Screen::quickHome || state.screen==Screen::quickManage || state.screen==Screen::quickActions) {
      const int last=state.screen==Screen::quickActions?5:4;
      const int next=int(state.selected)+(turn>0?1:-1);state.selected=next<0?0:next>last?last:next;
    } else if(state.screen==Screen::quickSave) {state.valueChoice=turn>0?1:0;
    } else if(state.screen==Screen::controllerMenu || state.screen==Screen::configureMenu || state.screen==Screen::networkMenu) {
      const int last=state.screen==Screen::controllerMenu?4:state.screen==Screen::configureMenu?7:1;
      const int next=int(state.selected)+(turn>0?1:-1);state.selected=next<0?0:next>last?last:next;
    } else if(state.screen==Screen::home || state.screen==Screen::roomKinds) {
      const int last=state.screen==Screen::home?(quickEnabled?2:homeRows-1):1;
      int next=int(state.selected)+(turn>0?1:-1);
      state.selected=next<0?0:next>last?last:next;
    } else if(state.screen==Screen::catalogue) {
      const int last=m.page.total?m.page.total-1:0;
      int next=int(state.cursor)+(turn>0?1:-1);
      state.cursor=next<0?0:next>last?last:next;
      const unsigned row=state.cursor>=m.page.offset?state.cursor-m.page.offset:catalogueRows;
      state.anchored=row<m.page.count;
      if(state.anchored) state.anchorId=m.page.rows[row].id;
    } else if(state.screen==Screen::settings || state.screen==Screen::administration || state.screen==Screen::system) formInput.turn=turn>0?1:-1;
    else if(state.screen == Screen::brightness) {
      if(state.setting == 0) {
        int index = 0;
        while(index < 7 && levels[index] != state.level)
          ++index;
        index += turn > 0 ? 1 : -1;
        state.level = levels[index < 0 ? 0 : index > 7 ? 7 : index];
      } else {
        uint16_t &value = state.setting == 1 ? state.dimSeconds : state.offSeconds;
        int index = 0;
        while(index < 8 && delays[index] != value)
          ++index;
        index += turn > 0 ? 1 : -1;
        value = delays[index < 0 ? 0 : index > 8 ? 8 : index];
      }
    } else if(state.screen==Screen::control || state.screen==Screen::actions || state.screen==Screen::repeatActions) {
      const auto menu=state.screen==Screen::repeatActions?repeatMenu(m.target.descriptor):controlMenu(m.target.capabilities,state.screen==Screen::control,repeatableControls(m.target.descriptor)!=0);
      int index=menu.index(state.action);if(index<0) index=0;
      index+=turn>0?1:-1;if(index<0) index=0;if(index>=menu.count) index=menu.count?menu.count-1:0;
      if(menu.count) {state.action=menu.actions[index];if(state.screen==Screen::actions || state.screen==Screen::repeatActions) state.selected=uint8_t(index);}
    } else if(state.screen==Screen::favouriteObserve || state.screen==Screen::favouriteConfirm) {
      state.valueChoice=turn>0?1:0;
    } else if(state.screen==Screen::value || state.screen==Screen::repeatValue) {
      if(!state.valueChoice) {valueEditor.rotate(turn,now);valueView();}
      else {state.valueChoice=turn>0?2:1;valueEditor.setStep(1,now);}
    }
  }
  // Inactivity returns to status but never navigates away during estimated motion.
  if(!moving && !keepUpdate && uint32_t(now - activity) >= (state.offSeconds ? uint32_t(state.offSeconds) * 1000 : 30000)) {
    if(state.screen == Screen::unlock) { editor.cancel(); editorView(); }
    if(state.screen == Screen::brightness) { setPreferences(brightnessBefore); savePending=false; }
    cancelValue();discardQuick();
    state.screen = Screen::status;
    state.action = Action::stop;
  }
}
void Controller::brightnessResult(uint32_t now, bool saved) {
  state.notice = saved ? Notice::saved : Notice::saveFailed;
  noticeSince = now;
}
bool Controller::take(Command &out) {
  if(!hasCommand)
    return false;
  out = pending;
  hasCommand = false;
  return true;
}
void Controller::result(uint32_t now,DispatchResult result) {
  // Completion/refusal must be visible while the physical gesture remains
  // consumed. Queued admission still needs the release-to-cancel projection.
  if(repeatPress && result!=DispatchResult::queued) state.repeatHeld=false;
  switch(result) {
  case DispatchResult::queued: state.notice=Notice::queued; break;
  case DispatchResult::done: state.notice=Notice::done; break;
  case DispatchResult::partial: state.notice=Notice::partial; break;
  case DispatchResult::stale: state.notice=Notice::changed; break;
  case DispatchResult::busy: state.notice=Notice::busy; break;
  case DispatchResult::denied: state.notice=Notice::unlockRequired; break;
  case DispatchResult::idle: state.notice=Notice::idle; break;
  case DispatchResult::cancelled: state.notice=Notice::cancelled; break;
  case DispatchResult::expired: state.notice=Notice::expired; break;
  case DispatchResult::cancelledBusy: state.notice=Notice::stopNotQueued; break;
  default: state.notice=Notice::refused; break;
  }
  noticeSince = now;
}
int8_t Encoder::sample(uint8_t ab) {
  ab &= 3;
  if((previous ^ ab) == 3) {
    steps = 0;
    previous = ab;
    return 0;
  }
  static const int8_t delta[16] = {0, -1, 1, 0, 1, 0, 0, -1, -1, 0, 0, 1, 0, 1, -1, 0};
  steps += delta[(previous << 2) | ab];
  previous = ab;
  // Manufacturer uses TWO03: physical detents at both 00 and 11.
  if(ab != 0 && ab != 3)
    return 0;
  const int8_t result = steps >= 2 ? 1 : steps <= -2 ? -1 : 0;
  steps = 0;
  return result;
}
} // namespace LocalUi
