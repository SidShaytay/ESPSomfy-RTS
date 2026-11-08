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
  hasCommand=false; state.notice=Notice::none; state.action=Action::stop;
  if(state.screen==Screen::unlock) { editor.cancel(); editorView(); state.screen=afterUnlock; }
  else if(state.screen==Screen::brightness) { setPreferences(brightnessBefore); savePending=false; state.screen=Screen::home; state.selected=4; }
  else if(state.screen==Screen::control) { state.screen=Screen::catalogue; }
  else if(state.screen==Screen::catalogue) {
    if(state.catalogue!=CatalogueKind::rooms && state.room!=allRooms) { state.screen=Screen::roomKinds; state.selected=0; }
    else { state.screen=Screen::home; state.selected=state.catalogue==CatalogueKind::rooms?0:state.catalogue==CatalogueKind::devices?1:2; }
  } else if(state.screen==Screen::roomKinds) openCatalogue(CatalogueKind::rooms,allRooms);
  else if(state.screen==Screen::settings) settingsInput.back=true;
  else if(state.screen==Screen::actions || state.screen==Screen::value) {cancelValue();state.screen=Screen::control;}
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
  valueEditor.cancel();state.draftValue=0;state.valueChoice=0;
}
void Controller::submitControl(uint32_t now,const Model &m,bool visible,Action action,int8_t value,uint8_t step) {
  const auto &t=m.target;
  if(!t.exists || t.ref!=state.entity || !state.openedToken || t.token!=state.openedToken)
    state.notice=Notice::changed;
  else if(!m.authorized && !(action==Action::stop && t.stopLease)) beginUnlock(now,m,Screen::control);
  else if(!visible) state.notice=Notice::wait;
  else if(!(t.capabilities & controlActionBit(action))) state.notice=Notice::refused;
  else if((action==Action::stop && !t.moving) || (action==Action::tiltStop && !t.tiltMoving)) state.notice=Notice::idle;
  else if(!m.controlsEnabled) state.notice=Notice::preview;
  else if(!m.ready) state.notice=Notice::refused;
  else {
    pending={state.entity,state.openedToken,action,value,step,m.authGeneration};
    hasCommand=true;state.notice=Notice::queued;
  }
  if(state.screen!=Screen::unlock) state.screen=Screen::control;
  state.action=defaultControlAction(t.capabilities);noticeSince=now;
}
void Controller::shortPress(uint32_t now,const Model &m,bool visible) {
  switch(state.screen) {
  case Screen::unlock:
    if(editor.press()==EditorResult::cancelled) state.screen=afterUnlock;
    editorView(); break;
  case Screen::status: state.screen=Screen::home; state.selected=0; break;
  case Screen::details: state.screen=Screen::status; break;
  case Screen::home:
    if(state.selected<3) {
      openCatalogue(state.selected==0?CatalogueKind::rooms:state.selected==1?CatalogueKind::devices:CatalogueKind::groups,allRooms);
    } else if(state.selected==3) {
      if(!m.authorized) beginUnlock(now,m,Screen::home);
      else { state.screen=Screen::settings; settingsInput={}; }
    } else if(state.selected==4) {
      if(!m.authorized) beginUnlock(now,m,Screen::home);
      else {
        brightnessBefore=preferences(); brightnessConfigurationGeneration=m.authConfigurationGeneration;
        state.screen=Screen::brightness; state.setting=0;
      }
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
      state.entity={state.catalogue==CatalogueKind::devices?EntityKind::device:EntityKind::group,entry.id};
      chosenIdentity=entry.identity; chosenRevision=entry.revision;
      chosenModelRevision=m.modelRevision; chosenRestoreRevision=m.restoreRevision;
      state.openedToken=0; bindTarget=true; state.screen=Screen::control; state.action=Action::stop;
      state.authorizedSelection=m.authorized; state.selectionAuthGeneration=m.authGeneration;
    }
    break;
  }
  case Screen::roomKinds:
    openCatalogue(state.selected?CatalogueKind::groups:CatalogueKind::devices,state.room); break;
  case Screen::settings:
    if(visible) settingsInput.activate=true; else state.notice=Notice::wait;
    break;
  case Screen::brightness:
    if(state.setting<2) ++state.setting;
    else if(!visible) state.notice=Notice::wait;
    else { savePending=true; state.screen=Screen::home; state.selected=4; }
    break;
  case Screen::control:
    if(state.action==Action::count) {
      if(!m.target.exists || m.target.ref!=state.entity || !state.openedToken || m.target.token!=state.openedToken) state.notice=Notice::changed;
      else if(!visible) state.notice=Notice::wait;
      else { const auto menu=controlMenu(m.target.capabilities,false);
        if(menu.count) {state.screen=Screen::actions;state.selected=0;state.action=menu.actions[0];}
      }
    } else submitControl(now,m,visible,state.action);
    break;
  case Screen::actions: {
    const auto menu=controlMenu(m.target.capabilities,false);
    if(!m.target.exists || m.target.token!=state.openedToken || m.target.ref!=state.entity) state.notice=Notice::changed;
    else if(!visible) state.notice=Notice::wait;
    else if(state.selected>=menu.count || menu.actions[state.selected]!=state.action) state.notice=Notice::changed;
    else if(!m.authorized) beginUnlock(now,m,Screen::control);
    else if(controlNeedsValue(state.action)) {
      const bool step=controlIsStep(state.action);
      const int initial=step?1:state.action==Action::position?m.target.position:m.target.tiltPosition;
      if(initial<0 || !valueEditor.beginInteger(initial,step?1:0,step?127:100,1,now)) state.notice=Notice::valueUnavailable;
      else {
        state.screen=Screen::value;state.valueChoice=0;valueView();
        valueAuthorizationGeneration=m.authGeneration;valueConfigurationGeneration=m.authConfigurationGeneration;
      }
    } else submitControl(now,m,visible,state.action);
    break;
  }
  case Screen::value:
    if(!visible) state.notice=Notice::wait;
    else if(state.valueChoice==0) { state.valueChoice=2;valueEditor.setStep(1,now); }
    else if(state.valueChoice==2) {cancelValue();state.screen=Screen::control;state.action=Action::stop;}
    else {
      int64_t number=0;valueEditor.apply(now);
      if(valueEditor.takeNumber(number)) {
        const auto action=state.action;cancelValue();
        submitControl(now,m,visible,action,controlIsStep(action)?-1:int8_t(number),controlIsStep(action)?uint8_t(number):0);
      } else {cancelValue();state.screen=Screen::control;state.action=Action::stop;state.notice=Notice::changed;}
    }
    break;
  }
  if(state.notice!=Notice::none) noticeSince=now;
}
void Controller::tick(uint32_t now, const Model &m, int8_t turn, bool pressed, bool visible,
                      bool backPressed) {
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
    const auto primary=controlMenu(m.target.capabilities,true);
    if(primary.index(state.action)<0 && primary.count) state.action=primary.actions[0];
    bindTarget=false;
  }
  if(state.screen==Screen::control && m.target.ref==state.entity && m.target.token==state.openedToken) {
    const auto menu=controlMenu(m.target.capabilities,true);
    if(menu.index(state.action)<0) state.action=defaultControlAction(m.target.capabilities);
  }
  if(authorizeAfterUnlock && m.authorized) {
    if(m.target.ref==state.entity && m.target.token==state.openedToken) {
      state.authorizedSelection=true; state.selectionAuthGeneration=m.authGeneration;
    }
    authorizeAfterUnlock=false;
  }
  if(state.screen==Screen::value) {
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
    state.screen = Screen::home;
    state.notice = Notice::unlockRequired;
    noticeSince = now;
    consumed = true;
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
  if(state.offSeconds && uint32_t(now - activity) >= uint32_t(state.offSeconds) * 1000)
    asleep = true;
  else if(state.dimSeconds && uint32_t(now - activity) >= uint32_t(state.dimSeconds) * 1000)
    dimmed = true;
  if(state.notice != Notice::none && uint32_t(now - noticeSince) >= 2500)
    state.notice = Notice::none;
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
    raw = pressed;
    rawSince = now;
  }
  if(raw != stable && uint32_t(now - rawSince) >= 40) {
    stable = raw;
    if(stable) {
      pressedSince = now;
      held = false;
      if(asleep || dimmed)
        consumed = true;
      touch(now);
      authActivity = true;
      editorActivity = now;
    } else {
      if(!consumed && !held)
        shortPress(now, m, visible);
      consumed = held = false;
    }
  }
  if(stable && !consumed && !held && uint32_t(now - pressedSince) >= 800) {
    held = true;
    if(state.screen == Screen::status) {
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
    if(waking)
      return;
    state.notice = Notice::none;
    if(state.screen == Screen::unlock) {
      editor.turn(turn);
      editorView();
    } else if(state.screen == Screen::details) {
      int offset = int(state.detailsOffset) + (turn > 0 ? 1 : -1);
      const int last = detailsRowCount - detailsVisibleRows;
      state.detailsOffset = offset < 0 ? 0 : offset > last ? last : offset;
    } else if(state.screen == Screen::status) {
      state.screen = Screen::home;
      state.selected = 0;
    } else if(state.screen==Screen::home || state.screen==Screen::roomKinds) {
      const int last=state.screen==Screen::home?homeRows-1:1;
      int next=int(state.selected)+(turn>0?1:-1);
      state.selected=next<0?0:next>last?last:next;
    } else if(state.screen==Screen::catalogue) {
      const int last=m.page.total?m.page.total-1:0;
      int next=int(state.cursor)+(turn>0?1:-1);
      state.cursor=next<0?0:next>last?last:next;
      const unsigned row=state.cursor>=m.page.offset?state.cursor-m.page.offset:catalogueRows;
      state.anchored=row<m.page.count;
      if(state.anchored) state.anchorId=m.page.rows[row].id;
    } else if(state.screen==Screen::settings) settingsInput.turn=turn>0?1:-1;
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
    } else if(state.screen==Screen::control || state.screen==Screen::actions) {
      const auto menu=controlMenu(m.target.capabilities,state.screen==Screen::control);
      int index=menu.index(state.action);if(index<0) index=0;
      index+=turn>0?1:-1;if(index<0) index=0;if(index>=menu.count) index=menu.count?menu.count-1:0;
      if(menu.count) {state.action=menu.actions[index];if(state.screen==Screen::actions) state.selected=uint8_t(index);}
    } else if(state.screen==Screen::value) {
      if(!state.valueChoice) {valueEditor.rotate(turn,now);valueView();}
      else {state.valueChoice=turn>0?2:1;valueEditor.setStep(1,now);}
    }
  }
  // Inactivity returns to status but never navigates away during estimated motion.
  if(!moving && uint32_t(now - activity) >= (state.offSeconds ? uint32_t(state.offSeconds) * 1000 : 30000)) {
    if(state.screen == Screen::unlock) { editor.cancel(); editorView(); }
    if(state.screen == Screen::brightness) { setPreferences(brightnessBefore); savePending=false; }
    cancelValue();
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
  switch(result) {
  case DispatchResult::queued: state.notice=Notice::queued; break;
  case DispatchResult::done: state.notice=Notice::done; break;
  case DispatchResult::partial: state.notice=Notice::partial; break;
  case DispatchResult::stale: state.notice=Notice::changed; break;
  case DispatchResult::busy: state.notice=Notice::busy; break;
  case DispatchResult::denied: state.notice=Notice::unlockRequired; break;
  case DispatchResult::idle: state.notice=Notice::idle; break;
  case DispatchResult::cancelled: state.notice=Notice::cancelled; break;
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
