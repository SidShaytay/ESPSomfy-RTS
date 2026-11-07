#pragma once
#include <stdint.h>
namespace LocalUi {
enum class Screen : uint8_t { status, choose, control, details, brightness };
enum class Action : uint8_t { up, stop, down };
enum class Notice : uint8_t { none, preview, queued, refused, idle, changed, wait, saved, saveFailed };
struct Target {
  char name[21] = {};
  uint32_t identity = 0;    // model RF identity; never displayed
  uint32_t members[2] = {}; // exact group member identities; never displayed
  bool valid = false, moving = false;
  int8_t open[2] = {-1, -1}; // estimates; second member only for the group
};
struct Model {
  Target targets[3];
  bool ready = false, controlsEnabled = false;
};
struct DisplayPreferences {
  uint8_t brightness = 32;
  uint16_t dimSeconds = 10, offSeconds = 20; // zero disables that timeout
};
struct View {
  Screen screen = Screen::status;
  uint8_t selected = 0, level = 32, setting = 0;
  uint16_t dimSeconds = 10, offSeconds = 20;
  Action action = Action::stop;
  Notice notice = Notice::none;
};
struct Command {
  uint8_t selected = 0;
  Action action = Action::stop;
  uint32_t identity = 0;
  uint32_t members[2] = {};
};
class Controller {
  View state;
  Command pending;
  bool hasCommand = false, started = false, raw = false, stable = false;
  bool consumed = false, held = false, asleep = false;
  bool dimmed = false, savePending = false;
  bool backRaw = false, backStable = false, backConsumed = false;
  uint32_t backSince = 0;
  uint32_t rawSince = 0, pressedSince = 0, activity = 0, noticeSince = 0, openedIdentity = 0;
  uint32_t openedMembers[2] = {};
  void touch(uint32_t now);
  void goBack();
  void shortPress(uint32_t now, const Model &, bool visible);

public:
  void tick(uint32_t now, const Model &, int8_t turn, bool pressed, bool visible, bool backPressed = false);
  const View &view() const { return state; }
  uint8_t brightness() const { return asleep ? 0 : dimmed ? (state.level < 4 ? 1 : 4) : state.level; }
  bool take(Command &out);
  void result(uint32_t now, bool accepted);
  void brightnessResult(uint32_t now, bool saved);

  void setBrightness(uint8_t value);
  uint8_t preference() const { return state.level; }
  DisplayPreferences preferences() const { return {state.level, state.dimSeconds, state.offSeconds}; }
  void setPreferences(const DisplayPreferences &);
  bool takePreferences(DisplayPreferences &value) {
    if(!savePending)
      return false;
    value = preferences();
    savePending = false;
    return true;
  }
};
class Encoder {
  uint8_t previous = 3;
  int8_t steps = 0;

public:
  int8_t sample(uint8_t ab);
};
} // namespace LocalUi
