#include "LiftCalibrationGate.h"
#include "Somfy.h"
#include "SomfyEditor.h"
#include "esp_log.h"
#include "outputs/ProtoSocket.h"
extern SomfyShadeController somfy;
// Internal mutation/serialization implementation; public callers use the
// complete applyModelEdit seam below, including actual-model fanout.
edit_result_t mutateModelEdit(model_edit_t &edit);
static const char *TAG = "ModelEdit";

static void markEdited(const model_edit_t &edit, const edit_result_t &result) {
  const edit_changes_t c = editChanges(edit.type, result);
  SomfyShade *shade = c.shadeId ? somfy.getShadeById(c.shadeId) : nullptr;
  if (shade)
    shade->configChanged();
  SomfyGroup *group = c.groupId ? somfy.getGroupById(c.groupId) : nullptr;
  if (group)
    group->configChanged();
  SomfyRoom *room = c.roomId ? somfy.getRoomById(c.roomId) : nullptr;
  if (room)
    protoSocket.room(*room);
  if (c.order != sort_list_t::none)
    protoSocket.order(c.order);
  if (c.repeaters)
    protoSocket.repeaters();
  if (c.removedShadeId)
    protoSocket.shadeRemoved(c.removedShadeId);
  if (c.removedGroupId)
    protoSocket.groupRemoved(c.removedGroupId);
  if (c.removedRoomId)
    protoSocket.roomRemoved(c.removedRoomId);
}
edit_result_t applyModelEdit(model_edit_t &edit) {
  edit_result_t result;
  if(LiftCalibrationGate::active.load() && !LiftCalibrationGate::modelSaving) result.desc="Calibration owns the controller; close its guide before editing";
  else if(!somfy.modelLoad.usable())result.desc="Model load failed; restore a complete model before editing";
  else result=mutateModelEdit(edit);
  if (!result.ok)
    ESP_LOGW(TAG, "Edit %u %s: %s", static_cast<uint8_t>(edit.type),
             result.applied ? "applied but not saved" : "refused", result.desc);
  // Reply first, then publish actual changes even when persistence failed.
  // A local caller leaves replyTo empty; the socket sends nothing for it.
  protoSocket.replyEdit(edit.replyTo, edit.type, result);
  if (result.applied) {
    markEdited(edit, result);
    protoSocket.modelPersistence({result.persistence}, edit.replyTo.client);
  }
  return result;
}
void runModelEdit(model_edit_t *edit) {
  applyModelEdit(*edit);
  cancelModelEdit(edit);
}
