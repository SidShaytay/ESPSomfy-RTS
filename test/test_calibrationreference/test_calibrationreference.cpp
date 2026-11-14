#include "ShadeBehaviour.h"
#include <unity.h>
#include <algorithm>
#include <type_traits>
#include <utility>
#include <vector>

// Detection keeps the old baseline runnable: its actual position/restore
// behavior fails the contract rather than stopping at a missing API.
template<class B, class = void> struct HasOutward : std::false_type {};
template<class B> struct HasOutward<B, std::void_t<decltype(std::declval<B &>().outwardPosition(0u))>> : std::true_type {};
template<class B> static float outward(B &b, uint32_t now) {
  if constexpr(HasOutward<B>::value) return b.outwardPosition(now);
  return b.state().currentPos;
}
template<class B, class = void> struct HasReferenceRestore : std::false_type {};
template<class B> struct HasReferenceRestore<B, std::void_t<decltype(std::declval<B &>().restore(0,0,0,0,true))>> : std::true_type {};
template<class B> static void restoreReference(B &b, float position, bool required) {
  if constexpr(HasReferenceRestore<B>::value) b.restore(position,0,70,-1,required);
  else b.restore(position,0,70,-1);
}
template<class B, class = void> struct HasConfirmation : std::false_type {};
template<class B> struct HasConfirmation<B, std::void_t<decltype(std::declval<B &>().confirmEndpoint(false))>> : std::true_type {};
template<class B> static void confirm(B &b, bool closed) {
  if constexpr(HasConfirmation<B>::value) b.confirmEndpoint(closed);
  else b.setPosition(closed ? 100 : 0,-1);
}
struct Sent { somfy_commands command; uint32_t ticket; };
struct ReferenceLink : MotorLink {
  somfy_frame_t &frame;
  uint32_t next = 40;
  std::vector<Sent> sent;
  explicit ReferenceLink(somfy_frame_t &f) : frame(f) {}
  send_result_t send(somfy_commands command, uint8_t repeats, uint8_t step,
                     uint16_t, bool) override {
    frame.cmd = command; frame.bitLength = 56; frame.remoteAddress = 123;
    frame.repeats = repeats; frame.stepSize = step;
    frame.processed = false; frame.valid = true;
    sent.push_back({command,++next});
    return {true,next};
  }
};
struct ReferenceEvents : ShadeEvents {
  void onChanged(uint16_t) override {}
  void onCommand(somfy_commands,const char *,uint32_t) override {}
  void onSaveWanted() override {}
  void onSensorFlags() override {}
};
struct Rig {
  ShadeSettings settings;
  RemoteSettings remote;
  somfy_frame_t frame;
  ReferenceLink link{frame};
  ReferenceEvents events;
  uint8_t id = 7;
  const char name[21] = "Reference shutter";
  ShadeBehaviour b{settings,remote,frame,link,events,id,name};
  uint32_t now = 1000;
  Rig() {
    remote.bitLength = 56;
    settings.upTime = 20000; settings.downTime = 10000;
    settings.calibration.kind = LiftKind::segmented;
    settings.calibration.down = {1000,3000,1000,200};
    settings.calibration.up = {2000,10000,2000,500};
    b.restore(0,0,70,-1);
  }
  void run(uint32_t milliseconds) {
    while(milliseconds) {
      const auto step = std::min(10u,milliseconds);
      now += step; milliseconds -= step; b.tick(now);
    }
  }
  void event(tx_event_t type, uint32_t ticket) {
    somfy_tx_event_t e; e.type = type; e.ticket = ticket; e.at = now;
    b.onTxEvent(e);
  }
  void press(uint32_t ticket) {
    event(tx_event_t::started,ticket); run(274); event(tx_event_t::finished,ticket);
  }
  uint32_t start(float target) {
    const auto at = now;
    TEST_ASSERT_TRUE(b.moveToTarget(target,-1,now));
    TEST_ASSERT_FALSE(link.sent.empty());
    press(link.sent.back().ticket);
    return at;
  }
  void until(uint32_t at) { TEST_ASSERT_TRUE(at >= now); run(at - now); }
  void stopAt(uint32_t elapsed) {
    const auto at = start(100); until(at + elapsed);
    const auto before = link.sent.size();
    TEST_ASSERT_TRUE(b.sendCommand(somfy_commands::My,1,0,now));
    TEST_ASSERT_EQUAL_UINT(before + 1,link.sent.size());
    TEST_ASSERT_EQUAL_UINT8(uint8_t(somfy_commands::My),uint8_t(link.sent.back().command));
    press(link.sent.back().ticket);
  }
  void refuseInterior() {
    const auto before = link.sent.size();
    TEST_ASSERT_FALSE(b.moveToTarget(50,-1,now));
    TEST_ASSERT_FALSE(b.moveToGroupTarget(50,now));
    TEST_ASSERT_EQUAL_UINT(before,link.sent.size());
  }
};
void setUp() {}
void tearDown() {}

static void stopping_during_takeup_requires_reference_before_any_interior_rf() {
  Rig r; r.stopAt(700);
  TEST_ASSERT_FLOAT_WITHIN(0.01f,0,r.b.state().currentPos);
  r.refuseInterior();
}
static void stopping_during_endpoint_settling_requires_reference_before_interior_rf() {
  Rig r; r.stopAt(9500);
  TEST_ASSERT_FLOAT_WITHIN(0.01f,100,r.b.state().currentPos);
  r.refuseInterior();
}
static void numeric_position_correction_cannot_impersonate_human_endpoint_confirmation() {
  Rig r; r.stopAt(700);
  r.b.setPosition(0,-1);
  r.refuseInterior();
  const auto before = r.link.sent.size();
  confirm(r.b,false);
  TEST_ASSERT_EQUAL_UINT(before,r.link.sent.size());
  TEST_ASSERT_FLOAT_WITHIN(0.01f,0,outward(r.b,r.now));
  TEST_ASSERT_TRUE(r.b.moveToTarget(50,-1,r.now));
  TEST_ASSERT_EQUAL_UINT(before + 1,r.link.sent.size());
  TEST_ASSERT_EQUAL_UINT8(uint8_t(somfy_commands::Down),uint8_t(r.link.sent.back().command));
}
static void endpoint_travel_remains_available_but_cannot_clear_an_uncertain_phase_alone() {
  for(float target : {0.0f,100.0f}) {
    Rig r; r.stopAt(700);
    const auto before = r.link.sent.size();
    TEST_ASSERT_TRUE(r.b.moveToTarget(target,-1,r.now));
    TEST_ASSERT_EQUAL_UINT(before + 1,r.link.sent.size());
    r.press(r.link.sent.back().ticket); r.run(25000);
    r.refuseInterior();
    confirm(r.b,target == 100);
    TEST_ASSERT_TRUE(r.b.moveToTarget(50,-1,r.now));
  }
}
static void restored_reference_barrier_survives_numeric_correction_until_explicit_anchor() {
  Rig r; restoreReference(r.b,100,true);
  r.refuseInterior();
  TEST_ASSERT_FLOAT_WITHIN(0.01f,99,outward(r.b,r.now));
  r.b.setPosition(0,-1); r.refuseInterior();
  TEST_ASSERT_FLOAT_WITHIN(0.01f,1,outward(r.b,r.now));
  confirm(r.b,false);
  TEST_ASSERT_TRUE(r.b.moveToTarget(50,-1,r.now));
  Rig known; restoreReference(known.b,100,false);
  TEST_ASSERT_TRUE(known.b.moveToTarget(50,-1,known.now));
}
static void outward_endpoints_wait_for_completion_while_internal_visible_position_and_target_remain_exact() {
  for(bool up : {false,true}) {
    Rig r;
    if(up) r.b.restore(100,0,70,-1);
    const float origin = up ? 100 : 0, target = up ? 0 : 100;
    const auto at = r.start(target);
    r.until(at + (up ? 1000 : 700));
    TEST_ASSERT_FLOAT_WITHIN(0.01f,origin,r.b.state().currentPos);
    TEST_ASSERT_FLOAT_WITHIN(0.01f,up ? 99 : 1,outward(r.b,r.now));
    TEST_ASSERT_FLOAT_WITHIN(0.01f,target,r.b.state().target);
    r.until(at + (up ? 7000 : 2500));
    TEST_ASSERT_FLOAT_WITHIN(0.05f,up ? 75 : 25,r.b.state().currentPos);
    TEST_ASSERT_FLOAT_WITHIN(0.05f,r.b.state().currentPos,outward(r.b,r.now));
    r.until(at + (up ? 19000 : 9500));
    TEST_ASSERT_FLOAT_WITHIN(0.01f,target,r.b.state().currentPos);
    TEST_ASSERT_FLOAT_WITHIN(0.01f,up ? 1 : 99,outward(r.b,r.now));
    r.until(at + (up ? 20000 : 10000));
    TEST_ASSERT_FLOAT_WITHIN(0.01f,target,outward(r.b,r.now));
  }
}
static void dropped_or_partial_start_and_partial_stop_require_reference_without_another_my() {
  for(unsigned fault = 0; fault < 3; ++fault) {
    Rig r;
    TEST_ASSERT_TRUE(r.b.moveToTarget(100,-1,r.now));
    const auto start = r.link.sent.back().ticket;
    if(fault == 0) r.event(tx_event_t::dropped,start);
    else if(fault == 1) {
      r.event(tx_event_t::started,start); r.run(100); r.event(tx_event_t::aborted,start);
    } else {
      r.press(start); r.run(426); // 700ms after actual start, still taking up.
      TEST_ASSERT_TRUE(r.b.sendCommand(somfy_commands::My,1,0,r.now));
      const auto stop = r.link.sent.back().ticket;
      r.event(tx_event_t::started,stop); r.run(100); r.event(tx_event_t::aborted,stop);
    }
    const auto before = r.link.sent.size();
    r.run(25000);
    TEST_ASSERT_EQUAL_UINT(before,r.link.sent.size());
    r.refuseInterior();
  }
}
int main() {
  UNITY_BEGIN();
  RUN_TEST(stopping_during_takeup_requires_reference_before_any_interior_rf);
  RUN_TEST(stopping_during_endpoint_settling_requires_reference_before_interior_rf);
  RUN_TEST(numeric_position_correction_cannot_impersonate_human_endpoint_confirmation);
  RUN_TEST(endpoint_travel_remains_available_but_cannot_clear_an_uncertain_phase_alone);
  RUN_TEST(restored_reference_barrier_survives_numeric_correction_until_explicit_anchor);
  RUN_TEST(outward_endpoints_wait_for_completion_while_internal_visible_position_and_target_remain_exact);
  RUN_TEST(dropped_or_partial_start_and_partial_stop_require_reference_without_another_my);
  return UNITY_END();
}
