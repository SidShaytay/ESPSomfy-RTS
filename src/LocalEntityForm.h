#pragma once
#include "EntityFields.h"
#include "LocalEntityView.h"
#include "LocalValueEditor.h"
#include "ProtoEdits.h"
namespace LocalUi {
namespace EntityAdmin {
struct Authorization {
  bool allowed = false;
  uint32_t generation = 0, configurationGeneration = 0;
};
struct Value {
  char kind = 0;
  int64_t integer = 0;
  bool boolean = false;
  char text[21] = {};
};
enum class Admission : uint8_t { accepted, locked, stale, busy };
struct ApplyResult {
  edit_result_t edit;
  Admission admission = Admission::accepted;
};
struct CommitResult {
  ModelCommitResult file;
  Admission admission = Admission::accepted;
};
// All methods execute synchronously on loop(). id 0 reads offered defaults.
// Reads must return the original value, including unknown legacy values.
class Backend {
public:
  virtual ~Backend() = default;
  virtual Authorization authorization() = 0;
  virtual uint64_t revision() = 0;
  virtual bool prepare(Kind kind, uint8_t id, uint8_t &offeredId) = 0;
  virtual bool read(Kind kind, uint8_t id, proto_field_t field, Value &out) = 0;
  // Final auth/history checks follow all allocation/validation work. The
  // expanded request may be consumed with allocator-preserving ADL swap.
  virtual ApplyResult apply(model_edit_type_t, JsonDocument &expanded,
                            Authorization, uint64_t) = 0;
  virtual CommitResult retryFileCommit(Authorization, uint64_t) = 0;
  virtual void activity() = 0;
};
class Form {
  Backend &backend;
  ArduinoJson::Allocator &allocator;
  JsonDocument changes;
  ValueEditor editor;
  FormView frame;
  Authorization admitted;
  uint64_t admittedRevision = 0, activityAt = 0, observedAt = 0;
  Kind kind = Kind::room;
  uint8_t sourceId = 0, id = 0, selected = 0, editing = 0, stepPower = 0;
  bool deleting = false;
  model_edit_type_t type(bool remove = false) const;
  const edit_form_t *metadata() const;
  bool guard(uint64_t now);
  bool read(const entity_field_t &, Value &);
  void render();
  void close(Notice);
  void touch(uint64_t now);
  void beginField(uint64_t now);
  void applyField(uint64_t now);
  void save(uint64_t now);

public:
  static constexpr uint64_t timeoutMs = 120000;
  Form(Backend &, ArduinoJson::Allocator &);
  ~Form();
  Form(const Form &) = delete;
  Form &operator=(const Form &) = delete;
  bool open(Kind, uint8_t id, uint64_t now);
  void tick(uint64_t now);
  void turn(int direction, uint64_t now);
  void activate(uint64_t now);
  void back(uint64_t now);
  void discard() { close(Notice::none); }
  const FormView &view() const { return frame; }
  bool closed() const { return frame.mode == Mode::closed; }
};
static_assert(sizeof(Form) <= 1024, "Bounded local entity form state");
} // namespace EntityAdmin
} // namespace LocalUi
