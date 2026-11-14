#pragma once
#include "LiftGuide.h"
#include "ShadeBehaviour.h"
struct LiftGuideSource {
  uint32_t client = 0, session = 0, configuration = 0;
  uint64_t security = 0, radio = 0;
  bool valid() const { return security && radio && (client ? !session && !configuration : session && configuration); }
  bool same(const LiftGuideSource &s) const {
    return valid() && s.valid() && client == s.client && session == s.session &&
      configuration == s.configuration && security == s.security && radio == s.radio;
  }
};
struct LiftGuideContext {
  uint8_t id = 0;
  uint32_t identity = 0;
  uint64_t model = 0, shade = 0, restore = 0;
  bool same(const LiftGuideContext &c) const {
    return id && identity && model && shade && restore && id == c.id && identity == c.identity &&
      model == c.model && shade == c.shade && restore == c.restore;
  }
};
struct LiftGuideSave { bool applied = false, durable = false; };
class LiftGuideBackend {
public:
  virtual ~LiftGuideBackend() = default;
  virtual bool current(const LiftGuideSource &) = 0;
  virtual bool capture(uint8_t id, LiftGuideContext &) = 0;
  virtual bool acquire(uint64_t &lease) = 0;
  virtual bool owns(uint64_t lease) = 0;
  virtual bool release(uint64_t lease) = 0;
  virtual send_result_t send(uint8_t id, somfy_commands, bool priority) = 0;
  virtual bool cancel(uint32_t ticket) = 0;
  virtual bool anchor(uint8_t id, bool closed) = 0;
  virtual bool anchorHalf(uint8_t) { return false; }
  virtual LiftGuideSave save(uint8_t id, const LiftCalibration &, uint32_t up, uint32_t down) = 0;
};
struct LiftGuideSnapshot {
  uint32_t token = 0;
  uint8_t id = 0;
  LiftGuideStage stage = LiftGuideStage::closed;
  LiftCalibration calibration;
  uint32_t upTime = 0, downTime = 0;
  bool owned = false, applied = false, durable = false, reversalsKnown = false;
};
class LiftGuideSession {
  LiftGuideBackend &backend;
  LiftGuide guide;
  LiftGuideSource source;
  LiftGuideContext context;
  uint32_t generation = 0, pending = 0;
  uint64_t lease = 0, admittedAt = 0, observedAt = 0;
  bool held = false, started = false, stopAfterStart = false, stopping = false;
  LiftGuideSave savedResult;
  bool fresh(uint64_t now);
  bool release();
  void fail();
  bool send(somfy_commands, bool stop);
public:
  static constexpr uint64_t maximumMs = 1800000;
  LiftGuideSession(LiftGuideBackend &b, uint32_t seed) : backend(b), generation(seed) {}
  bool begin(const LiftGuideSource &, uint8_t id, LiftKind, uint64_t now, uint32_t &token);
  bool act(const LiftGuideSource &, uint32_t token, LiftGuideAction, uint64_t now, bool human = true);
  bool snapshot(const LiftGuideSource &, uint32_t token, uint64_t now, LiftGuideSnapshot &);
  void txEvent(const somfy_tx_event_t &);
  void poll(uint64_t now);
  void interrupt();
  bool active() const { return held; }
  uint8_t targetId() const { return context.id; }
};
