#include "ConfigFile.h"
#include "SafeRestart.h"
#include "Somfy.h"
#include "TaskWdt.h"
#include "esp_log.h"
static const char *TAG = "ModelCommit";
extern SomfyShadeController somfy;

ModelCommitResult SomfyShadeController::commit() {
  resetTaskWdt();
  this->lastModelCommit = this->modelLoad.mayCommit() ? ShadeConfigFile::commitModel(this) : ModelCommitResult{ModelCommitState::failed,ModelCommitStage::untrusted};
  this->modelLoad.committed(this->lastModelCommit.saved());
  this->isDirty = !this->lastModelCommit.saved();
  noteModelCommit(this->lastModelCommit);
  // Throttle failed attempts as well as successful writes; never hot-loop IO.
  this->lastCommit = millis();
  if (!this->lastModelCommit.saved())
    ESP_LOGE(TAG, "Model commit failed at stage %u%s",
             unsigned(this->lastModelCommit.stage),
             this->lastModelCommit.state == ModelCommitState::uncertain
                 ? "; canonical file uncertain"
                 : "; previous file retained");
  return this->lastModelCommit;
}

ModelCommitResult SomfyRoom::save() { return somfy.commit(); }
ModelCommitResult SomfyGroup::save() { return somfy.commit(); }
ModelCommitResult SomfyShade::commit() { return somfy.commit(); }
ModelCommitResult SomfyShade::save() {
  // Normalize this applied edit once, not every serialization/COMMIT retry.
  if (this->tiltType == tilt_types::none ||
      this->shadeType != shade_types::blind) {
    this->clearTilt();
    this->tiltType = tilt_types::none;
  }
  const auto result = this->commit();
  this->publish();
  return result;
}
