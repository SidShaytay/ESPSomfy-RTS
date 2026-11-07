#include <unity.h>
#include <vector>
#include <Preferences.h>
#include "Somfy.h"

// Run the production adapter, model wrappers, remote rolling-code submission,
// and controller TX fan-out. Host shims replace startup, the direct model-id
// lookup, radio/persistence/GPIO and network publication boundaries. The
// native source filter excludes these units, so compile them here for this
// suite. No RF propagation, motor reception or physical travel is simulated.
#define TAG GroupTag
#include "../../src/SomfyGroup.cpp"
#undef TAG
#define TAG RemoteTag
#include "../../src/SomfyRemote.cpp"
#undef TAG
#define TAG ShadeTag
#include "../../src/SomfyShade.cpp"
#undef TAG
#include "../../src/ShadeControllerTx.cpp"

SomfyShadeController somfy;
uint8_t radioBitLength = 56;
static RadioScheduler radio;
static std::vector<somfy_tx_job_t> submitted;

SomfyShadeController::SomfyShadeController() {}
void SomfyShadeController::commit() {}
SomfyShade *SomfyShadeController::getShadeById(uint8_t id) {
  for(auto &shade : shades) if(shade.getShadeId() == id) return &shade;
  return nullptr;
}
void SomfyShadeController::updateGroupFlags() {
  for(auto &group : groups) if(group.getGroupId() != 255) group.updateFlags();
}
uint32_t Transceiver::submitTx(somfy_tx_job_t &job, bool priority, uint16_t delay) {
  auto ticket = radio.submit(job,priority,delay,millis());
  if(ticket) submitted.push_back(job);
  return ticket;
}
void Transceiver::cancelTx(uint32_t address, uint32_t except) {
  radio.cancelRemote(address,millis(),except);
}
void SomfyRemote::triggerGPIOs(somfy_frame_t &) { TEST_FAIL_MESSAGE("RF group reached GPIO"); }
void SomfyShade::triggerGPIOs(somfy_frame_t &) { TEST_FAIL_MESSAGE("RF shade reached GPIO"); }
void SomfyShade::publish() {}
void SomfyShade::emitState() { changed |= SHADE_CHANGED_STATE; }
void SomfyGroup::emitState() { changed |= GROUP_CHANGED_STATE; }
void SomfyShade::emitCommand(somfy_commands,const char *,uint32_t) {}

static SomfyGroup &group() { return somfy.groups[0]; }
static SomfyShade &member(size_t i) { return somfy.shades[i]; }
static void reset() {
  radio = RadioScheduler{};
  submitted.clear();
  Preferences::resetTestState();
  stubMillis=1000;
  for(auto &shade : somfy.shades) shade.clear();
  for(auto &g : somfy.groups) g.clear();
  // Sanitized semantic values from the authorized saved configuration; no
  // device addresses, counters, names or credentials are copied into Git.
  for(size_t i=0;i<2;i++) {
    auto &shade=member(i);
    shade.setShadeId(i+1); shade.setRemoteAddress(0x110001+i);
    shade.shadeType=shade_types::shutter; shade.tiltType=tilt_types::none;
    shade.proto=radio_proto::RTS; shade.bitLength=56; shade.repeats=1;
    shade.upTime=i ? 21150 : 23750; shade.downTime=i ? 19300 : 22100;
    shade.setFlags(0); shade.restore(0,0,-1,-1);
  }
  auto &g=group(); g.setGroupId(1); g.setRemoteAddress(0x220001);
  g.proto=radio_proto::RTS; g.bitLength=56; g.repeats=1; g.flipCommands=false;
  g.flags=0; g.direction=0; g.changed=0; g.lastRollingCode=0; g.lastFrame=somfy_frame_t{};
  g.linkShade(1); g.linkShade(2);
}
static void events() {
  somfy_tx_event_t event;
  while(radio.popEvent(event)) somfy.processTxEvent(event);
}
static somfy_tx_job_t next() {
  somfy_tx_job_t job; uint32_t wait;
  TEST_ASSERT_TRUE(radio.next(millis(),job,wait));
  return job;
}
static void completeJob(const somfy_tx_job_t &job,uint32_t duration=180) {
  TEST_ASSERT_TRUE(radio.start(job.ticket,millis())); events();
  stubMillis+=duration;
  radio.ended(job.ticket,true,millis()); events();
}
static void tick(uint32_t duration) {
  for(uint32_t t=0;t<duration;t+=10) {
    stubMillis+=10;
    member(0).checkMovement(); member(1).checkMovement(); group().updateTargetDirection();
  }
}
static void saved_configuration_uses_one_native_group_start() {
  reset();
  TEST_ASSERT_TRUE(group().moveToTarget(50));
  TEST_ASSERT_EQUAL_UINT(1,submitted.size());
  TEST_ASSERT_EQUAL_UINT32(group().getRemoteAddress(),submitted[0].frame.remoteAddress);
  TEST_ASSERT_EQUAL(somfy_commands::Down,submitted[0].frame.cmd);
  auto job=next(); TEST_ASSERT_EQUAL_UINT32(submitted[0].ticket,job.ticket);
  tick(3000); TEST_ASSERT_EQUAL_FLOAT(0,member(0).state().currentPos);
  TEST_ASSERT_EQUAL_FLOAT(0,member(1).state().currentPos);
  completeJob(job);
  tick(9300);
  TEST_ASSERT_EQUAL_UINT(1,submitted.size());
  tick(180);
  TEST_ASSERT_EQUAL_UINT(2,submitted.size());
  TEST_ASSERT_EQUAL_UINT32(member(1).getRemoteAddress(),submitted[1].frame.remoteAddress);
}
static somfy_frame_t decoded(const somfy_tx_job_t &job) {
  byte bytes[10]; memcpy(bytes,job.tx.payload,sizeof(bytes));
  somfy_frame_t frame; frame.bitLength=job.tx.bit_length; frame.decodeFrame(bytes);
  TEST_ASSERT_TRUE(frame.valid); return frame;
}
static void production_frames_encode_group_identity_and_independent_stop_identities() {
  reset(); TEST_ASSERT_TRUE(group().moveToTarget(50));
  auto shared=next(); auto frame=decoded(shared);
  TEST_ASSERT_EQUAL_UINT32(group().getRemoteAddress(),frame.remoteAddress);
  TEST_ASSERT_EQUAL(somfy_commands::Down,frame.cmd);
  TEST_ASSERT_EQUAL_UINT16(1,frame.rollingCode);
  TEST_ASSERT_EQUAL_UINT(1,shared.repeats);
  completeJob(shared); tick(9500);
  auto fastStop=next(); frame=decoded(fastStop);
  TEST_ASSERT_EQUAL_UINT32(member(1).getRemoteAddress(),frame.remoteAddress);
  TEST_ASSERT_EQUAL(somfy_commands::My,frame.cmd);
  TEST_ASSERT_EQUAL_UINT16(1,frame.rollingCode);
  completeJob(fastStop); tick(1400);
  auto slowStop=next(); frame=decoded(slowStop);
  TEST_ASSERT_EQUAL_UINT32(member(0).getRemoteAddress(),frame.remoteAddress);
  TEST_ASSERT_EQUAL(somfy_commands::My,frame.cmd);
  TEST_ASSERT_EQUAL_UINT16(1,frame.rollingCode);
  completeJob(slowStop); tick(10);
  TEST_ASSERT_TRUE(member(0).isIdle()); TEST_ASSERT_TRUE(member(1).isIdle());
  TEST_ASSERT_EQUAL_UINT(3,submitted.size()); TEST_ASSERT_EQUAL_INT8(0,group().direction);
}
static void repeated_estimated_endpoint_still_encodes_two_native_presses() {
  reset(); TEST_ASSERT_TRUE(group().moveToTarget(0)); completeJob(next()); tick(10);
  TEST_ASSERT_TRUE(group().moveToTarget(0));
  TEST_ASSERT_EQUAL_UINT(2,submitted.size());
  auto first=decoded(submitted[0]),second=decoded(submitted[1]);
  TEST_ASSERT_EQUAL_UINT32(group().getRemoteAddress(),first.remoteAddress);
  TEST_ASSERT_EQUAL_UINT32(group().getRemoteAddress(),second.remoteAddress);
  TEST_ASSERT_EQUAL(somfy_commands::Up,first.cmd); TEST_ASSERT_EQUAL(somfy_commands::Up,second.cmd);
  TEST_ASSERT_EQUAL_UINT16(first.rollingCode+1,second.rollingCode);
}
static void mixed_required_directions_preserve_individual_fallback_and_live_tickets() {
  reset(); member(0).restore(20,0,-1,-1); member(1).restore(80,0,-1,-1);
  TEST_ASSERT_TRUE(group().moveToTarget(50)); TEST_ASSERT_EQUAL_UINT(2,submitted.size());
  auto first=next(); TEST_ASSERT_EQUAL_UINT32(member(0).getRemoteAddress(),first.frame.remoteAddress);
  TEST_ASSERT_EQUAL(somfy_commands::Down,decoded(first).cmd); completeJob(first);
  auto second=next(); TEST_ASSERT_EQUAL_UINT32(member(1).getRemoteAddress(),second.frame.remoteAddress);
  TEST_ASSERT_EQUAL(somfy_commands::Up,decoded(second).cmd); completeJob(second);
  tick(1000); TEST_ASSERT_TRUE(member(0).state().currentPos>20); TEST_ASSERT_TRUE(member(1).state().currentPos<80);
}
static void ordinary_native_group_and_target_native_group_encode_same_command_contract() {
  reset(); TEST_ASSERT_TRUE(group().sendCommand(somfy_commands::Down)); auto ordinary=submitted.back();
  reset(); TEST_ASSERT_TRUE(group().moveToTarget(100)); auto target=submitted.back();
  TEST_ASSERT_EQUAL_UINT(ordinary.repeats,target.repeats);
  TEST_ASSERT_EQUAL_UINT(ordinary.tx.hwsync,target.tx.hwsync);
  TEST_ASSERT_EQUAL_UINT(ordinary.tx.bit_length,target.tx.bit_length);
  TEST_ASSERT_EQUAL_UINT8_ARRAY(ordinary.tx.payload,target.tx.payload,10);
}
static void slider_burst_keeps_one_shared_start_and_latest_target() {
  reset(); TEST_ASSERT_TRUE(group().moveToTarget(1));
  const auto ticket=submitted.back().ticket;
  TEST_ASSERT_TRUE(group().moveToTarget(20)); TEST_ASSERT_TRUE(group().moveToTarget(80));
  TEST_ASSERT_FALSE(group().sendCommand(somfy_commands::My));
  TEST_ASSERT_FALSE(member(0).moveToTarget(20)); TEST_ASSERT_EQUAL_UINT(1,submitted.size());
  auto shared=next(); TEST_ASSERT_EQUAL_UINT32(ticket,shared.ticket); completeJob(shared);
  TEST_ASSERT_EQUAL_FLOAT(80,member(0).state().target); TEST_ASSERT_EQUAL_FLOAT(80,member(1).state().target);
  TEST_ASSERT_EQUAL_UINT(1,submitted.size());
  tick(1000); TEST_ASSERT_EQUAL_UINT(1,submitted.size());
}
static void reverse_slider_burst_defers_stop_until_shared_start_completes() {
  reset(); TEST_ASSERT_TRUE(group().moveToTarget(80));
  TEST_ASSERT_TRUE(group().moveToTarget(0)); TEST_ASSERT_EQUAL_UINT(1,submitted.size());
  auto shared=next(); TEST_ASSERT_TRUE(radio.start(shared.ticket,millis())); events(); tick(100);
  TEST_ASSERT_EQUAL_UINT(1,submitted.size());
  radio.ended(shared.ticket,true,millis()); events();
  TEST_ASSERT_EQUAL_UINT(3,submitted.size());
  TEST_ASSERT_EQUAL(somfy_commands::My,decoded(submitted[1]).cmd);
  TEST_ASSERT_EQUAL(somfy_commands::My,decoded(submitted[2]).cmd);
}
static void ordinary_group_pending_start_still_refuses_target_replacement() {
  reset(); TEST_ASSERT_TRUE(group().sendCommand(somfy_commands::Down));
  TEST_ASSERT_FALSE(group().moveToTarget(50)); TEST_ASSERT_EQUAL_UINT(1,submitted.size());
  auto shared=next(); TEST_ASSERT_EQUAL_UINT32(group().getRemoteAddress(),shared.frame.remoteAddress);
  completeJob(shared);
}
static void compatible_active_retarget_reuses_shared_movement_without_extra_rf() {
  reset(); TEST_ASSERT_TRUE(group().moveToTarget(80)); completeJob(next()); tick(3000);
  TEST_ASSERT_TRUE(group().moveToTarget(50)); TEST_ASSERT_EQUAL_UINT(1,submitted.size());
  TEST_ASSERT_EQUAL_FLOAT(50,member(0).state().target); TEST_ASSERT_EQUAL_FLOAT(50,member(1).state().target);
  TEST_ASSERT_EQUAL_UINT16(0,member(0).lastRollingCode); TEST_ASSERT_EQUAL_UINT16(0,member(1).lastRollingCode);
  tick(1000); TEST_ASSERT_EQUAL_UINT(1,submitted.size());
}
static void dropped_shared_start_discards_coalesced_latest_target_without_rf() {
  reset(); TEST_ASSERT_TRUE(group().moveToTarget(1)); TEST_ASSERT_TRUE(group().moveToTarget(80));
  radio.flush(millis()); events(); tick(30000);
  TEST_ASSERT_EQUAL_FLOAT(0,member(0).state().currentPos); TEST_ASSERT_EQUAL_FLOAT(0,member(1).state().currentPos);
  TEST_ASSERT_EQUAL_FLOAT(0,member(0).state().target); TEST_ASSERT_EQUAL_FLOAT(0,member(1).state().target);
  TEST_ASSERT_EQUAL_UINT(1,submitted.size());
}
static void flipped_group_and_members_preserve_shared_wire_contract_for_burst() {
  reset(); group().flipCommands=true; member(0).flipCommands=true; member(1).flipCommands=true;
  TEST_ASSERT_TRUE(group().moveToTarget(1)); TEST_ASSERT_TRUE(group().moveToTarget(80));
  TEST_ASSERT_EQUAL_UINT(1,submitted.size()); TEST_ASSERT_EQUAL(somfy_commands::Up,decoded(submitted[0]).cmd);
  completeJob(next()); TEST_ASSERT_EQUAL_FLOAT(80,member(0).state().target); TEST_ASSERT_EQUAL_FLOAT(80,member(1).state().target);
}
int main() {
  UNITY_BEGIN();
  RUN_TEST(saved_configuration_uses_one_native_group_start);
  RUN_TEST(dropped_shared_start_discards_coalesced_latest_target_without_rf);
  RUN_TEST(flipped_group_and_members_preserve_shared_wire_contract_for_burst);
  RUN_TEST(compatible_active_retarget_reuses_shared_movement_without_extra_rf);
  RUN_TEST(production_frames_encode_group_identity_and_independent_stop_identities);
  RUN_TEST(repeated_estimated_endpoint_still_encodes_two_native_presses);
  RUN_TEST(mixed_required_directions_preserve_individual_fallback_and_live_tickets);
  RUN_TEST(ordinary_native_group_and_target_native_group_encode_same_command_contract);
  RUN_TEST(slider_burst_keeps_one_shared_start_and_latest_target);
  RUN_TEST(reverse_slider_burst_defers_stop_until_shared_start_completes);
  RUN_TEST(ordinary_group_pending_start_still_refuses_target_replacement);
  return UNITY_END();
}
