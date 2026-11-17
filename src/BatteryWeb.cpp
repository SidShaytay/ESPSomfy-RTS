#include "BatteryWeb.h"
#include "BatteryStatus.h"
#include "outputs/ProtoSocket.h"
#ifndef ESPSOMFY_STATUS_DISPLAY
// Boards without the power bus have no gauge and no charger to limit.
namespace BatteryStatus {
View view() { return {}; }
bool setLimit(int8_t) { return false; }
}
#endif
namespace BatteryWeb {
void runProtocol(const loop_item_t &item) {
  if(item.device.value >= 0 && !BatteryStatus::setLimit(item.device.value)) {
    protoSocket.replyRefused(item.replyTo, proto_error_t::badRequest, "This controller cannot save a charge limit.");
    return;
  }
  const auto v = BatteryStatus::view();
  auto reply = protoSocket.beginReply(item.replyTo);
  reply.add(proto_field_t::batteryState, uint8_t(v.reading.state));
  if(v.reading.state == BatteryGauge::State::ready) {
    reply.add(proto_field_t::batteryPercent, v.reading.percent);
    reply.add(proto_field_t::batteryMillivolts, v.reading.millivolts);
    reply.add(proto_field_t::batteryMilliamps, v.reading.milliamps);
  }
  reply.add(proto_field_t::chargeLimitSupported, v.limitSupported);
  if(v.limitSupported) {
    reply.add(proto_field_t::chargeLimit, v.limit);
    reply.add(proto_field_t::chargeLimitApplied, v.limitState != BatteryCharger::State::failed);
  }
  protoSocket.endReply(item.replyTo);
}
}
