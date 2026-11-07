#include "LocalUi.h"
namespace LocalUi {
namespace {
constexpr uint8_t levels[] = {2, 4, 8, 16, 32, 64, 128, 255};
}
void Controller::setBrightness(uint8_t value) {
  state.level = 32;
  for(auto level : levels)
    if(level == value)
      state.level = value;
}
void Controller::touch(uint32_t now) {
  activity = now;
  asleep = dimmed = false;
}
void Controller::shortPress(uint32_t now, const Model &m, bool visible) {
  switch(state.screen) {
  case Screen::status:
    state.screen = Screen::choose;
    state.selected = 0;
    break;
  case Screen::details:
    state.screen = Screen::status;
    break;
  case Screen::brightness:
    savePending = true;
    state.screen = Screen::choose;
    break;
  case Screen::choose:
    if(state.selected == 4) {
      state.screen = Screen::brightness;
      break;
    }
    if(state.selected == 3) {
      state.screen = Screen::status;
      break;
    }
    state.screen = Screen::control;
    state.action = Action::stop;
    openedIdentity = m.targets[state.selected].identity;
    for(int i = 0; i < 2; i++)
      openedMembers[i] = m.targets[state.selected].members[i];
    break;
  case Screen::control: {
    const auto &t = m.targets[state.selected];
    if(!t.valid || t.identity != openedIdentity || t.members[0] != openedMembers[0] ||
       t.members[1] != openedMembers[1])
      state.notice = Notice::changed;
    else if(!visible)
      state.notice = Notice::wait;
    else if(state.action == Action::stop && !t.moving)
      state.notice = Notice::idle;
    else if(!m.controlsEnabled)
      state.notice = Notice::preview;
    else if(!m.ready)
      state.notice = Notice::refused;
    else {
      pending = {state.selected, state.action, t.identity};
      for(int i = 0; i < 2; i++)
        pending.members[i] = t.members[i];
      hasCommand = true;
      state.notice = Notice::queued;
    }
    state.action = Action::stop;
    noticeSince = now;
    break;
  }
  }
}
void Controller::tick(uint32_t now, const Model &m, int8_t turn, bool pressed, bool visible) {
  if(!started) {
    started = true;
    activity = now;
    raw = stable = pressed;
    consumed = pressed;
    rawSince = pressedSince = now;
  }
  // Stay awake while ANY configured target is estimated moving, even when the
  // user navigates away. A held button never extends this timer by itself.
  bool moving = false;
  for(const auto &t : m.targets)
    moving |= t.valid && t.moving;
  if(moving)
    touch(now);
  if(uint32_t(now - activity) >= 20000)
    asleep = true;
  else if(uint32_t(now - activity) >= 10000)
    dimmed = true;
  if(state.notice != Notice::none && uint32_t(now - noticeSince) >= 2500)
    state.notice = Notice::none;
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
    } else {
      if(!consumed && !held)
        shortPress(now, m, visible);
      consumed = held = false;
    }
  }
  if(stable && !consumed && !held && uint32_t(now - pressedSince) >= 800) {
    held = true;
    state.notice = Notice::none;
    if(state.screen == Screen::control)
      state.screen = Screen::choose;
    else if(state.screen == Screen::brightness) {
      savePending = true;
      state.screen = Screen::choose;
    } else if(state.screen == Screen::status)
      state.screen = Screen::details;
    else
      state.screen = Screen::status;
  }
  if(turn) {
    const bool waking = asleep || dimmed;
    touch(now);
    if(stable || raw) {
      consumed = true;
      return;
    } // turning a held knob cannot also click
    if(waking)
      return;
    state.notice = Notice::none;
    if(state.screen == Screen::status || state.screen == Screen::details) {
      state.screen = Screen::choose;
      state.selected = 0;
    } else if(state.screen == Screen::choose)
      state.selected = (state.selected + (turn > 0 ? 1 : 4)) % 5;
    else if(state.screen == Screen::brightness) {
      int index = 0;
      while(index < 7 && levels[index] != state.level)
        ++index;
      index += turn > 0 ? 1 : -1;
      state.level = levels[index < 0 ? 0 : index > 7 ? 7 : index];
    } else {
      int a = int(state.action) + (turn > 0 ? 1 : -1);
      state.action = Action(a < 0 ? 0 : a > 2 ? 2 : a);
    }
  }
  // Inactivity returns to status but never navigates away during estimated motion.
  if(!moving && uint32_t(now - activity) >= 20000) {
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
void Controller::result(uint32_t now, bool accepted) {
  state.notice = accepted ? Notice::queued : Notice::refused;
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
  if(ab != 3)
    return 0;
  const int8_t result = steps <= -4 ? 1 : steps >= 4 ? -1 : 0;
  steps = 0;
  return result;
}
} // namespace LocalUi
