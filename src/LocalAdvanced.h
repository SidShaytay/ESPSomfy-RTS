#pragma once
#include "LocalEntityForm.h"
#include "LocalLearnCandidates.h"
#include "LocalLearnQueue.h"
#include "LocalTargetRead.h"
#include "ShadeRequests.h"
namespace LocalUi {
enum class PrimaryCounterEffect : uint8_t;
namespace Advanced {
enum class Scope : uint8_t { devices, groups, repeaters };
struct Target {
  Scope scope = Scope::devices;
  uint8_t id = 0;
};
// Repeater membership is not an EntityRef. Retain its complete physical list
// separately; all comparisons are typed, never padding/hash comparisons.
struct Context {
  RawTargetContext raw;
  uint64_t history = 0;
  uint32_t repeaters[7]{};
  Scope scope = Scope::devices;
  bool same(const Context &other) const {
    if (!history || history != other.history || scope != other.scope)
      return false;
    if (scope != Scope::repeaters)
      return raw.same(other.raw);
    for (unsigned i = 0; i < 7; i++)
      if (repeaters[i] != other.repeaters[i])
        return false;
    return true;
  }
};
struct Item {
  uint32_t key = 0;
  uint8_t slot = 0;
  bool missing = false;
  uint16_t rollingCode = 0;
  char label[32]{};
};
enum class ActionKind : uint8_t {
  setPaired,
  unlinkRemote,
  unlinkRepeater,
  removeMissingMember
};
struct Action {
  ActionKind kind = ActionKind::setPaired;
  uint32_t key = 0;
  uint8_t slot = 0;
  bool paired = false;
};
// Metadata Actions change configuration only. Sensor transmission has its own
// typed, separately confirmed request and fresh queue/radio/owner/idle checks.
struct LearnApplyResult {
  edit_result_t edit;
  EntityAdmin::Admission admission = EntityAdmin::Admission::accepted;
  LearnQueueReason freshness = LearnQueueReason::ready;
};
struct LearnFileResult {
  ModelCommitReport report;
  EntityAdmin::Admission admission = EntityAdmin::Admission::accepted;
  LearnQueueReason freshness = LearnQueueReason::ready;
};
struct PrimaryReadResult {
  CounterDomainState state = CounterDomainState::invalid;
  EntityAdmin::Admission admission = EntityAdmin::Admission::accepted;
};
struct PrimaryApplyResult {
  CounterDomainResult counter;
  EntityAdmin::Admission admission = EntityAdmin::Admission::accepted;
  LearnQueueReason freshness = LearnQueueReason::ready;
};
struct SensorSimulation {
  int8_t windy=-1,sunny=-1; // Keep, Off, On; both Keep cannot send.
  uint8_t repeats=1;
};
inline bool prepareSensorSimulation(radio_proto protocol,uint8_t bits,bool group,int id,
                                    const SensorSimulation &draft,shade_request_t &out){
  if((protocol!=radio_proto::RTS&&protocol!=radio_proto::RTW&&protocol!=radio_proto::RTV)||
     (bits!=56&&bits!=80)||id<1||id>254||draft.repeats<1||draft.repeats>127||
     draft.windy< -1||draft.windy>1||draft.sunny< -1||draft.sunny>1||
     (draft.windy==-1&&draft.sunny==-1))return false;
  shade_request_t request;request.type=group?shade_request_type_t::group_sensor:shade_request_type_t::shade_sensor;
  request.id=uint8_t(id);request.command=somfy_commands::Sensor;request.windy=draft.windy;request.sunny=draft.sunny;
  request.repeat=int8_t(draft.repeats);out=request;return true;
}
struct SensorResult {
  shade_request_result_t result=shade_request_result_t::radio_refused;
  EntityAdmin::Admission admission=EntityAdmin::Admission::accepted;
};
class Backend {
public:
  virtual ~Backend() = default;
  virtual bool supportsSensor(Target){return false;}
  virtual EntityAdmin::Admission checkSensorContext(Target,const Context &,EntityAdmin::Authorization,uint64_t){return EntityAdmin::Admission::stale;}
  virtual SensorResult sendSensor(Target,const Context &,EntityAdmin::Authorization,uint64_t,
                                   const SensorSimulation &,LearnPending &){return {shade_request_result_t::radio_refused,EntityAdmin::Admission::invalid};}
  virtual PrimaryReadResult readPrimary(Target, const Context &,
      EntityAdmin::Authorization, uint64_t, PrimaryCounterReview &) = 0;
  // Live aliases/quarantine only; no NVS/owner/allocation. Safe in a final
  // read-only grant witness while the domain owns the counter reservation.
  virtual bool primaryCurrent(Target, const PrimaryCounterReview &) = 0;
  virtual PrimaryApplyResult correctPrimary(Target, const Context &,
      EntityAdmin::Authorization, uint64_t, const PrimaryCounterReview &,
      uint16_t, LearnPending &) = 0;
  virtual ReceiveHub &receivedFrames() = 0;
  virtual LearnQueued queueLearn(LearnPending &, uint64_t) = 0;
  virtual bool cancelLearn(uint64_t, LearnPending &) = 0;
  // Read-only auth/history/raw-context check; own FIFO marker still exists.
  virtual EntityAdmin::Admission checkLearnContext(Target, const Context &,
                                                   EntityAdmin::Authorization,
                                                   uint64_t) = 0;
  virtual LearnApplyResult linkLearn(Target, const Context &,
                                     EntityAdmin::Authorization, uint64_t,
                                     const LearnSelection &,
                                     LearnPending &witness,
                                     ArduinoJson::Allocator &) = 0;
  virtual LearnFileResult retryLearnFile(Target, const Context &,
                                         EntityAdmin::Authorization, uint64_t,
                                         LearnPending &witness) = 0;

  virtual EntityAdmin::Authorization authorization() = 0;
  virtual uint64_t revision() = 0;
  virtual void activity() = 0;
  virtual bool targets(Scope, EntityAdmin::OrderIds &, EntityAdmin::OrderIds &,
                       uint8_t &) = 0;
  virtual bool targetLabel(Target, uint8_t physicalSlot,
                           EntityAdmin::Choice &) = 0;
  virtual bool capture(Target, Context &) = 0;
  virtual bool paired(Target, bool &) = 0;
  virtual uint8_t itemCount(Target) = 0;
  virtual bool itemAt(Target, uint8_t, Item &) = 0;
  virtual EntityAdmin::ApplyResult apply(Target, const Action &,
                                         const Context &,
                                         EntityAdmin::Authorization, uint64_t,
                                         ArduinoJson::Allocator &) = 0;
  virtual EntityAdmin::CommitResult
  retryFile(Target, const Context &, EntityAdmin::Authorization, uint64_t) = 0;
};
class Form {
  struct Session;
  Backend &backend;
  ArduinoJson::Allocator &allocator;
  Session *session = nullptr;
  EntityAdmin::Notice status = EntityAdmin::Notice::none;
  bool privateProjectionDiscarded = false;
  void close(EntityAdmin::Notice);
  void endLearning();
  bool guard(uint64_t);
  void touch(uint64_t);
  bool select();
  void apply();
  void refused(EntityAdmin::Admission, const char *reason = nullptr);
  LearnQueueReason learnCurrent(uint64_t);
  void runLearn(uint64_t);
  void learnCancelled(LearnQueueReason);
  void queueLearn(uint64_t, bool fileOnly);
  void learnFault(const char *, bool chooseFrame = false);
  void primaryEffect(PrimaryCounterEffect effect, uint64_t now);
  void runPrimary();

public:
  static constexpr uint64_t timeoutMs = 120000;
  Form(Backend &b, ArduinoJson::Allocator &a) : backend(b), allocator(a) {}
  ~Form() { discard(); }
  Form(const Form &) = delete;
  Form &operator=(const Form &) = delete;
  bool open(Scope, uint64_t now);
  void tick(uint64_t now) { guard(now); }
  void turn(int direction, uint64_t now);
  void activate(uint64_t now);
  void back(uint64_t now);
  void discard() { close(EntityAdmin::Notice::none); }
  bool closed() const { return !session; }
  bool takePrivateProjectionDiscard() {
    const bool value = privateProjectionDiscarded;
    privateProjectionDiscarded = false;
    return value;
  }
  EntityAdmin::Notice notice() const { return status; }
  uint64_t epoch() const;
  void render(EntityAdmin::FormView &);
};
static_assert(sizeof(Form) <= 32,
              "Advanced retains a lazy handle, not a frame");
} // namespace Advanced
} // namespace LocalUi
