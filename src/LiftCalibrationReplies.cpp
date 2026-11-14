#include "LiftCalibrationService.h"
#include "LiftCalibrationProtocol.h"
#include "LoopQueue.h"
#include "outputs/ProtoSocket.h"
namespace LiftCal {
void runProtocol(const loop_item_t &item) {
  const auto &request = item.calibration;
  const auto source = webSource(item.replyTo.client);
  uint32_t token = request.token;
  bool accepted;
  if(request.action == LiftCalibrationRequest::begin)
    accepted = begin(source,request.id,request.kind,token);
  else if(request.action == LiftCalibrationRequest::poll) accepted = true;
  else accepted = act(source,token,static_cast<LiftGuideAction>(request.action),request.human);
  if(!accepted) {
    protoSocket.replyRefused(item.replyTo,proto_error_t::busy,
      "Calibration action refused: check the current step, source, shutter and radio state");
    return;
  }
  LiftGuideSnapshot state;
  if(!snapshot(source,token,state)) {
    protoSocket.replyRefused(item.replyTo,proto_error_t::badRequest,"Calibration session expired or changed");
    return;
  }
  const auto view = liftGuideView(state.stage,state.applied,state.durable,state.reversalsKnown);
  auto reply = protoSocket.beginReply(item.replyTo);
  reply.add(proto_field_t::calibrationSession,state.token);
  reply.add(proto_field_t::shadeId,state.id);
  reply.add(proto_field_t::calibrationStage,uint8_t(state.stage));
  reply.add(proto_field_t::calibrationTitle,view.title);
  reply.add(proto_field_t::calibrationHint,view.hint);
  reply.add(proto_field_t::calibrationOwned,state.owned);
  reply.add(proto_field_t::calibrationReversalsKnown,state.reversalsKnown);
  reply.add(proto_field_t::applied,state.applied);
  reply.add(proto_field_t::durable,state.durable);
  reply.add(proto_field_t::calibrationKind,uint8_t(state.calibration.kind));
  reply.add(proto_field_t::upTime,state.upTime);reply.add(proto_field_t::downTime,state.downTime);
  reply.add(proto_field_t::upTakeupMs,state.calibration.up.takeupMs);
  reply.add(proto_field_t::upMidpointMs,state.calibration.up.midpointMs);
  reply.add(proto_field_t::upSettlingMs,state.calibration.up.settlingMs);
  reply.add(proto_field_t::upReversalMs,state.calibration.up.reversalMs);
  reply.add(proto_field_t::downTakeupMs,state.calibration.down.takeupMs);
  reply.add(proto_field_t::downMidpointMs,state.calibration.down.midpointMs);
  reply.add(proto_field_t::downSettlingMs,state.calibration.down.settlingMs);
  reply.add(proto_field_t::downReversalMs,state.calibration.down.reversalMs);
  reply.beginList(proto_field_t::calibrationChoices);
  for(unsigned i = 0; i < view.count; ++i) {
    reply.beginItem();reply.add(proto_field_t::calibrationAction,uint8_t(view.choices[i].action));
    reply.add(proto_field_t::name,view.choices[i].label);reply.endItem();
  }
  reply.endList();
  protoSocket.endReply(item.replyTo);
}
}
