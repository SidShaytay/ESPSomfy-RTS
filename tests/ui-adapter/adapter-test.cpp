// Exercise the production adapter with an in-memory model and request sink.
// No network, GPIO or radio implementation is linked into this executable.
#define SOMFY_H
#include "LocalUiAdapter.h"
#include "ShadeRequests.h"
#include <assert.h>
#include <stdio.h>
#include <vector>
struct TestShade {
  char name[21] = "Test";
  bool paired = true, idle = true;
  radio_proto proto = radio_proto::RTS;
  shade_types shadeType = shade_types::shutter;
  uint32_t address = 100;
  struct State { float currentPos = 50; } value;
  uint32_t getRemoteAddress() const { return address; }
  bool isIdle() const { return idle; }
  const State &state() const { return value; }
  float transformPosition(float v) const { return v; }
};
struct TestGroup {
  char name[21] = "Group";
  uint8_t linkedShades[3] = {1, 2, 0};
  radio_proto proto = radio_proto::RTS;
  uint32_t getRemoteAddress() const { return 300; }
};
struct SomfyShadeController {
  TestShade shades[2];
  TestGroup group;
  struct Radio {
    bool available() const { return true; }
    bool txSuspended() const { return false; }
  } transceiver;
  TestShade *getShadeById(uint8_t id) { return id >= 1 && id <= 2 ? &shades[id - 1] : nullptr; }
  TestGroup *getGroupById(uint8_t id) { return id == 1 ? &group : nullptr; }
} somfy;
// This isolated sink replaces only the final dispatcher boundary.
static std::vector<shade_request_t> submitted;
struct RecordingRequests {
  shade_request_result_t run(const shade_request_t &r) {
    submitted.push_back(r);
    return shade_request_result_t::done;
  }
} shadeRequests;
#define ShadeRequests RecordingRequests
#include "../../src/LocalUiAdapter.cpp"
#undef ShadeRequests
static LocalUi::Command command(uint8_t selected, LocalUi::Action action) {
  const auto m = LocalUi::snapshotModel(true);
  const auto &t = m.targets[selected];
  return {selected, action, t.identity, {t.members[0], t.members[1]}};
}
int main() {
  using namespace LocalUi;
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
  puts("Production UI adapter: shutter/all-category eligibility, dispatch and identity/STOP checks passed");
}
