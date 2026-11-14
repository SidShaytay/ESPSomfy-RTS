#include <unity.h>
#include <vector>
#include "LiftGuideSession.h"

#define ASSERT_STAGE(expected, snapshot) TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(expected), static_cast<uint8_t>((snapshot).stage))

void setUp() {}
void tearDown() {}

static LiftGuideSource webSource(uint32_t client = 17) {
  LiftGuideSource source;
  source.client = client;
  source.security = 101;
  source.radio = 202;
  return source;
}
struct Job {
  uint8_t id;
  somfy_commands command;
  bool priority;
  uint32_t ticket;
  enum Phase { queued, onAir, finished, dropped, aborted } phase = queued;
};
struct Backend : LiftGuideBackend {
  LiftGuideSource authoritative = webSource();
  LiftGuideContext context{7, 0x123456, 11, 22, 33};
  bool sourceAllowed = true, captureAllowed = true, acquireAllowed = true,
       held = false, rejectSend = false;
  uint64_t lease = 0;
  unsigned acquisitions = 0, releases = 0, anchors = 0, saves = 0;
  uint32_t nextTicket = 40;
  std::vector<Job> jobs;
  std::vector<uint32_t> cancellations;
  LiftGuideSave saveResult{true, true};
  LiftCalibration savedProfile;
  uint32_t savedUp = 0, savedDown = 0;
  bool current(const LiftGuideSource &source) override {
    return sourceAllowed && authoritative.same(source);
  }
  bool capture(uint8_t id, LiftGuideContext &out) override {
    if(!captureAllowed || id != 7) return false;
    out = context;
    return true;
  }
  bool acquire(uint64_t &out) override {
    ++acquisitions;
    if(!acquireAllowed || held) return false;
    held = true;
    out = ++lease;
    return true;
  }
  bool owns(uint64_t owner) override { return held && owner == lease; }
  bool release(uint64_t owner) override {
    if(!owns(owner)) return false;
    held = false;
    ++releases;
    return true;
  }
  send_result_t send(uint8_t id, somfy_commands command, bool priority) override {
    if(rejectSend) return {};
    const auto ticket = ++nextTicket;
    jobs.push_back({id, command, priority, ticket});
    return {true, ticket};
  }
  Job *find(uint32_t ticket) {
    for(auto &job : jobs) if(job.ticket == ticket) return &job;
    return nullptr;
  }
  bool cancel(uint32_t ticket) override {
    auto *job = find(ticket);
    if(!job || job->phase != Job::queued) return false;
    job->phase = Job::dropped;
    cancellations.push_back(ticket);
    return true;
  }
  bool anchor(uint8_t id, bool) override {
    if(id != 7) return false;
    ++anchors;
    return true;
  }
  LiftGuideSave save(uint8_t id, const LiftCalibration &profile, uint32_t up, uint32_t down) override {
    TEST_ASSERT_EQUAL_UINT8(7, id);
    ++saves;
    savedProfile = profile;
    savedUp = up;
    savedDown = down;
    return saveResult;
  }
};
struct Rig {
  Backend backend;
  LiftGuideSession session{backend, 0x12340000};
  LiftGuideSource source = webSource();
  uint32_t token = 0;
  uint64_t now = 100;
  void begin(LiftKind kind = LiftKind::direct) {
    TEST_ASSERT_TRUE(session.begin(source, 7, kind, now, token));
    TEST_ASSERT_NOT_EQUAL_UINT32(0, token);
    TEST_ASSERT_TRUE(session.active());
  }
  void act(LiftGuideAction action, uint64_t at, bool human = true) {
    now = at;
    TEST_ASSERT_TRUE(session.act(source, token, action, now, human));
  }
  void reject(LiftGuideAction action, uint64_t at, bool human = true) {
    now = at;
    TEST_ASSERT_FALSE(session.act(source, token, action, now, human));
  }
  LiftGuideSnapshot snapshot(uint64_t at) {
    now = at;
    LiftGuideSnapshot out;
    TEST_ASSERT_TRUE(session.snapshot(source, token, now, out));
    return out;
  }
  void event(tx_event_t type, uint32_t ticket, uint32_t at) {
    now = at;
    auto *job = backend.find(ticket);
    somfy_tx_event_t event;
    event.type = type;
    event.ticket = ticket;
    event.at = at;
    event.remoteAddress = backend.context.identity;
    if(job) {
      event.cmd = job->command;
      if(type == tx_event_t::started) job->phase = Job::onAir;
      if(type == tx_event_t::finished) job->phase = Job::finished;
      if(type == tx_event_t::dropped) job->phase = Job::dropped;
      if(type == tx_event_t::aborted) job->phase = Job::aborted;
    }
    session.txEvent(event);
  }
  uint32_t queuedDown() {
    begin();
    act(LiftGuideAction::confirmOpen, 200);
    act(LiftGuideAction::startDown, 300);
    TEST_ASSERT_EQUAL_UINT(1, backend.jobs.size());
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(somfy_commands::Down), static_cast<uint8_t>(backend.jobs[0].command));
    return backend.jobs[0].ticket;
  }
  void review() {
    const auto down = queuedDown();
    event(tx_event_t::started, down, 1000);
    event(tx_event_t::finished, down, 1274);
    act(LiftGuideAction::halfway, 4000);
    act(LiftGuideAction::confirmEndpoint, 11000);
    act(LiftGuideAction::startUp, 12000);
    TEST_ASSERT_EQUAL_UINT(2, backend.jobs.size());
    const auto up = backend.jobs.back().ticket;
    event(tx_event_t::started, up, 20000);
    event(tx_event_t::finished, up, 20274);
    act(LiftGuideAction::halfway, 32000);
    act(LiftGuideAction::confirmEndpoint, 40000);
    const auto state = snapshot(40001);
    ASSERT_STAGE(LiftGuideStage::review, state);
    TEST_ASSERT_TRUE(state.owned);
  }
};
static void test_invalid_source_target_or_busy_backend_refuses_before_rf() {
  for(unsigned fault = 0; fault < 4; ++fault) {
    Rig r;
    if(fault == 0) r.backend.sourceAllowed = false;
    if(fault == 1) r.backend.captureAllowed = false;
    if(fault == 2) r.backend.acquireAllowed = false;
    if(fault == 3) r.source.security = 0;
    TEST_ASSERT_FALSE(r.session.begin(r.source, 7, LiftKind::direct, 100, r.token));
    TEST_ASSERT_FALSE(r.session.active());
    TEST_ASSERT_FALSE(r.backend.held);
    TEST_ASSERT_TRUE(r.backend.jobs.empty());
    TEST_ASSERT_EQUAL_UINT(0, r.backend.saves);
  }
}
static void test_one_owner_token_and_source_cannot_be_used_by_another_client() {
  Rig r;
  r.begin();
  const auto first = r.token;
  LiftGuideSession competitor(r.backend, 0x56780000);
  uint32_t other = 0;
  TEST_ASSERT_FALSE(competitor.begin(r.source, 7, LiftKind::direct, 150, other));
  auto outsider = webSource(18);
  TEST_ASSERT_FALSE(r.session.act(outsider, r.token, LiftGuideAction::open, 160));
  TEST_ASSERT_FALSE(r.session.act(r.source, r.token + 1, LiftGuideAction::open, 170));
  TEST_ASSERT_TRUE(r.backend.jobs.empty());
  r.act(LiftGuideAction::cancel, 200);
  TEST_ASSERT_FALSE(r.session.active());
  r.now = 300;
  r.begin();
  TEST_ASSERT_NOT_EQUAL_UINT32(first, r.token);
  TEST_ASSERT_FALSE(r.session.act(r.source, first, LiftGuideAction::open, 310));
  TEST_ASSERT_TRUE(r.backend.jobs.empty());
  // Local LCD ownership uses the local session/configuration pair instead of
  // a web client ID; changing either cannot reuse the same guide token.
  Rig local;
  local.source.client = 0;
  local.source.session = 71;
  local.source.configuration = 81;
  local.backend.authoritative = local.source;
  local.begin();
  auto stale = local.source;
  ++stale.configuration;
  TEST_ASSERT_FALSE(local.session.act(stale, local.token, LiftGuideAction::open, 200));
  TEST_ASSERT_TRUE(local.backend.jobs.empty());
}
static void test_context_or_authorization_history_changes_cancel_queued_start_without_my() {
  for(unsigned change = 0; change < 6; ++change) {
    Rig r;
    const auto ticket = r.queuedDown();
    if(change == 0) ++r.backend.authoritative.security;
    if(change == 1) ++r.backend.authoritative.radio;
    if(change == 2) ++r.backend.context.model;
    if(change == 3) ++r.backend.context.shade;
    if(change == 4) ++r.backend.context.restore;
    if(change == 5) ++r.backend.context.identity;
    r.session.poll(500);
    TEST_ASSERT_EQUAL_UINT(1, r.backend.cancellations.size());
    TEST_ASSERT_EQUAL_UINT32(ticket, r.backend.cancellations[0]);
    r.event(tx_event_t::dropped, ticket, 500);
    TEST_ASSERT_FALSE(r.session.active());
    TEST_ASSERT_EQUAL_UINT(1, r.backend.jobs.size());
    TEST_ASSERT_EQUAL_UINT(1, r.backend.releases);
  }
}
static void test_session_expiry_and_backwards_clock_cancel_unstarted_work() {
  for(bool backwards : {false, true}) {
    Rig r;
    const auto ticket = r.queuedDown();
    const uint64_t expired = backwards ? 299 : 101 + LiftGuideSession::maximumMs;
    r.session.poll(expired);
    TEST_ASSERT_EQUAL_UINT(1, r.backend.cancellations.size());
    r.event(tx_event_t::dropped, ticket, static_cast<uint32_t>(expired));
    TEST_ASSERT_FALSE(r.session.active());
    TEST_ASSERT_EQUAL_UINT(1, r.backend.jobs.size());
    TEST_ASSERT_EQUAL_UINT(1, r.backend.releases);
  }
}
static void test_real_ticket_origin_and_human_markers_are_required_before_endpoint_anchor() {
  Rig r;
  r.begin();
  r.reject(LiftGuideAction::confirmOpen, 200, false);
  TEST_ASSERT_EQUAL_UINT(0, r.backend.anchors);
  r.act(LiftGuideAction::confirmOpen, 250);
  r.act(LiftGuideAction::startDown, 300);
  const auto ticket = r.backend.jobs[0].ticket;
  r.event(tx_event_t::started, ticket + 1, 500);
  ASSERT_STAGE(LiftGuideStage::startingDown, r.snapshot(600));
  r.event(tx_event_t::started, ticket, 1000);
  r.event(tx_event_t::finished, ticket + 1, 1200);
  r.event(tx_event_t::finished, ticket, 1274);
  r.reject(LiftGuideAction::halfway, 2000, false);
  r.act(LiftGuideAction::halfway, 4000);
  r.act(LiftGuideAction::confirmEndpoint, 11000);
  const auto state = r.snapshot(11001);
  TEST_ASSERT_EQUAL_UINT32(10000, state.downTime);
  TEST_ASSERT_EQUAL_UINT32(3000, state.calibration.down.midpointMs);
  TEST_ASSERT_EQUAL_UINT(2, r.backend.anchors);
}
static void test_stop_before_rf_start_cancels_ticket_without_my() {
  Rig r;
  const auto ticket = r.queuedDown();
  r.act(LiftGuideAction::stop, 400);
  TEST_ASSERT_EQUAL_UINT(1, r.backend.cancellations.size());
  TEST_ASSERT_EQUAL_UINT32(ticket, r.backend.cancellations[0]);
  TEST_ASSERT_EQUAL_UINT(1, r.backend.jobs.size());
  r.event(tx_event_t::dropped, ticket, 500);
  TEST_ASSERT_FALSE(r.session.active());
  const auto state = r.snapshot(600);
  ASSERT_STAGE(LiftGuideStage::failed, state);
  TEST_ASSERT_FALSE(state.owned);
}
static void test_stop_after_rf_start_waits_for_finished_press_before_my() {
  Rig r;
  const auto ticket = r.queuedDown();
  r.event(tx_event_t::started, ticket, 1000);
  r.act(LiftGuideAction::stop, 1100);
  TEST_ASSERT_EQUAL_UINT(1, r.backend.jobs.size());
  r.event(tx_event_t::finished, ticket, 1274);
  TEST_ASSERT_EQUAL_UINT(2, r.backend.jobs.size());
  const auto stop = r.backend.jobs.back();
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(somfy_commands::My), static_cast<uint8_t>(stop.command));
  TEST_ASSERT_TRUE(stop.priority);
  r.event(tx_event_t::started, stop.ticket, 1300);
  r.event(tx_event_t::finished, stop.ticket, 1574);
  TEST_ASSERT_FALSE(r.session.active());
  ASSERT_STAGE(LiftGuideStage::failed, r.snapshot(1600));
  TEST_ASSERT_EQUAL_UINT(1, r.backend.releases);
}
static void test_dropped_or_partial_start_never_automatically_follows_with_my() {
  for(bool partial : {false, true}) {
    Rig r;
    const auto ticket = r.queuedDown();
    if(partial) {
      r.event(tx_event_t::started, ticket, 1000);
      r.act(LiftGuideAction::stop, 1100);
    }
    r.event(partial ? tx_event_t::aborted : tx_event_t::dropped, ticket, 1200);
    TEST_ASSERT_EQUAL_UINT(1, r.backend.jobs.size());
    TEST_ASSERT_FALSE(r.session.active());
    ASSERT_STAGE(LiftGuideStage::failed, r.snapshot(1300));
    r.reject(LiftGuideAction::save, 1400);
    TEST_ASSERT_EQUAL_UINT(0, r.backend.saves);
  }
}
static void test_durable_save_releases_owner_once_and_cannot_replay() {
  Rig r;
  r.review();
  r.act(LiftGuideAction::save, 41000);
  TEST_ASSERT_EQUAL_UINT(1, r.backend.saves);
  TEST_ASSERT_FALSE(r.session.active());
  const auto state = r.snapshot(41001);
  ASSERT_STAGE(LiftGuideStage::complete, state);
  TEST_ASSERT_TRUE(state.applied && state.durable);
  TEST_ASSERT_FALSE(state.owned);
  TEST_ASSERT_EQUAL_UINT32(10000, r.backend.savedDown);
  TEST_ASSERT_EQUAL_UINT32(20000, r.backend.savedUp);
  TEST_ASSERT_TRUE(validLiftCalibration(r.backend.savedProfile, r.backend.savedUp, r.backend.savedDown));
  r.reject(LiftGuideAction::save, 42000);
  TEST_ASSERT_EQUAL_UINT(1, r.backend.saves);
  TEST_ASSERT_EQUAL_UINT(1, r.backend.releases);
}
static void test_applied_undurable_save_exposes_attention_without_save_or_rf_replay() {
  Rig r;
  r.backend.saveResult = {true, false};
  r.review();
  r.act(LiftGuideAction::save, 41000);
  TEST_ASSERT_EQUAL_UINT(1, r.backend.saves);
  TEST_ASSERT_FALSE(r.session.active());
  const auto state = r.snapshot(41001);
  ASSERT_STAGE(LiftGuideStage::failed, state);
  TEST_ASSERT_TRUE(state.applied);
  TEST_ASSERT_FALSE(state.durable || state.owned);
  r.reject(LiftGuideAction::save, 42000);
  r.reject(LiftGuideAction::startDown, 42001);
  r.reject(LiftGuideAction::startUp, 42002);
  TEST_ASSERT_EQUAL_UINT(1, r.backend.saves);
  TEST_ASSERT_EQUAL_UINT(2, r.backend.jobs.size());
}

static void test_lost_begin_reply_can_resume_same_source_without_restarting_or_reacquiring() {
  Rig r; r.begin();
  const auto first = r.token;
  uint32_t resumed = 0;
  TEST_ASSERT_TRUE(r.session.begin(r.source,7,LiftKind::direct,150,resumed));
  TEST_ASSERT_EQUAL_UINT32(first,resumed);
  TEST_ASSERT_EQUAL_UINT(1,r.backend.acquisitions);
  TEST_ASSERT_TRUE(r.backend.jobs.empty());
  TEST_ASSERT_FALSE(r.session.begin(r.source,7,LiftKind::segmented,160,resumed));
}
int main() {
  UNITY_BEGIN();
  RUN_TEST(test_lost_begin_reply_can_resume_same_source_without_restarting_or_reacquiring);
  RUN_TEST(test_invalid_source_target_or_busy_backend_refuses_before_rf);
  RUN_TEST(test_one_owner_token_and_source_cannot_be_used_by_another_client);
  RUN_TEST(test_context_or_authorization_history_changes_cancel_queued_start_without_my);
  RUN_TEST(test_session_expiry_and_backwards_clock_cancel_unstarted_work);
  RUN_TEST(test_real_ticket_origin_and_human_markers_are_required_before_endpoint_anchor);
  RUN_TEST(test_stop_before_rf_start_cancels_ticket_without_my);
  RUN_TEST(test_stop_after_rf_start_waits_for_finished_press_before_my);
  RUN_TEST(test_dropped_or_partial_start_never_automatically_follows_with_my);
  RUN_TEST(test_durable_save_releases_owner_once_and_cannot_replay);
  RUN_TEST(test_applied_undurable_save_exposes_attention_without_save_or_rf_replay);
  return UNITY_END();
}
