#pragma once
#include <stdint.h>
#include "LocalCredentialEditor.h"
namespace LocalUi {
constexpr uint8_t detailsRowCount = 10, detailsVisibleRows = 6;
enum class Screen : uint8_t { status, choose, control, details, brightness, unlock };
enum class Action : uint8_t { up, stop, down };
enum class Notice : uint8_t { none, preview, queued, refused, idle, changed, wait, saved, saveFailed, unlockRequired, unlockWrong, unlockWait, unlockUnavailable, unlocked };
struct Target {
  char name[21] = {};
  uint32_t identity = 0;    // model RF identity; never displayed
  uint64_t configuration = 0;
  uint32_t members[2] = {}; // exact group member identities; never displayed
  bool valid = false, moving = false, stopLease = false;
  int8_t open[2] = {-1, -1}; // estimates; second member only for the group
};
struct Model {
  Target targets[3];
  bool ready = false, controlsEnabled = false;
  AuthMode authMode = AuthMode::none;
  bool authorized = true;
  uint32_t authGeneration = 0, authConfigurationGeneration = 0, retryAfterMs = 0;
};
struct DisplayPreferences {
  uint8_t brightness = 32;
  uint16_t dimSeconds = 10, offSeconds = 20; // zero disables that timeout
};
struct View {
  Screen screen = Screen::status;
  uint8_t selected = 0, level = 32, setting = 0, detailsOffset = 0;
  uint16_t dimSeconds = 10, offSeconds = 20;
  Action action = Action::stop;
  Notice notice = Notice::none;
  uint16_t credentialChoice = 0;
  uint8_t credentialLength = 0, credentialMaximum = 0;
  bool credentialUser = false, credentialByte = false;
  uint32_t retryAfterMs = 0;
};
struct Command {
  uint8_t selected = 0;
  Action action = Action::stop;
  uint32_t identity = 0;
  uint32_t members[2] = {};
  uint32_t authGeneration = 0;
  uint64_t configuration = 0;
};
class Controller {
  View state;
  Command pending;
  CredentialEditor editor;
  AuthMode editorMode = AuthMode::none;
  Screen afterUnlock = Screen::control;
  DisplayPreferences brightnessBefore;
  uint32_t brightnessConfigurationGeneration = 0;
  uint32_t editorActivity = 0, editorConfigurationGeneration = 0;
  bool authActivity = false;
  void editorView();
  bool hasCommand = false, started = false, raw = false, stable = false;
  bool consumed = false, held = false, asleep = false;
  bool dimmed = false, savePending = false;
  bool backRaw = false, backStable = false, backConsumed = false;
  uint32_t backSince = 0;
  uint32_t rawSince = 0, pressedSince = 0, activity = 0, noticeSince = 0, openedIdentity = 0;
  uint32_t openedMembers[2] = {};
  uint64_t openedConfiguration = 0;
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
  void unlockResult(uint32_t now, AuthResult);
  bool takeCredentials(Credentials &out) { return editor.take(out); }
  bool takeActivity() { bool touched = authActivity; authActivity = false; return touched; }

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
