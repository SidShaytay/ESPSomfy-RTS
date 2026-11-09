#pragma once
#include "LocalEntityForm.h"
namespace LocalUi {
namespace Ordering {
// One loop-owned draft. The parent supplies its existing protected FormView;
// this component retains IDs/physical slots only, never another frame/model.
class Form {
  EntityAdmin::Backend &backend;
  ArduinoJson::Allocator &allocator;
  EntityAdmin::OrderIds ids{}, rollback{}, slots{};
  EntityAdmin::Authorization admitted;
  uint64_t revision = 0, activityAt = 0, observedAt = 0;
  enum class State : uint8_t {
    closed,
    browse,
    move,
    confirm,
    result
  } state = State::closed;
  EntityAdmin::Kind kind = EntityAdmin::Kind::room;
  EntityAdmin::Notice status = EntityAdmin::Notice::none;
  uint8_t count = 0, cursor = 0, start = 0;
  bool dirty = false, normalization = false;
  void close(EntityAdmin::Notice);
  bool guard(uint64_t);
  void touch(uint64_t);
  void exchange(uint8_t, uint8_t);
  bool capture();
  bool refreshDirty();
  void save();
  void refused(EntityAdmin::Admission);

public:
  static constexpr uint64_t timeoutMs = 120000;
  Form(EntityAdmin::Backend &source, ArduinoJson::Allocator &memory)
      : backend(source), allocator(memory) {}
  ~Form() { discard(); }
  Form(const Form &) = delete;
  Form &operator=(const Form &) = delete;
  bool open(EntityAdmin::Kind, uint64_t now);
  void tick(uint64_t now);
  void turn(int direction, uint64_t now);
  void activate(uint64_t now);
  void back(uint64_t now);
  void discard() { close(EntityAdmin::Notice::none); }
  bool closed() const { return state == State::closed; }
  EntityAdmin::Notice notice() const { return status; }
  uint64_t epoch() const { return revision; }
  void render(EntityAdmin::FormView &);
};
static_assert(sizeof(Form) <= 160,
              "Bounded order IDs, rollback and slot cache");
} // namespace Ordering
} // namespace LocalUi
