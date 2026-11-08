// Exercise the production adapter with an in-memory model and request sink.
// No network, GPIO or radio implementation is linked into this executable.
#define SOMFY_H
#include "LocalUiAdapter.h"
#include "ShadeRequests.h"
#include "ShadeSettings.h"
#include "LocalUiAuthorizationAdapter.h"
#include <assert.h>
#include <stdio.h>
#include <vector>
struct TestShade : ShadeSettings, RemoteSettings {
  char name[21] = "Test";
  bool paired = true, idle = true;
  radio_proto proto = radio_proto::RTS;
  bool light=false;
  uint8_t gpioUp=0,gpioDown=0,gpioMy=0,gpioFlags=0,roomId=0;
  struct Remote { uint32_t getRemoteAddress() const { return 0; } } linkedRemotes[7];
  uint32_t address = 100;
  struct State { float currentPos = 50; } value;
  uint32_t getRemoteAddress() const { return address; }
  bool isIdle() const { return idle; }
  const State &state() const { return value; }
  float transformPosition(float v) const { return v; }
};
struct TestGroup : RemoteSettings {
  char name[21] = "Group";
  uint8_t linkedShades[3] = {1, 2, 0};
  radio_proto proto = radio_proto::RTS;
  uint32_t getRemoteAddress() const { return 300; }
};
struct SomfyShadeController {
  TestShade shades[2];
  TestGroup group;
  struct Radio {
    struct Config {
      bool enabled=true,noiseDetection=false; uint8_t type=56,SCKPin=11,TXPin=3,RXPin=38,
      MOSIPin=9,MISOPin=10,CSNPin=12; radio_proto proto=radio_proto::RTS;
      int8_t txPower=10; float frequency=433.42,deviation=47.60,rxBandwidth=99.97;
    } config;
    bool available() const { return true; }
    bool txSuspended() const { return false; }
  } transceiver;
  TestShade *getShadeById(uint8_t id) { return id >= 1 && id <= 2 ? &shades[id - 1] : nullptr; }
  TestGroup *getGroupById(uint8_t id) { return id == 1 ? &group : nullptr; }
} somfy;
static LocalUi::SecuritySnapshot security{LocalUi::AuthMode::none,true,false,0,0,0};
LocalUi::SecuritySnapshot LocalUi::pollAuthorization() { return security; }
// This isolated sink replaces only the final dispatcher boundary.
static std::vector<shade_request_t> submitted;
static shade_request_result_t submissionResult = shade_request_result_t::done;
struct RecordingRequests {
  shade_request_result_t run(const shade_request_t &r) {
    submitted.push_back(r);
    return submissionResult;
  }
} shadeRequests;
#define ShadeRequests RecordingRequests
#include "../../src/LocalUiAdapter.cpp"
#undef ShadeRequests
static LocalUi::Command command(uint8_t selected, LocalUi::Action action) {
  LocalUi::focusAuthorizationTarget(selected);
  const auto m = LocalUi::snapshotModel(true);
  const auto &t = m.targets[selected];
  return {selected, action, t.identity, {t.members[0], t.members[1]}, m.authGeneration, t.configuration};
}
int main() {
  using namespace LocalUi;
  const auto boot=snapshotModel(false);
  assert(!boot.authorized && boot.authMode==AuthMode::unavailable);
  Controller bootInput;
  bootInput.tick(0,boot,1,false,false);
  for(unsigned i=0;i<4;i++) bootInput.tick(10+i,boot,1,false,false);
  bootInput.tick(100,boot,0,true,false); bootInput.tick(141,boot,0,true,false);
  bootInput.tick(180,boot,0,false,false); bootInput.tick(221,boot,0,false,false);
  assert(bootInput.view().screen!=Screen::brightness);
  DisplayPreferences bootSave; Command bootCommand;
  assert(!bootInput.takePreferences(bootSave) && !bootInput.take(bootCommand));
  somfy.shades[1].address = 200;
  // Real reported configuration: paired RTS devices of category shutter (4).
  auto m = snapshotModel(true);
  assert(m.targets[0].valid && m.targets[1].valid && m.targets[2].valid);
  assert(dispatch(command(0, Action::up)));
  assert(submitted.back().id == 1 && submitted.back().command == somfy_commands::Up);
  assert(dispatch(command(2, Action::down)));
  assert(submitted.back().type == shade_request_type_t::group_command);
  assert(submitted.back().command == somfy_commands::Down);
  // Device category must not be an extra UI eligibility policy.
  for(unsigned type = 0; type <= static_cast<unsigned>(shade_types::rgate1); ++type) {
    somfy.shades[0].shadeType = static_cast<shade_types>(type);
    assert(snapshotModel(true).targets[0].valid);
    assert(dispatch(command(0, Action::down)));
  }
  // Identity and STOP/MY safeguards remain intact.
  auto stale = command(0, Action::up);
  ++somfy.shades[0].address;
  const auto count = submitted.size();
  assert(!dispatch(stale) && submitted.size() == count);
  assert(!dispatch(command(2, Action::stop)) && submitted.size() == count);
  somfy.shades[1].idle = false;
  assert(dispatch(command(2, Action::stop)));
  assert(submitted.size() == count + 1 && submitted.back().id == 2);
  assert(submitted.back().command == somfy_commands::My);
  // A moving pair uses the same shared identity as its Up/Down press.
  somfy.shades[0].idle = false;
  const auto bothMoving = submitted.size();
  assert(dispatch(command(2, Action::stop)));
  assert(submitted.size() == bothMoving + 1);
  assert(submitted.back().type == shade_request_type_t::group_command);
  assert(submitted.back().id == 1 && submitted.back().command == somfy_commands::My);
  // A refused group Stop is reported without extra individual commands.
  submissionResult = shade_request_result_t::radio_refused;
  const auto refused = submitted.size();
  assert(!dispatch(command(2, Action::stop)));
  assert(submitted.size() == refused + 1);
  assert(submitted.back().type == shade_request_type_t::group_command);
  submissionResult = shade_request_result_t::done;
  // A single selection still addresses only itself even if both are moving.
  assert(dispatch(command(0, Action::stop)));
  assert(submitted.back().type == shade_request_type_t::shade_command);
  assert(submitted.back().id == 1 && submitted.back().command == somfy_commands::My);
  somfy.shades[1].idle = true;
  const auto bedroomOnly = submitted.size();
  assert(dispatch(command(2, Action::stop)));
  assert(submitted.size() == bedroomOnly + 1);
  assert(submitted.back().type == shade_request_type_t::shade_command);
  assert(submitted.back().id == 1 && submitted.back().command == somfy_commands::My);
  security={AuthMode::pin,false,false,1,1,0};
  const auto secureBoot=submitted.size();
  assert(!dispatch(command(0,Action::up)) && !dispatch(command(0,Action::stop)));
  assert(submitted.size()==secureBoot);
  security.allowed=true; security.generation=2;
  somfy.shades[0].idle=somfy.shades[1].idle=false;
  const auto authorized=command(2,Action::stop);
  security.allowed=false; security.expired=true; security.generation=3;
  const auto expired=submitted.size();
  snapshotModel(true); // timeout itself emits nothing
  assert(submitted.size()==expired);
  assert(dispatch(command(2,Action::stop)));
  assert(submitted.back().type==shade_request_type_t::group_command);
  assert(!dispatch(command(2,Action::up)));
  somfy.shades[0].idle=true; snapshotModel(true);
  somfy.shades[0].idle=false; // new movement cannot reacquire member one's lease
  const auto secondOnly=submitted.size();
  assert(dispatch(command(2,Action::stop)) && submitted.size()==secondOnly+1);
  assert(submitted.back().type==shade_request_type_t::shade_command && submitted.back().id==2);
  somfy.shades[1].idle=true; snapshotModel(true);
  somfy.shades[1].idle=false;
  assert(!dispatch(command(2,Action::stop)));
  security.allowed=true; security.expired=false; security.generation=4;
  snapshotModel(true);
  assert(!dispatch(authorized)); // changed ordinary session generation
  security.allowed=false; security.expired=true; security.generation=5;
  snapshotModel(true);
  ++somfy.shades[0].upTime; // same RF identity, changed configuration
  assert(!dispatch(command(2,Action::stop)));
  security.allowed=true; security.expired=false; security.generation=6;
  snapshotModel(true);
  security.allowed=false; security.expired=true; security.generation=7;
  ++security.configurationGeneration;
  assert(!dispatch(command(0,Action::stop)));
  security.allowed=true; security.expired=false; security.generation=8;
  focusAuthorizationTarget(0); snapshotModel(true);
  security.allowed=false; security.expired=true; security.generation=9;
  const auto focusExpiry=submitted.size();
  const auto focused=snapshotModel(true);
  assert(focused.targets[0].stopLease && !focused.targets[1].stopLease && !focused.targets[2].stopLease);
  assert(submitted.size()==focusExpiry); // expiry does not send STOP itself
  assert(!dispatch(command(1,Action::stop))); // another moving, never-focused target is not authorized
  assert(submitted.size()==focusExpiry);
  security.allowed=true; security.expired=false; security.generation=10;
  focusAuthorizationTarget(0); snapshotModel(true);
  focusAuthorizationTarget(255); snapshotModel(true); // ordinary menu is not another target
  security.allowed=false; security.expired=true; security.generation=11;
  const auto menuExpiry=submitted.size();
  const auto inMenu=snapshotModel(true);
  assert(inMenu.targets[0].stopLease && !inMenu.targets[1].stopLease);
  assert(submitted.size()==menuExpiry);
  assert(dispatch(command(0,Action::stop))); // return to same target keeps reachable STOP
  somfy.shades[0].idle=true; focusAuthorizationTarget(255); snapshotModel(true);
  somfy.shades[0].idle=false;
  assert(!dispatch(command(0,Action::stop))); // idle in menu ends lease permanently
  assert(!dispatch(command(1,Action::stop)));
  assert(submitted.size()>=expired);
  puts("Production UI adapter: shutter/all-category eligibility, dispatch and identity/STOP checks passed");
}
