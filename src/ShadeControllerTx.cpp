#include "Somfy.h"

// Radio submission and model fan-out. Kept separate from controller boot and
// filesystem persistence so host integration tests run this exact TX path.

void SomfyShadeController::processFrame(somfy_frame_t &frame, bool internal, const SomfyRemote *skip) {
  for(uint8_t i = 0; i < SOMFY_MAX_SHADES; i++) {
    if(&this->shades[i] == skip) continue;
    if(this->shades[i].getShadeId() != 255) this->shades[i].processFrame(frame, internal);
  }
}

void SomfyShadeController::markTxPending(uint32_t remoteAddress, uint32_t ticket, const SomfyRemote *skip) {
  for(uint8_t i = 0; i < SOMFY_MAX_SHADES; i++) {
    SomfyShade *shade = &this->shades[i];
    if(shade == skip) continue;
    if(shade->getShadeId() != 255 && shade->findRemote(remoteAddress)) shade->markTxPending(ticket);
  }
  // Group commands move their member shades through processInternalCommand.
  for(uint8_t i = 0; i < SOMFY_MAX_GROUPS; i++) {
    SomfyGroup *group = &this->groups[i];
    if(group->getGroupId() == 255 || group->getRemoteAddress() != remoteAddress) continue;
    for(uint8_t j = 0; j < SOMFY_MAX_GROUPED_SHADES; j++) {
      if(group->linkedShades[j] == 0) continue;
      SomfyShade *shade = this->getShadeById(group->linkedShades[j]);
      if(shade) shade->markTxPending(ticket);
    }
  }
}

void SomfyShadeController::cancelGroupStartsForShade(uint8_t shadeId, uint32_t exceptTicket) {
  for(uint8_t i = 0; i < SOMFY_MAX_GROUPS; i++) {
    SomfyGroup &group = this->groups[i];
    if(group.getGroupId() != 255 && group.hasShadeId(shadeId))
      this->transceiver.cancelTx(group.getRemoteAddress(), exceptTicket);
  }
}

void SomfyShadeController::cancelGroupReplansForShade(uint8_t shadeId) {
  for(uint8_t i = 0; i < SOMFY_MAX_GROUPS; i++) {
    SomfyGroup &group = this->groups[i];
    if(group.getGroupId() != 255 && group.hasShadeId(shadeId)) group.cancelReplan();
  }
}

void SomfyShadeController::processTxEvent(const somfy_tx_event_t &evt) {
  for(uint8_t i = 0; i < SOMFY_MAX_SHADES; i++)
    if(this->shades[i].getShadeId() != 255) this->shades[i].processTxEvent(evt);
  // A waiting group target is planned as soon as the outcome it waited for
  // arrives, not a loop later.
  for(uint8_t i = 0; i < SOMFY_MAX_GROUPS; i++)
    if(this->groups[i].getGroupId() != 255) this->groups[i].serviceTarget();
}

uint32_t SomfyShadeController::sendFrame(somfy_frame_t &frame, uint8_t repeat, uint16_t delayMs, bool priority) {
  somfy_tx_job_t job;
  encodeTxJob(job, frame, repeat);
  job.firstRepeat = 1;
  return this->transceiver.submitTx(job, priority, delayMs);
}
