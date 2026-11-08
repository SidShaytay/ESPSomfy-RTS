#ifdef ESPSOMFY_LOCAL_UI
#include "LocalEntityRuntime.h"
#include "LocalOperationAdmission.h"
#include "LocalUiAuthorizationAdapter.h"
#include "LoopQueue.h"
#include "ModelCommitService.h"
#include "Somfy.h"
#include "SomfyEditor.h"
#include <string.h>
extern SomfyShadeController somfy;
extern LoopQueue loopQueue;
namespace LocalUi {
namespace EntityAdmin {
namespace {
class RuntimeBackend final : public Backend {
  Admission admit(Authorization expected, uint64_t revision) {
    const auto now = authorization();
    if (!expected.allowed || !now.allowed)
      return Admission::locked;
    if (!expected.generation || !expected.configurationGeneration ||
        !revision || expected.generation != now.generation ||
        expected.configurationGeneration != now.configurationGeneration ||
        revision != this->revision())
      return Admission::stale;
    if (loopQueue.pending())
      return Admission::busy;
    return Admission::accepted;
  }

public:
  Authorization authorization() override {
    const auto state = pollAuthorization();
    return {state.allowed && !localControlsFrozen(), state.generation,
            state.configurationGeneration};
  }
  uint64_t revision() override { return somfy.configurationRevision; }
  bool prepare(Kind kind, uint8_t requested, uint8_t &id) override {
    if (!authorization().allowed || kind != Kind::room)
      return false;
    if (requested) {
      if (!somfy.getRoomById(requested))
        return false;
      id = requested;
    } else
      id = somfy.getNextRoomId();
    return id && id != 255;
  }
  bool read(Kind kind, uint8_t id, proto_field_t field, Value &out) override {
    out = {};
    if (!authorization().allowed || kind != Kind::room)
      return false;
    out.kind = protoField(field).kind;
    auto *room = id ? somfy.getRoomById(id) : nullptr;
    if (id && !room)
      return false;
    if (field == proto_field_t::name) {
      if (room) {
        if (!memchr(room->name, 0, sizeof(room->name)))
          return false;
        memcpy(out.text, room->name, sizeof(out.text));
      }
    } else if (field == proto_field_t::sortOrder)
      out.integer = room ? room->sortOrder : somfy.getMaxRoomOrder() + 1;
    else
      return false;
    return true;
  }
  ApplyResult apply(model_edit_type_t type, JsonDocument &expanded,
                    Authorization expected, uint64_t revision) override {
    const auto admission = admit(expected, revision);
    if (admission != Admission::accepted)
      return {{}, admission};
    model_edit_t edit;
    edit.type = type;
    // ADL swap moves the allocator with its storage. No copy into an internal
    // heap-backed document occurs; edit destruction releases/wipes the draft.
    swap(edit.doc, expanded);
    return {applyModelEdit(edit)};
  }
  CommitResult retryFileCommit(Authorization expected,
                               uint64_t revision) override {
    const auto admission = admit(expected, revision);
    if (admission != Admission::accepted)
      return {{}, admission};
    return {applyModelCommit().file};
  }
  void activity() override { authorizationActivity(); }
};
} // namespace
Backend &runtimeBackend() {
  static RuntimeBackend backend;
  return backend;
}
} // namespace EntityAdmin
} // namespace LocalUi
#endif
