#include "LocalUi.h"
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
void Controller::goBack() {
  hasCommand = false;
  state.notice = Notice::none;
  state.action = Action::stop;
  if(state.screen == Screen::brightness) {
    savePending = true;
    state.screen = Screen::choose;
  } else if(state.screen == Screen::control)
    state.screen = Screen::choose;
  else
    state.screen = Screen::status;
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
    if(state.setting < 2)
      ++state.setting;
    else {
      savePending = true;
      state.screen = Screen::choose;
    }
    break;
  case Screen::choose:
    if(state.selected == 4) {
      state.screen = Screen::brightness;
      state.setting = 0;
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
    else if(!visible && state.action != Action::stop)
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
  // Stay awake while ANY configured target is estimated moving, even when the
  // user navigates away. A held button never extends this timer by itself.
  bool moving = false;
  for(const auto &t : m.targets)
    moving |= t.valid && t.moving;
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
    if(stable || raw) {
      consumed = true;
      return;
    }
    if(waking)
      return;
    state.notice = Notice::none;
    if(state.screen == Screen::details) {
      int offset = int(state.detailsOffset) + (turn > 0 ? 1 : -1);
      const int last = detailsRowCount - detailsVisibleRows;
      state.detailsOffset = offset < 0 ? 0 : offset > last ? last : offset;
    } else if(state.screen == Screen::status) {
      state.screen = Screen::choose;
      state.selected = 0;
    } else if(state.screen == Screen::choose)
      state.selected = (state.selected + (turn > 0 ? 1 : 4)) % 5;
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
    } else {
      int action = int(state.action) + (turn > 0 ? 1 : -1);
      state.action = Action(action < 0 ? 0 : action > 2 ? 2 : action);
    }
  }
  // Inactivity returns to status but never navigates away during estimated motion.
  if(!moving && uint32_t(now - activity) >= (state.offSeconds ? uint32_t(state.offSeconds) * 1000 : 30000)) {
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
  // Manufacturer uses TWO03: physical detents at both 00 and 11.
  if(ab != 0 && ab != 3)
    return 0;
  const int8_t result = steps >= 2 ? 1 : steps <= -2 ? -1 : 0;
  steps = 0;
  return result;
}
} // namespace LocalUi
