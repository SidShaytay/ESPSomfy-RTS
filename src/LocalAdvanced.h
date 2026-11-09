#pragma once
#include "LocalEntityForm.h"
#include "LocalTargetRead.h"
namespace LocalUi {
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
// Stage1 has no RF method. All mutation is configuration only. Runtime checks
// fresh authority/history/raw context and queue/restart/owner/idle admission.
class Backend {
public:
  virtual ~Backend() = default;
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
  void close(EntityAdmin::Notice);
  bool guard(uint64_t);
  void touch(uint64_t);
  bool select();
  void apply();
  void refused(EntityAdmin::Admission);

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
  EntityAdmin::Notice notice() const { return status; }
  uint64_t epoch() const;
  void render(EntityAdmin::FormView &);
};
static_assert(sizeof(Form) <= 32,
              "Advanced retains a lazy handle, not a frame");
} // namespace Advanced
} // namespace LocalUi
