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
                               : result.failure==edit_failure_t::busy ? proto_error_t::busy
                               : result.failure==edit_failure_t::internal ? proto_error_t::internal
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
      const char *why=report.refusal==RestartReason::counterUncertain
          ? "Counter storage is uncertain. Commit cannot recover an unknown counter; no new writes were performed."
          : report.refusal==RestartReason::persistenceBusy
          ? "Storage or update work is active; no file recovery performed. Retry Commit later."
          : report.refusal==RestartReason::invalidRequest
          ? "Invalid commit mode/source; no file save performed."
          : "Upload recovery expired or does not own storage; no file save or restart was performed.";
      w.add(proto_field_t::desc,why);
    } else w.add(proto_field_t::desc,restartReason(report.refusal));
  } else if (report.restartDeferred)
    w.add(proto_field_t::desc,
          "Model file saved; restart not scheduled while operations remain "
          "queued. Resolve queued operations, then Restart.");
  else if (!result.saved()) {
    w.add(proto_field_t::code, static_cast<uint8_t>(result.stage==ModelCommitStage::filesystemBusy ? proto_error_t::busy : proto_error_t::internal));
    w.add(proto_field_t::desc, modelCommitFailureDescription(result));
  }
}
const char *parseModelCommitPayload(JsonObjectConst payload) {
  return payload.size() ? "Commit accepts no edit, target or command fields"
                        : nullptr;
}
