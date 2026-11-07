#include <unity.h>
#include <vector>
#include "GroupMotion.h"
#include "ShadeBehaviour.h"
#include "RadioScheduler.h"
#include "TxBuffer.h"

// Offline timing model of the actual 56-bit Transceiver::sendFrame loops:
// SYMBOL=640us, initial wake/sync/payload/silence, then repeat sync/payload.
// This is transmitter timing, not evidence a physical motor received a frame.
static uint32_t airtimeMs(uint8_t repeats) {
  const uint32_t firstUs = 10920 + 7357 + 2*8*640 + 4850 + 640 + 56*2*640 + 2*13717;
  const uint32_t repeatUs = 7*8*640 + 4850 + 640 + 56*2*640 + 2*13717;
  return (firstUs + repeats*repeatUs + 999)/1000;
}
struct Events : ShadeEvents {
  void onChanged(uint16_t) override {}
  void onCommand(somfy_commands, const char *, uint32_t) override {}
  void onSaveWanted() override {}
  void onSensorFlags() override {}
};
struct Sent { uint32_t at, ticket; };
struct Link : MotorLink {
  RadioScheduler &radio;
  uint32_t &now;
  somfy_frame_t &frame;
  std::vector<Sent> sent;
  uint32_t address;
  Link(RadioScheduler &r,uint32_t &n,somfy_frame_t &f,uint32_t a):radio(r),now(n),frame(f),address(a) {}
  send_result_t send(somfy_commands cmd,uint8_t repeat,uint8_t step,uint16_t delay,bool priority) override {
    somfy_frame_t f;
    f.remoteAddress=address; f.cmd=cmd; f.bitLength=56; f.repeats=repeat; f.stepSize=step; f.valid=true;
    somfy_tx_job_t job;
    encodeTxJob(job,f,repeat);
    send_result_t result;
    result.ticket=radio.submit(job,priority,delay,now); result.sent=result.ticket!=0;
    if(result.sent) { frame=f; sent.push_back({now,result.ticket}); }
    return result;
  }
};
struct Member {
  ShadeSettings settings;
  RemoteSettings remote;
  somfy_frame_t frame;
  Events events;
  uint8_t id;
  Link link;
  ShadeBehaviour motion;
  Member(RadioScheduler &r,uint32_t &now,uint8_t id):id(id),link(r,now,frame,id),motion(settings,remote,frame,link,events,this->id,"member") {
    remote.bitLength=56; remote.repeats=1; settings.upTime=settings.downTime=10000;
  }
};
struct Rig {
  RadioScheduler radio;
  uint32_t now=1000;
  Member a{radio,now,1}, b{radio,now,2};
  void events() { somfy_tx_event_t e; while(radio.popEvent(e)) { a.motion.onTxEvent(e); b.motion.onTxEvent(e); } }
  void tick(uint32_t duration) { for(uint32_t t=0;t<duration;t++) { ++now; events(); a.motion.tick(now); b.motion.tick(now); } }
  void start(float target,uint8_t repeats=1) {
    somfy_frame_t frame; frame.remoteAddress=100; frame.cmd=target==0 ? somfy_commands::Up : somfy_commands::Down; frame.bitLength=56;
    somfy_tx_job_t job; encodeTxJob(job,frame,repeats);
    const uint32_t ticket=radio.submit(job,false,0,now);
    TEST_ASSERT_NOT_EQUAL(0,ticket);
    a.motion.adoptGroupTarget(target,ticket,now); b.motion.adoptGroupTarget(target,ticket,now);
  }
  uint32_t transmitNext(bool addGap=true) {
    somfy_tx_job_t job; uint32_t wait;
    TEST_ASSERT_TRUE(radio.next(now,job,wait)); TEST_ASSERT_TRUE(radio.start(job.ticket,now)); events();
    tick(airtimeMs(job.repeats)); radio.ended(job.ticket,true,now); events();
    if(addGap) tick(100); // Transceiver::runJob's TX_QUEUE_DELAY
    return job.frame.remoteAddress;
  }
};
static void long_start_does_not_rewind_a_short_requested_target() {
  Rig r; r.start(1);
  TEST_ASSERT_EQUAL(100,r.transmitNext());
  TEST_ASSERT_EQUAL_FLOAT(3.74f,r.a.motion.state().currentPos);
  TEST_ASSERT_EQUAL_FLOAT(3.74f,r.b.motion.state().currentPos);
  TEST_ASSERT_EQUAL(1,r.a.link.sent.size()); TEST_ASSERT_EQUAL(1,r.b.link.sent.size());
  TEST_ASSERT_EQUAL(1,r.transmitNext());
  TEST_ASSERT_EQUAL_FLOAT(3.74f,r.a.motion.state().currentPos);
  TEST_ASSERT_EQUAL(2,r.transmitNext());
  TEST_ASSERT_EQUAL_FLOAT(7.48f,r.b.motion.state().currentPos);
}
static void separated_deadlines_reach_each_estimated_target_with_real_airtime() {
  Rig r; r.b.settings.downTime=20000; r.start(50);
  r.transmitNext(); r.tick(4626);
  TEST_ASSERT_EQUAL_FLOAT(50,r.a.motion.state().currentPos);
  TEST_ASSERT_EQUAL_FLOAT(25,r.b.motion.state().currentPos);
  TEST_ASSERT_EQUAL(1,r.transmitNext());
  r.tick(4626); // 10000ms since group start; the prior stop has long completed.
  TEST_ASSERT_EQUAL_FLOAT(50,r.a.motion.state().currentPos);
  TEST_ASSERT_EQUAL_FLOAT(50,r.b.motion.state().currentPos);
  TEST_ASSERT_EQUAL(2,r.transmitNext());
  TEST_ASSERT_EQUAL_FLOAT(50,r.b.motion.state().currentPos);
}
static void equal_deadlines_serialize_stops_and_report_the_resulting_estimates() {
  Rig r; r.start(50);
  r.transmitNext(); r.tick(4626); // 5000ms since shared TX start.
  TEST_ASSERT_EQUAL_FLOAT(50,r.a.motion.state().currentPos);
  TEST_ASSERT_EQUAL_FLOAT(50,r.b.motion.state().currentPos);
  TEST_ASSERT_EQUAL(1,r.transmitNext());
  TEST_ASSERT_EQUAL_FLOAT(50,r.a.motion.state().currentPos);
  TEST_ASSERT_EQUAL_FLOAT(53.74f,r.b.motion.state().currentPos);
  TEST_ASSERT_EQUAL(2,r.transmitNext());
  TEST_ASSERT_EQUAL_FLOAT(53.74f,r.b.motion.state().currentPos);
}
static void pending_slider_updates_keep_one_start_and_apply_the_latest_target() {
  Rig r;
  r.start(5);
  for(float target : {50.0f, 70.0f}) {
    TEST_ASSERT_TRUE(r.a.motion.moveToGroupTarget(target, r.now));
    TEST_ASSERT_TRUE(r.b.motion.moveToGroupTarget(target, r.now));
  }
  TEST_ASSERT_TRUE(r.a.link.sent.empty());
  TEST_ASSERT_TRUE(r.b.link.sent.empty());
  TEST_ASSERT_EQUAL(100, r.transmitNext());
  TEST_ASSERT_EQUAL_FLOAT(70, r.a.motion.state().target);
  TEST_ASSERT_EQUAL_FLOAT(70, r.b.motion.state().target);
  r.tick(6626);
  TEST_ASSERT_EQUAL_FLOAT(70, r.a.motion.state().currentPos);
  TEST_ASSERT_EQUAL_FLOAT(70, r.b.motion.state().currentPos);
  TEST_ASSERT_EQUAL(1, r.a.link.sent.size());
  TEST_ASSERT_EQUAL(1, r.b.link.sent.size());
}
static void same_direction_retarget_keeps_the_original_start_and_clock() {
  Rig r;
  r.start(50);
  r.transmitNext();
  r.tick(626); // Both estimates are10%, moving down on the shared start.
  TEST_ASSERT_TRUE(r.a.motion.moveToGroupTarget(70, r.now));
  TEST_ASSERT_TRUE(r.b.motion.moveToGroupTarget(70, r.now));
  TEST_ASSERT_TRUE(r.a.link.sent.empty());
  TEST_ASSERT_TRUE(r.b.link.sent.empty());
  TEST_ASSERT_EQUAL_FLOAT(70, r.a.motion.state().target);
  r.tick(6000);
  TEST_ASSERT_EQUAL_FLOAT(70, r.a.motion.state().currentPos);
  TEST_ASSERT_EQUAL_FLOAT(70, r.b.motion.state().currentPos);
}
static void pending_endpoint_refresh_can_be_replaced_without_stopping_an_idle_member() {
  Rig r;
  r.start(0); // Already-open estimate; the Up refresh may not move this motor.
  TEST_ASSERT_TRUE(r.a.motion.moveToGroupTarget(50, r.now));
  TEST_ASSERT_TRUE(r.b.motion.moveToGroupTarget(50, r.now));
  TEST_ASSERT_TRUE(r.a.link.sent.empty());
  r.transmitNext();
  // A directional start, rather than My/favorite, must follow the refresh.
  TEST_ASSERT_EQUAL(somfy_commands::Down, r.a.frame.cmd);
  TEST_ASSERT_EQUAL(somfy_commands::Down, r.b.frame.cmd);
}
static void latest_pending_target_matching_original_cancels_an_intermediate_update() {
  Rig r;
  r.start(5);
  TEST_ASSERT_TRUE(r.a.motion.moveToGroupTarget(70, r.now));
  TEST_ASSERT_TRUE(r.a.motion.moveToGroupTarget(5, r.now));
  r.transmitNext();
  TEST_ASSERT_EQUAL_FLOAT(5, r.a.motion.state().target);
  TEST_ASSERT_TRUE(r.a.link.sent.empty());
}
static void delayed_loop_retarget_uses_current_travel_before_choosing_direction() {
  Rig r;
  r.start(80);
  r.transmitNext();
  r.tick(1626); // now3000, estimate20% since TX start1000.
  TEST_ASSERT_EQUAL_FLOAT(20,r.a.motion.state().currentPos);
  r.now+=1000; // The motor travels to30% while loop/model ticks are delayed.
  TEST_ASSERT_TRUE(r.a.motion.moveToGroupTarget(25,r.now));
  TEST_ASSERT_EQUAL_FLOAT(30,r.a.motion.state().currentPos);
  TEST_ASSERT_EQUAL(1,r.a.link.sent.size());
  TEST_ASSERT_EQUAL(somfy_commands::My,r.a.frame.cmd);
  TEST_ASSERT_EQUAL(1,r.transmitNext());
  // Only the completed stop permits a reverse start towards25%.
  TEST_ASSERT_EQUAL(2,r.a.link.sent.size());
  TEST_ASSERT_EQUAL(somfy_commands::Up,r.a.frame.cmd);
  TEST_ASSERT_EQUAL_FLOAT(25,r.a.motion.state().target);
}
static void delayed_loop_at_estimated_limit_restarts_without_favorite() {
  Rig r;
  r.start(80);
  r.transmitNext();
  r.tick(1626);
  TEST_ASSERT_EQUAL_FLOAT(20,r.a.motion.state().currentPos);
  r.now+=9000; // No model tick/stop was possible; timed travel reaches100%.
  TEST_ASSERT_TRUE(r.a.motion.moveToGroupTarget(50,r.now));
  TEST_ASSERT_EQUAL_FLOAT(100,r.a.motion.state().currentPos);
  TEST_ASSERT_EQUAL(1,r.a.link.sent.size());
  TEST_ASSERT_EQUAL(somfy_commands::Up,r.a.frame.cmd);
  TEST_ASSERT_EQUAL_FLOAT(50,r.a.motion.state().target);
}
static void unavailable_radio_does_not_turn_a_group_target_into_limit_travel() {
  Rig r;
  r.start(50);
  somfy_tx_job_t job;
  uint32_t wait;
  TEST_ASSERT_TRUE(r.radio.next(r.now, job, wait));
  TEST_ASSERT_FALSE(r.radio.start(job.ticket, r.now, false));
  r.events();
  r.tick(20000);
  TEST_ASSERT_EQUAL_FLOAT(0, r.a.motion.state().currentPos);
  TEST_ASSERT_EQUAL_FLOAT(0, r.b.motion.state().currentPos);
  TEST_ASSERT_EQUAL_FLOAT(0, r.a.motion.state().target);
  TEST_ASSERT_EQUAL_FLOAT(0, r.b.motion.state().target);
  TEST_ASSERT_TRUE(r.a.link.sent.empty());
  TEST_ASSERT_TRUE(r.b.link.sent.empty());
}
void setUp() {}
void tearDown() {}
int main() {
  UNITY_BEGIN();
  RUN_TEST(delayed_loop_retarget_uses_current_travel_before_choosing_direction);
  RUN_TEST(delayed_loop_at_estimated_limit_restarts_without_favorite);
  RUN_TEST(pending_slider_updates_keep_one_start_and_apply_the_latest_target);
  RUN_TEST(same_direction_retarget_keeps_the_original_start_and_clock);
  RUN_TEST(pending_endpoint_refresh_can_be_replaced_without_stopping_an_idle_member);
  RUN_TEST(latest_pending_target_matching_original_cancels_an_intermediate_update);
  RUN_TEST(unavailable_radio_does_not_turn_a_group_target_into_limit_travel);
  RUN_TEST(long_start_does_not_rewind_a_short_requested_target);
  RUN_TEST(equal_deadlines_serialize_stops_and_report_the_resulting_estimates);
  RUN_TEST(separated_deadlines_reach_each_estimated_target_with_real_airtime);
  return UNITY_END();
}
