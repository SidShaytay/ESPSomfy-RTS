#include <unity.h>
#include <vector>
#include "GroupMotion.h"
#include "ShadeBehaviour.h"
#include "RadioScheduler.h"

// The planner, member movement models and radio queue run together with a
// deterministic clock. Only the physical transmitter and persistence are fake.
struct Events : ShadeEvents {
  void onChanged(uint16_t) override {}
  void onCommand(somfy_commands, const char *, uint32_t) override {}
  void onSaveWanted() override {}
  void onSensorFlags() override {}
};
struct Submission { uint32_t address, at; somfy_commands command; };
struct Link : MotorLink {
  RadioScheduler &radio;
  uint32_t &now;
  somfy_frame_t &frame;
  std::vector<Submission> &sent;
  uint32_t address;
  Link(RadioScheduler &radio, uint32_t &now, somfy_frame_t &frame,
      std::vector<Submission> &sent, uint32_t address)
    : radio(radio), now(now), frame(frame), sent(sent), address(address) {}
  send_result_t send(somfy_commands cmd, uint8_t repeat, uint8_t step,
      uint16_t delay, bool priority) override {
    somfy_tx_job_t job;
    job.frame.remoteAddress = address;
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
      sent.push_back({address, now, cmd});
    }
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
  Member(RadioScheduler &radio, uint32_t &now, std::vector<Submission> &sent, uint8_t id)
    : id(id), link(radio, now, frame, sent, id),
      motion(settings, remote, frame, link, events, this->id, "member") {
    remote.bitLength = 56;
  }
};
struct Rig : GroupMotionOps, GroupStopOps {
  RadioScheduler radio;
  uint32_t now = 1000;
  std::vector<Submission> sent;
  Member a{radio, now, sent, 1}, b{radio, now, sent, 2};
  unsigned groupStarts = 0;
  unsigned individualPlans = 0;
  uint32_t groupTicket = 0;
  Member &at(size_t i) { return i == 0 ? a : b; }
  size_t memberCount() override { return 2; }
  bool memberIdle(size_t i) override {
    return at(i).motion.isIdle() && !at(i).motion.hasGroupTargetWork();
  }
  bool stopMember(size_t i) override { return at(i).motion.stopGroupTarget(now); }
  bool member(size_t i, GroupMemberMotion &out) override {
    Member &m = at(i);
    out.current = m.motion.state().currentPos;
    out.idle = m.motion.isIdle() && !m.motion.hasGroupTargetWork();
    out.targetSupported = !m.motion.hasPendingGroupStart() || m.motion.ownsGroupPositioning();
    return true;
  }
  bool startGroup(int dir, float target) override {
    somfy_tx_job_t job;
    job.frame.remoteAddress = 100;
    job.frame.cmd = dir < 0 ? somfy_commands::Up : somfy_commands::Down;
    groupTicket = radio.submit(job, false, 0, now);
    if(!groupTicket) return false;
    ++groupStarts;
    for(size_t i = 0; i < 2; ++i) at(i).motion.adoptGroupTarget(target, groupTicket, now);
    return true;
  }
  bool moveMember(size_t i, float target) override {
    ++individualPlans;
    return at(i).motion.moveToGroupTarget(target, now);
  }
  void events() {
    somfy_tx_event_t event;
    while(radio.popEvent(event)) {
      a.motion.onTxEvent(event);
      b.motion.onTxEvent(event);
    }
  }
  void tick(uint32_t duration) {
    for(uint32_t elapsed = 0; elapsed < duration; elapsed += 10) {
      now += 10;
      a.motion.tick(now);
      b.motion.tick(now);
    }
  }
  uint32_t transmitNext() {
    somfy_tx_job_t job;
    uint32_t wait;
    TEST_ASSERT_TRUE(radio.next(now, job, wait));
    TEST_ASSERT_TRUE(radio.start(job.ticket, now));
    events();
    radio.ended(job.ticket, true, now);
    events();
    return job.frame.remoteAddress;
  }
};

static void shared_start_waits_for_air_and_stops_members_at_separate_times() {
  Rig r;
  r.a.settings.downTime = 10000;
  r.b.settings.downTime = 20000;
  TEST_ASSERT_TRUE(runGroupTarget(r, 50));
  TEST_ASSERT_EQUAL(1, r.groupStarts);
  TEST_ASSERT_EQUAL(0, r.individualPlans);
  r.tick(3000); // A busy transmitter has not started the group yet.
  TEST_ASSERT_EQUAL_FLOAT(0, r.a.motion.state().currentPos);
  TEST_ASSERT_EQUAL_FLOAT(0, r.b.motion.state().currentPos);
  TEST_ASSERT_EQUAL(100, r.transmitNext());
  r.tick(5000);
  TEST_ASSERT_EQUAL_FLOAT(50, r.a.motion.state().currentPos);
  TEST_ASSERT_EQUAL_FLOAT(25, r.b.motion.state().currentPos);
  TEST_ASSERT_EQUAL(1, r.sent.size());
  TEST_ASSERT_EQUAL(1, r.sent[0].address);
  TEST_ASSERT_EQUAL(9000, r.sent[0].at);
  TEST_ASSERT_EQUAL(somfy_commands::My, r.sent[0].command);
  TEST_ASSERT_EQUAL(1, r.transmitNext());
  r.tick(5000);
  TEST_ASSERT_EQUAL_FLOAT(50, r.b.motion.state().currentPos);
  TEST_ASSERT_EQUAL(2, r.sent.size());
  TEST_ASSERT_EQUAL(2, r.sent[1].address);
  TEST_ASSERT_EQUAL(14000, r.sent[1].at);
  TEST_ASSERT_EQUAL(2, r.transmitNext());
  TEST_ASSERT_TRUE(r.a.motion.isAtTarget());
  TEST_ASSERT_TRUE(r.b.motion.isAtTarget());
}
static void correct_member_prevents_shared_start() {
  Rig r;
  r.a.motion.restore(50, 0, -1, -1);
  TEST_ASSERT_TRUE(runGroupTarget(r, 50));
  TEST_ASSERT_EQUAL(0, r.groupStarts);
  TEST_ASSERT_EQUAL(1, r.individualPlans);
  TEST_ASSERT_EQUAL(1, r.sent.size());
  TEST_ASSERT_EQUAL(2, r.sent[0].address);
}
static void refused_shared_start_leaves_member_targets_unchanged() {
  Rig r;
  r.radio.suspend(TX_SUSPEND_OTA, r.now);
  TEST_ASSERT_FALSE(runGroupTarget(r, 50));
  TEST_ASSERT_EQUAL(0, r.sent.size());
  TEST_ASSERT_EQUAL_FLOAT(0, r.a.motion.state().target);
  TEST_ASSERT_EQUAL_FLOAT(0, r.b.motion.state().target);
}
static void dropped_group_start_does_not_manufacture_movement() {
  Rig r;
  TEST_ASSERT_TRUE(runGroupTarget(r, 50));
  r.radio.flush(r.now);
  r.events();
  r.tick(20000);
  TEST_ASSERT_EQUAL_FLOAT(0, r.a.motion.state().currentPos);
  TEST_ASSERT_EQUAL_FLOAT(0, r.b.motion.state().currentPos);
  TEST_ASSERT_EQUAL(0, r.sent.size());
}
static void all_correct_members_do_not_send_a_start_or_stop() {
  Rig r;
  r.a.motion.restore(50, 0, -1, -1);
  r.b.motion.restore(50, 0, -1, -1);
  TEST_ASSERT_TRUE(runGroupTarget(r, 50));
  TEST_ASSERT_EQUAL(0, r.groupStarts);
  TEST_ASSERT_EQUAL(0, r.individualPlans);
  TEST_ASSERT_EQUAL(0, r.sent.size());
}
static void opposite_directions_use_individual_starts() {
  Rig r;
  r.a.motion.restore(20, 0, -1, -1);
  r.b.motion.restore(80, 0, -1, -1);
  TEST_ASSERT_TRUE(runGroupTarget(r, 50));
  TEST_ASSERT_EQUAL(0, r.groupStarts);
  TEST_ASSERT_EQUAL(2, r.sent.size());
  TEST_ASSERT_EQUAL(somfy_commands::Down, r.sent[0].command);
  TEST_ASSERT_EQUAL(somfy_commands::Up, r.sent[1].command);
}
static void repeated_endpoints_still_submit_native_commands() {
  Rig r;
  TEST_ASSERT_TRUE(runGroupTarget(r, 0));
  TEST_ASSERT_EQUAL(100, r.transmitNext());
  r.tick(10);
  TEST_ASSERT_TRUE(runGroupTarget(r, 0));
  TEST_ASSERT_EQUAL(100, r.transmitNext());
  TEST_ASSERT_EQUAL(2, r.groupStarts);
  TEST_ASSERT_EQUAL(0, r.sent.size());
}
static void reversal_stops_each_member_before_its_new_start() {
  Rig r;
  TEST_ASSERT_TRUE(runGroupTarget(r, 50));
  TEST_ASSERT_EQUAL(100, r.transmitNext());
  r.tick(2000);
  TEST_ASSERT_TRUE(runGroupTarget(r, 10));
  TEST_ASSERT_EQUAL(1, r.groupStarts);
  TEST_ASSERT_EQUAL(2, r.sent.size());
  TEST_ASSERT_EQUAL(somfy_commands::My, r.sent[0].command);
  TEST_ASSERT_EQUAL(somfy_commands::My, r.sent[1].command);
  TEST_ASSERT_EQUAL(1, r.transmitNext());
  TEST_ASSERT_EQUAL(3, r.sent.size());
  TEST_ASSERT_EQUAL(somfy_commands::Up, r.sent[2].command);
  TEST_ASSERT_EQUAL(2, r.transmitNext());
  TEST_ASSERT_EQUAL(4, r.sent.size());
  TEST_ASSERT_EQUAL(somfy_commands::Up, r.sent[3].command);
  TEST_ASSERT_EQUAL(1, r.transmitNext());
  TEST_ASSERT_EQUAL(2, r.transmitNext());
  r.tick(1000);
  TEST_ASSERT_EQUAL_FLOAT(10, r.a.motion.state().currentPos);
  TEST_ASSERT_EQUAL_FLOAT(10, r.b.motion.state().currentPos);
}
static void pending_shared_start_coalesces_replacement_without_cancelling_it() {
  Rig r;
  TEST_ASSERT_TRUE(runGroupTarget(r, 50));
  TEST_ASSERT_TRUE(runGroupTarget(r, 70));
  TEST_ASSERT_EQUAL(1, r.groupStarts);
  TEST_ASSERT_EQUAL(0, r.sent.size());
  TEST_ASSERT_EQUAL(100, r.transmitNext());
  TEST_ASSERT_EQUAL_FLOAT(70, r.a.motion.state().target);
  TEST_ASSERT_EQUAL_FLOAT(70, r.b.motion.state().target);
  r.tick(2000);
  TEST_ASSERT_EQUAL_FLOAT(20, r.a.motion.state().currentPos);
}
static void group_stop_skips_completed_member_and_stops_remaining_member() {
  Rig r;
  r.a.settings.downTime = 10000;
  r.b.settings.downTime = 20000;
  r.a.motion.restore(0, 0, 75, -1);
  TEST_ASSERT_TRUE(runGroupTarget(r, 50));
  r.transmitNext();
  r.tick(5000);
  TEST_ASSERT_EQUAL(1, r.transmitNext()); // A has stopped; My now recalls favorite.
  TEST_ASSERT_TRUE(r.a.motion.isIdle());
  TEST_ASSERT_FALSE(r.b.motion.isIdle());
  TEST_ASSERT_TRUE(runGroupStop(r));
  TEST_ASSERT_EQUAL(2, r.sent.size());
  TEST_ASSERT_EQUAL(2, r.sent[1].address);
  TEST_ASSERT_EQUAL(somfy_commands::My, r.sent[1].command);
  TEST_ASSERT_EQUAL(2, r.transmitNext());
  r.tick(1000);
  TEST_ASSERT_EQUAL_FLOAT(50, r.a.motion.state().currentPos);
  TEST_ASSERT_EQUAL_FLOAT(25, r.b.motion.state().currentPos);
  TEST_ASSERT_TRUE(r.b.motion.isIdle());
}
int main() {
  UNITY_BEGIN();
  RUN_TEST(shared_start_waits_for_air_and_stops_members_at_separate_times);
  RUN_TEST(correct_member_prevents_shared_start);
  RUN_TEST(refused_shared_start_leaves_member_targets_unchanged);
  RUN_TEST(dropped_group_start_does_not_manufacture_movement);
  RUN_TEST(all_correct_members_do_not_send_a_start_or_stop);
  RUN_TEST(opposite_directions_use_individual_starts);
  RUN_TEST(repeated_endpoints_still_submit_native_commands);
  RUN_TEST(reversal_stops_each_member_before_its_new_start);
  RUN_TEST(pending_shared_start_coalesces_replacement_without_cancelling_it);
  RUN_TEST(group_stop_skips_completed_member_and_stops_remaining_member);
  return UNITY_END();
}
