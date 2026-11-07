#include <unity.h>
#include <initializer_list>
#include "GroupMotion.h"
#include "ShadeBehaviour.h"
#include "RadioScheduler.h"

struct Events : ShadeEvents {
  void onChanged(uint16_t) override {}
  void onCommand(somfy_commands, const char *, uint32_t) override {}
  void onSaveWanted() override {}
  void onSensorFlags() override {}
};
// Mirrors SomfyShade's admission hook: only an accepted external movement
// command supersedes the previous positioning job, preserving its new ticket.
struct AdmissionLink : MotorLink {
  RadioScheduler radio;
  somfy_frame_t frame;
  ShadeBehaviour *motion = nullptr;
  uint32_t now = 1000;
  bool prepareOnSend = false;
  bool refuse = false;
  send_result_t send(somfy_commands cmd, uint8_t repeat, uint8_t step,
      uint16_t delay, bool priority) override {
    if(refuse) return {};
    somfy_tx_job_t job;
    job.frame.remoteAddress = 1;
    job.frame.cmd = cmd;
    job.frame.valid = true;
    job.frame.bitLength = 56;
    job.frame.repeats = repeat;
    job.frame.stepSize = step;
    send_result_t result;
    result.ticket = radio.submit(job, priority, delay, now);
    result.sent = result.ticket != 0;
    if(result.sent) {
      frame = job.frame;
      if(prepareOnSend) {
        prepareOnSend = false;
        radio.cancelRemote(1, now, result.ticket);
        motion->cancelPositioning();
      }
    }
    return result;
  }
};
struct Rig {
  ShadeSettings settings;
  RemoteSettings remote;
  AdmissionLink link;
  Events events;
  uint8_t id = 1;
  ShadeBehaviour motion{settings, remote, link.frame, link, events, id, "member"};
  Rig() {
    remote.bitLength = 56;
    settings.downTime = 10000;
    link.motion = &motion;
  }
  void drainEvents() {
    somfy_tx_event_t event;
    while(link.radio.popEvent(event)) motion.onTxEvent(event);
  }
  somfy_commands transmitNext() {
    somfy_tx_job_t job;
    uint32_t wait;
    TEST_ASSERT_TRUE(link.radio.next(link.now, job, wait));
    TEST_ASSERT_TRUE(link.radio.start(job.ticket, link.now));
    drainEvents();
    link.radio.ended(job.ticket, true, link.now);
    drainEvents();
    return job.frame.cmd;
  }
  void queueTargetStop() {
    somfy_tx_job_t start;
    start.frame.remoteAddress = 100;
    start.frame.cmd = somfy_commands::Down;
    uint32_t ticket = link.radio.submit(start, false, 0, link.now);
    TEST_ASSERT_NOT_EQUAL(0, ticket);
    motion.adoptGroupTarget(50, ticket, link.now);
    TEST_ASSERT_EQUAL(somfy_commands::Down, transmitNext());
    link.now += 5000;
    motion.tick(link.now);
    TEST_ASSERT_EQUAL_FLOAT(50, motion.state().currentPos);
    TEST_ASSERT_FALSE(motion.isIdle());
  }
  bool external(somfy_commands cmd) {
    link.prepareOnSend = commandSupersedesPositioning(cmd);
    const bool accepted = motion.sendCommand(cmd, 1, 0, link.now);
    link.prepareOnSend = false;
    return accepted;
  }
};
static void nonmovement_commands_preserve_queued_group_stop() {
  for(somfy_commands cmd : {somfy_commands::Flag, somfy_commands::SunFlag, somfy_commands::Sensor}) {
    Rig r;
    r.queueTargetStop();
    TEST_ASSERT_TRUE(r.external(cmd));
    // SunFlag may update the sensor target; it must retain the admitted stop.
    if(cmd == somfy_commands::Flag) TEST_ASSERT_EQUAL_FLOAT(50, r.motion.state().target);
    TEST_ASSERT_EQUAL(somfy_commands::My, r.transmitNext());
    TEST_ASSERT_TRUE(r.motion.isIdle());
    TEST_ASSERT_EQUAL_FLOAT(50, r.motion.state().currentPos);
  }
}
static void group_flag_before_target_retains_timed_stop() {
  for(somfy_commands cmd : {somfy_commands::Flag, somfy_commands::SunFlag}) {
    Rig r;
    r.motion.adoptGroupTarget(50, 81, r.link.now);
    somfy_tx_event_t start;
    start.type = tx_event_t::started;
    start.ticket = 81;
    start.at = r.link.now;
    r.motion.onTxEvent(start);
    start.type = tx_event_t::finished;
    r.motion.onTxEvent(start);
    r.link.now += 2500;
    r.motion.tick(r.link.now);
    TEST_ASSERT_EQUAL_FLOAT(25, r.motion.state().currentPos);
    r.motion.onGroupCommand(cmd, 1, r.link.now);
    r.link.now += 2500;
    r.motion.tick(r.link.now);
    TEST_ASSERT_EQUAL_FLOAT(50, r.motion.state().currentPos);
    TEST_ASSERT_EQUAL(somfy_commands::My, r.transmitNext());
    TEST_ASSERT_TRUE(r.motion.isIdle());
  }
}
static void accepted_movement_cancels_stale_stop_and_preserves_replacement() {
  Rig r;
  r.queueTargetStop();
  TEST_ASSERT_TRUE(r.external(somfy_commands::Up));
  TEST_ASSERT_EQUAL_FLOAT(0, r.motion.state().target);
  TEST_ASSERT_EQUAL(somfy_commands::Up, r.transmitNext());
}
static void refused_movement_keeps_the_previous_stop() {
  Rig r;
  r.queueTargetStop();
  r.link.refuse = true;
  TEST_ASSERT_FALSE(r.external(somfy_commands::Up));
  TEST_ASSERT_EQUAL_FLOAT(50, r.motion.state().target);
  TEST_ASSERT_EQUAL(somfy_commands::My, r.transmitNext());
  TEST_ASSERT_TRUE(r.motion.isIdle());
}
void setUp() {}
void tearDown() {}
int main() {
  UNITY_BEGIN();
  RUN_TEST(nonmovement_commands_preserve_queued_group_stop);
  RUN_TEST(group_flag_before_target_retains_timed_stop);
  RUN_TEST(accepted_movement_cancels_stale_stop_and_preserves_replacement);
  RUN_TEST(refused_movement_keeps_the_previous_stop);
  return UNITY_END();
}
