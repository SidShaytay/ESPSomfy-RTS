#include "Arduino.h"
#include "SomfyFrame.h"
#include "SettingsLoadState.h"
#include "RadioUpdateActivity.h"
#include "esp_log.h"
#include <assert.h>
#include <stdio.h>
#include <string>
static const char *TAG = "Group test";
#define TRANSCEIVER_H
// Only physical radio/shutdown and NVS are replaced. The actual model header,
// group dispatcher, member behaviour and elapsed-time movement stay production.
struct Transceiver {
  struct {
    SettingsLoadStatus loadStatus{SettingsLoadState::loaded};
    uint8_t type = 56;
    radio_proto proto = radio_proto::RTS;
  } config;
  bool sealed = false;
  bool counterEditUncertain() const { return false; }
  bool restartQuiescent() const { return true; }
  bool installPauseHeld(const RadioInstallToken &) const { return false; }
  bool sealForRestart() {
    sealed = true;
    return true;
  }
  void reopenAfterRefusedRestart() { sealed = false; }
};
#include "GitUpdate.h"
#include "SafeRestart.h"
#include "LoopQueue.h"
#include "SettingsDurability.h"
#include "Somfy.h"
#include "QualifiedSettings.h"
#include "Utils.h"
#include "esp_timer.h"
// This restart fixture never installs a counter quarantine.
bool counterCommandAllowed(uint32_t,somfy_commands){return true;}
static unsigned radioCalls = 0, nvsCalls = 0, commits = 0, shutdowns = 0;
static uint32_t nowMs = 1;
static unsigned long clockNow() { return nowMs; }
SomfyShadeController::SomfyShadeController() : startingAddress(100) {modelLoad.bootResult(ModelLoadStatus::loaded);}
SomfyShadeController somfy;
ConfigSettings settings; // Qualified security boot; real NVS/auth failures have separate tests.
LoopQueue loopQueue;

uint32_t LoopQueue::pending() const { return 0; }
bool LoopQueue::sealForRestart() { return true; }
bool LoopQueue::restartSealHeld() const { return false; }
void LoopQueue::reopenAfterRefusedRestart() {}
SomfyShade *SomfyShadeController::getShadeById(uint8_t id) {
  for (auto &s : shades)
    if (id && id != 255 && s.getShadeId() == id)
      return &s;
  return nullptr;
}
ModelCommitResult SomfyShadeController::commit() {
  ++commits;somfy.isDirty=false;
  noteModelCommit({ModelCommitState::saved});
  return {ModelCommitState::saved, ModelCommitStage::none};
}
void performLegacyShutdown() {
  ++shutdowns;
  assert(gitUpdate.busy() && somfy.transceiver.sealed);
}
// Fake accepted physical transport records the same last-frame evidence;
// readiness itself must never call this endpoint or change the counter.
bool SomfyRemote::sendCommand(somfy_commands command) {
  return sendCommand(command, 1, 0);
}
bool SomfyRemote::sendCommand(somfy_commands command, uint8_t, uint8_t) {
  ++radioCalls;
  ++nvsCalls;
  ++lastRollingCode;
  lastFrame.cmd = command;
  lastFrame.valid = true;
  return true;
}
send_result_t SomfyRemote::sendCommandAfter(somfy_commands command, uint8_t repeat,
                                              uint8_t step, uint16_t, bool, bool) {
  return {SomfyRemote::sendCommand(command, repeat, step), radioCalls};
}
bool SomfyRemote::isLastCommand(somfy_commands command) {
  return lastFrame.cmd == command;
}
uint16_t SomfyRemote::setRollingCode(uint16_t value) {
  ++nvsCalls;
  return value;
}
void SomfyRemote::triggerGPIOs(somfy_frame_t &) { assert(false); }
bool SomfyShade::sendCommand(somfy_commands) {
  assert(false);
  return false;
}
bool SomfyShade::sendCommand(somfy_commands, uint8_t, uint8_t) {
  assert(false);
  return false;
}
void SomfyShade::triggerGPIOs(somfy_frame_t &) { assert(false); }
bool SomfyShade::gpioCommandAllowed(somfy_commands) {
  assert(false);
  return false;
}
send_result_t SomfyShade::send(somfy_commands, uint8_t, uint8_t, uint16_t,
                               bool) {
  assert(false);
  return {};
}
void SomfyShade::onSaveWanted() { somfy.isDirty = true; }
void SomfyShade::onSensorFlags() {}
void SomfyShade::emitCommand(somfy_commands, const char *, uint32_t) {}
#define millis clockNow
#include "../../src/SafeRestart.cpp"
#include "production-group-domain.inc"
#undef millis
int main(int argc, char **argv) {
  qualifySettings(settings);
  assert(argc == 2);
  const std::string mode = argv[1];
  stubEspTimerMicros = 1000;
  auto &group = somfy.groups[15];
  group.setGroupId(4);
  group.setRemoteAddress(400);
  group.bitLength = 56;
  auto &shade = somfy.shades[31];
  shade.setShadeId(7);
  shade.setRemoteAddress(700);
  shade.upTime = shade.downTime = 100;
  if(mode=="favorite") {
    group.bitLength=shade.bitLength=80;group.linkedShades[31]=7;
    shade.restore(0,0,50,-1);
    assert(group.sendCommand(somfy_commands::Favorite,3));
    assert(radioCalls==1 && nvsCalls==1 && group.lastFrame.cmd==somfy_commands::Favorite);
    assert(shade.state().target==50 && !shade.idleForAdministration());
    assert(requestRestart(1).reason==RestartReason::activeMovement);
    // Native motor movement completes without any member-addressed STOP or
    // programming. The fake member transport above rejects every such send.
    nowMs+=1000;shade.checkMovement();
    assert(shade.state().currentPos==50 && shade.state().myPos==50);
    assert(shade.idleForAdministration() && radioCalls==1 && nvsCalls==1);
    puts("Group Favorite: one group transmission, member estimate and completion without member RF");return 0;
  }
  const bool unknown = mode.rfind("unknown", 0) == 0, empty = mode == "empty",
             dangling = mode == "dangling";
  const bool up = mode == "up" || mode == "moving" || dangling;
  somfy_commands command = up ? somfy_commands::Up : somfy_commands::Down;
  if (mode == "unknownUp")
    command = somfy_commands::Up;
  if (mode == "unknownDown")
    command = somfy_commands::Down;
  if (mode == "unknownMy")
    command = somfy_commands::My;
  if (mode == "unknownToggle")
    command = somfy_commands::Toggle;
  if (mode == "unknownStepUp")
    command = somfy_commands::StepUp;
  if (mode == "unknownStepDown")
    command = somfy_commands::StepDown;
  if (mode == "unknownFavorite")
    command = somfy_commands::Favorite;
  shade.restore(up ? 100 : 0, 0, 50, -1);
  if (!unknown && !empty)
    group.linkedShades[31] = 7; // physical holes are intentional
  if (dangling) {
    group.linkedShades[0] = 7;
    group.linkedShades[31] = 88;
  }
  assert(shade.idleForAdministration());
  if (!empty)
    assert(group.sendCommand(command, 1));
  if (!unknown && !empty) {
    assert(!shade.idleForAdministration());
    assert(requestRestart(1).reason ==
           RestartReason::activeMovement);
    if (mode != "moving") {
      nowMs = 2;
      shade.checkMovement();
      nowMs = 1002;
      shade.checkMovement();
      assert(shade.idleForAdministration());
      assert(group.direction ==
             (up ? -1 : 1)); // never fabricate clearing direction
    }
  }
  const bool expected = mode == "up" || mode == "down" || empty;
  const auto admission = requestRestart(1);
  assert(admission.admitted == expected);
  if (unknown || dangling)
    assert(admission.reason == RestartReason::unknownGroup);
  const auto rf = radioCalls, nvs = nvsCalls;
  const auto direction = group.direction;
  stubEspTimerMicros+=1000;
  const bool restarted = restartLoop();
  assert(restarted == expected);
  assert(shutdowns == unsigned(expected) && commits >= unsigned(expected));
  assert(radioCalls == rf && nvsCalls == nvs && group.direction == direction);
  puts(
      "Actual group dispatcher + member behaviour completion/readiness passed");
}
