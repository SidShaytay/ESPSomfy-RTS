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
struct Choice {
  int64_t value = 0;
  char label[32] = {};
};
enum class Admission : uint8_t { accepted, locked, stale, busy, invalid, noMemory };
struct ApplyResult {
  edit_result_t edit;
  Admission admission = Admission::accepted;
  proto_field_t field = proto_field_t::count;
  // Owned canonical explanation for a correctable refusal; never a stack
  // pointer, user-supplied text, path, identity or credential value.
  char reason[64] = {};
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
  // Bounded choice reads share the prepared target/history. Values are stable
  // domain IDs, never list indices. A missing advertised choice invalidates
  // the form; labels must be terminated. Zero means a numeric/text field.
  virtual uint8_t choiceCount(Kind, uint8_t sourceId, proto_field_t);
  virtual bool choiceAt(Kind, uint8_t sourceId, proto_field_t, uint8_t, Choice &);
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
  bool deleting = false, validationField = false;
  char validationReason[64] = {};
  model_edit_type_t type(bool remove = false) const;
  const edit_form_t *metadata() const;
  bool guard(uint64_t now);
  bool read(const entity_field_t &, Value &);
  void render();
  bool validOriginal(const entity_field_t &, const Value &);
  bool choice(const entity_field_t &, uint8_t, Choice &);
  void formatValue(const entity_field_t &, const Value &, char *, size_t);
  void close(Notice);
  void touch(uint64_t now);
  enum class FieldStart : uint8_t { selected, existing, replaceInvalid };
  void beginField(uint64_t now, FieldStart start = FieldStart::selected);
  bool fieldActive(const entity_field_t &);
  void refuse(proto_field_t field, const char *reason);
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
