#include "ProtoModelResults.h"
void writeModelEditResult(ProtoWriter &w, model_edit_type_t type,
                          const edit_result_t &result) {
  w.add(proto_field_t::ok, result.ok);
  w.add(proto_field_t::applied, result.applied);
  w.add(proto_field_t::durable, result.durable());
  if (result.applied) {
    w.add(proto_field_t::modelSaveState,
          static_cast<int>(result.persistence.state));
    if (type == model_edit_type_t::add_room)
      w.add(proto_field_t::roomId, result.id);
    else if (type == model_edit_type_t::add_shade)
      w.add(proto_field_t::shadeId, result.id);
    else if (type == model_edit_type_t::add_group)
      w.add(proto_field_t::groupId, result.id);
  }
  if (!result.ok) {
    w.add(proto_field_t::code,
          static_cast<uint8_t>(result.applied    ? proto_error_t::internal
                               : result.notFound ? proto_error_t::notFound
                                                 : proto_error_t::badRequest));
    w.add(proto_field_t::desc, result.desc);
    w.add(proto_field_t::validation, result.code);
  }
}
void writeModelCommitReport(ProtoWriter &w, ModelCommitReport report) {
  const auto result = report.file;
  w.add(proto_field_t::ok, result.saved());
  w.add(proto_field_t::durable, result.saved());
  w.add(proto_field_t::modelSaveState, static_cast<int>(result.state));
  w.add(proto_field_t::rebooting, report.rebooting);
  if(report.refusal!=RestartReason::none) {
    if(!result.saved()) {
      w.add(proto_field_t::code,static_cast<uint8_t>(proto_error_t::busy));
      w.add(proto_field_t::desc,"Upload recovery expired or does not own storage; no file save or restart was performed.");
    } else w.add(proto_field_t::desc,restartReason(report.refusal));
  } else if (report.restartDeferred)
    w.add(proto_field_t::desc,
          "Model file saved; restart not scheduled while operations remain "
          "queued. Resolve queued operations, then Restart.");
  else if (!result.saved()) {
    w.add(proto_field_t::code, static_cast<uint8_t>(proto_error_t::internal));
    w.add(proto_field_t::desc, modelCommitFailureDescription(result.state));
  }
}
const char *parseModelCommitPayload(JsonObjectConst payload) {
  return payload.size() ? "Commit accepts no edit, target or command fields"
                        : nullptr;
}
