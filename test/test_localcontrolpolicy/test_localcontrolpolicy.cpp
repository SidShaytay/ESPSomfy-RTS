#include <unity.h>
#include "LocalControlPolicy.h"
#include <limits.h>
using namespace LocalUi;
void setUp() {}
void tearDown() {}
static ControlDescriptor motor() {
  ControlDescriptor d; d.paired=true; return d;
}
static void all_types_protocols_bits_and_tilt_modes_have_expected_capabilities() {
  const bool directional[17]={1,1,1,1,1,0,1,1,1,0,1,1,1,1,0,0,0};
  const bool ordinary[17]={1,1,1,1,1,0,1,1,1,0,0,1,1,1,0,0,0};
  const bool toggle[17]={0,0,0,0,0,1,0,0,0,1,0,0,0,0,1,1,1};
  const bool sun[17]={1,1,1,1,1,0,0,1,1,0,0,1,1,1,0,0,0};
  const radio_proto protocols[]={radio_proto::RTS,radio_proto::RTW,radio_proto::RTV,radio_proto::GP_Relay,radio_proto::GP_Remote};
  for(uint8_t type=0;type<17;type++) for(auto proto:protocols)
    for(uint8_t tilt=0;tilt<5;tilt++) for(uint8_t bits:{uint8_t(56),uint8_t(80)}) {
      auto d=motor(); d.shadeType=shade_types(type); d.proto=proto;
      d.tiltType=tilt_types(tilt); d.bitLength=bits; d.sunSensor=true;
      const bool rf=proto==radio_proto::RTS || proto==radio_proto::RTW || proto==radio_proto::RTV;
      TEST_ASSERT_EQUAL(directional[type],allows(ControlAction::up,d));
      TEST_ASSERT_EQUAL(directional[type],allows(ControlAction::down,d));
      TEST_ASSERT_EQUAL(ordinary[type],allows(ControlAction::stop,d));
      TEST_ASSERT_EQUAL(ordinary[type],allows(ControlAction::my,d));
      TEST_ASSERT_EQUAL(toggle[type],allows(ControlAction::toggle,d));
      TEST_ASSERT_EQUAL(ordinary[type] && tilt!=3,allows(ControlAction::position,d));
      for(auto a:{ControlAction::tiltUp,ControlAction::tiltStop,ControlAction::tiltDown,ControlAction::tiltPosition})
        TEST_ASSERT_EQUAL(ordinary[type] && tilt!=0,allows(a,d));
      TEST_ASSERT_EQUAL(rf && bits==80 && ordinary[type],allows(ControlAction::favourite,d));
      TEST_ASSERT_EQUAL(rf && bits==80 && ordinary[type],allows(ControlAction::stepUp,d));
      TEST_ASSERT_EQUAL(rf && bits==80 && ordinary[type],allows(ControlAction::stepDown,d));
      TEST_ASSERT_EQUAL(sun[type],allows(ControlAction::sunOn,d));
      TEST_ASSERT_EQUAL(sun[type],allows(ControlAction::sunOff,d));
      d.sunSensor=false; TEST_ASSERT_FALSE(allows(ControlAction::sunOn,d));
      d.paired=false;
      TEST_ASSERT_EQUAL(!rf,allows(directional[type]?ControlAction::up:ControlAction::toggle,d));
    }
}
static void malformed_descriptors_and_actions_fail_closed() {
  auto d=motor();
  d.proto=radio_proto(3); TEST_ASSERT_EQUAL(0,controlCapabilities(d));
  d=motor(); d.shadeType=shade_types(17); TEST_ASSERT_EQUAL(0,controlCapabilities(d));
  d=motor(); d.tiltType=tilt_types(5); TEST_ASSERT_EQUAL(0,controlCapabilities(d));
  for(uint8_t bits:{uint8_t(0),uint8_t(55),uint8_t(57),uint8_t(81),uint8_t(255)}) {
    d=motor();d.bitLength=bits; TEST_ASSERT_EQUAL(0,controlCapabilities(d));
  }
  d=motor();d.proto=radio_proto::GP_Relay;d.bitLength=0;d.paired=false;
  TEST_ASSERT_TRUE(allows(ControlAction::up,d));
  shade_request_t invalid;invalid.id=73;
  TEST_ASSERT_EQUAL(int(ControlResult::unsupported),int(prepareControlRequest(ControlAction(255),d,5,-1,0,invalid)));
  TEST_ASSERT_EQUAL(73,invalid.id);
  TEST_ASSERT_FALSE(allows(ControlAction::count,d));
  TEST_ASSERT_FALSE(allows(ControlAction(255),d));
  TEST_ASSERT_EQUAL(0,controlActionBit(ControlAction(255)));
}
static void bounds_never_turn_invalid_targets_into_my_or_change_output() {
  auto d=motor(); d.tiltType=tilt_types::integrated;
  for(auto action:{ControlAction::position,ControlAction::tiltPosition}) {
    for(int32_t target:{INT32_MIN,-1,101,128,256,INT32_MAX}) {
      shade_request_t out;out.id=73;out.command=somfy_commands::Down;out.target=42;
      TEST_ASSERT_EQUAL(int(ControlResult::invalid),int(prepareControlRequest(action,d,8,target,0,out)));
      TEST_ASSERT_EQUAL(73,out.id);TEST_ASSERT_EQUAL(42,out.target);
      TEST_ASSERT_EQUAL(int(somfy_commands::Down),int(out.command));
    }
    for(int32_t target:{0,1,50,99,100}) {
      shade_request_t out;
      TEST_ASSERT_EQUAL(int(ControlResult::done),int(prepareControlRequest(action,d,254,target,0,out)));
      TEST_ASSERT_EQUAL(254,out.id);TEST_ASSERT_EQUAL(target,out.target);
      TEST_ASSERT_EQUAL(int(action==ControlAction::position?shade_request_type_t::shade_command:shade_request_type_t::tilt_command),int(out.type));
      TEST_ASSERT_EQUAL(-1,out.repeat);
    }
  }
  for(int32_t id:{INT32_MIN,-1,0,255,256,INT32_MAX}) {
    shade_request_t out;out.id=73;
    TEST_ASSERT_EQUAL(int(ControlResult::invalid),int(prepareControlRequest(ControlAction::up,d,id,-1,0,out)));
    TEST_ASSERT_EQUAL(73,out.id);
  }
  for(int32_t id:{1,32,128,254}) {
    shade_request_t out;
    TEST_ASSERT_EQUAL(int(ControlResult::done),int(prepareControlRequest(ControlAction::up,d,id,-1,0,out)));
    TEST_ASSERT_EQUAL(id,out.id);
  }
}
static void stop_is_idle_guarded_my_and_tilt_stop_requires_tilt_moving() {
  auto d=motor();d.tiltType=tilt_types::tiltmotor;
  shade_request_t out;out.id=73;
  TEST_ASSERT_EQUAL(int(ControlResult::idle),int(prepareControlRequest(ControlAction::stop,d,4,-1,0,out)));
  TEST_ASSERT_EQUAL(73,out.id);
  d.moving=true;
  TEST_ASSERT_EQUAL(int(ControlResult::done),int(prepareControlRequest(ControlAction::stop,d,4,-1,0,out)));
  TEST_ASSERT_EQUAL(int(somfy_commands::My),int(out.command));
  TEST_ASSERT_EQUAL(int(ControlResult::idle),int(prepareControlRequest(ControlAction::tiltStop,d,4,-1,0,out)));
  d.tiltMoving=true;
  TEST_ASSERT_EQUAL(int(ControlResult::done),int(prepareControlRequest(ControlAction::tiltStop,d,4,-1,0,out)));
  TEST_ASSERT_EQUAL(int(shade_request_type_t::tilt_command),int(out.type));
  TEST_ASSERT_EQUAL(int(somfy_commands::My),int(out.command));
  TEST_ASSERT_EQUAL(int(ControlResult::done),int(prepareControlRequest(ControlAction::my,d,4,-1,0,out)));
}
static void direct_actions_use_existing_typed_request_opcodes() {
  auto d=motor();d.bitLength=80;d.tiltType=tilt_types::integrated;d.sunSensor=true;
  const ControlAction actions[]={ControlAction::up,ControlAction::down,ControlAction::my,ControlAction::favourite,
    ControlAction::tiltUp,ControlAction::tiltDown,ControlAction::sunOn,ControlAction::sunOff};
  const somfy_commands commands[]={somfy_commands::Up,somfy_commands::Down,somfy_commands::My,somfy_commands::Favorite,
    somfy_commands::Up,somfy_commands::Down,somfy_commands::SunFlag,somfy_commands::Flag};
  for(unsigned i=0;i<sizeof(actions)/sizeof(actions[0]);i++) {
    shade_request_t out;
    TEST_ASSERT_EQUAL(int(ControlResult::done),int(prepareControlRequest(actions[i],d,9,-1,0,out)));
    TEST_ASSERT_EQUAL(int(commands[i]),int(out.command));TEST_ASSERT_EQUAL(-1,out.target);
    TEST_ASSERT_EQUAL(int(i==4 || i==5?shade_request_type_t::tilt_command:shade_request_type_t::shade_command),int(out.type));
  }
  d.shadeType=shade_types::drycontact;d.proto=radio_proto::GP_Remote;d.paired=false;d.bitLength=0;
  shade_request_t out;
  TEST_ASSERT_EQUAL(int(ControlResult::done),int(prepareControlRequest(ControlAction::toggle,d,9,-1,0,out)));
  TEST_ASSERT_EQUAL(int(somfy_commands::Toggle),int(out.command));
}
static void steps_require_one_to_127_and_unused_arguments_are_rejected() {
  auto d=motor();d.bitLength=80;
  for(auto action:{ControlAction::stepUp,ControlAction::stepDown}) {
    for(int32_t step:{INT32_MIN,-1,0,128,256,INT32_MAX}) {
      shade_request_t out;out.id=73;
      TEST_ASSERT_EQUAL(int(ControlResult::invalid),int(prepareControlRequest(action,d,5,-1,step,out)));
      TEST_ASSERT_EQUAL(73,out.id);
    }
    for(int32_t step:{1,3,127}) {
      shade_request_t out;
      TEST_ASSERT_EQUAL(int(ControlResult::done),int(prepareControlRequest(action,d,5,-1,step,out)));
      TEST_ASSERT_EQUAL(step,out.stepSize);
      TEST_ASSERT_EQUAL(int(action==ControlAction::stepUp?somfy_commands::StepUp:somfy_commands::StepDown),int(out.command));
    }
  }
  shade_request_t out;out.id=73;
  TEST_ASSERT_EQUAL(int(ControlResult::invalid),int(prepareControlRequest(ControlAction::up,d,5,0,0,out)));
  TEST_ASSERT_EQUAL(int(ControlResult::invalid),int(prepareControlRequest(ControlAction::position,d,5,50,1,out)));
  TEST_ASSERT_EQUAL(int(ControlResult::invalid),int(prepareControlRequest(ControlAction::up,d,5,-1,1,out)));
  TEST_ASSERT_EQUAL(73,out.id);
}
static void groups_intersect_members_but_use_any_sensor_and_no_position_or_tilt() {
  auto first=motor(), second=motor();first.bitLength=second.bitLength=80;
  first.sunSensor=true;
  ControlDescriptor group;group.group=true;group.memberCount=2;group.bitLength=80;group.moving=true;
  group.commonActions=controlCapabilities(first)&controlCapabilities(second);group.sunSensor=true;
  for(auto a:{ControlAction::up,ControlAction::stop,ControlAction::down,ControlAction::my,
              ControlAction::stepUp,ControlAction::stepDown,ControlAction::sunOn,ControlAction::sunOff})
    TEST_ASSERT_TRUE(allows(a,group));
  for(auto a:{ControlAction::position,ControlAction::tiltPosition,ControlAction::tiltUp,
              ControlAction::tiltDown,ControlAction::tiltStop,ControlAction::favourite,ControlAction::toggle})
    TEST_ASSERT_FALSE(allows(a,group));
  shade_request_t out;
  TEST_ASSERT_EQUAL(int(ControlResult::done),int(prepareControlRequest(ControlAction::stepDown,group,16,-1,7,out)));
  TEST_ASSERT_EQUAL(int(shade_request_type_t::group_command),int(out.type));TEST_ASSERT_EQUAL(7,out.stepSize);
  group.sunSensor=false;TEST_ASSERT_FALSE(allows(ControlAction::sunOn,group));
  second.shadeType=shade_types::drycontact;
  group.commonActions=controlCapabilities(first)&controlCapabilities(second);
  TEST_ASSERT_FALSE(allows(ControlAction::up,group));
  group.commonActions=0;group.sunSensor=true;
  TEST_ASSERT_TRUE(allows(ControlAction::sunOn,group)); // any sensor, even no common movement action
  TEST_ASSERT_FALSE(allows(ControlAction::up,group));
  group.memberCount=0;TEST_ASSERT_EQUAL(0,controlCapabilities(group));
  group.memberCount=33;TEST_ASSERT_EQUAL(0,controlCapabilities(group));
  group.memberCount=2;
  group.commonActions=allControlActions;group.proto=radio_proto::GP_Remote;
  TEST_ASSERT_EQUAL(0,controlCapabilities(group));
}
int main() {
  UNITY_BEGIN();
  RUN_TEST(all_types_protocols_bits_and_tilt_modes_have_expected_capabilities);
  RUN_TEST(malformed_descriptors_and_actions_fail_closed);
  RUN_TEST(bounds_never_turn_invalid_targets_into_my_or_change_output);
  RUN_TEST(stop_is_idle_guarded_my_and_tilt_stop_requires_tilt_moving);
  RUN_TEST(direct_actions_use_existing_typed_request_opcodes);
  RUN_TEST(steps_require_one_to_127_and_unused_arguments_are_rejected);
  RUN_TEST(groups_intersect_members_but_use_any_sensor_and_no_position_or_tilt);
  return UNITY_END();
}
