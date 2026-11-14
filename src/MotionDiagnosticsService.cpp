#include "MotionDiagnosticsService.h"
#include "LoopQueue.h"
#include "outputs/ProtoSocket.h"
#include <Arduino.h>
namespace MotionTrace {
namespace { MotionDiagnostics capture; bool usb = false; }
static void captureRecord(uint8_t category, uint8_t kind, uint8_t shade, uint32_t ticket, int32_t value, uint32_t now) {
  if(!capture.append(category,kind,shade,ticket,value,now) || !usb) return;
  Serial.printf("TRACE %lu %u %u %u %lu %ld\n",(unsigned long)now,category,kind,shade,
    (unsigned long)ticket,(long)value);
}
void runProtocol(const loop_item_t &item) {
  const auto &request = item.diagnostics;
  if(request.configure) {
    if(!capture.configure(request.mask,request.ttl,millis())) {
      protoSocket.replyRefused(item.replyTo,proto_error_t::badRequest,"Invalid trace mask or duration");return;
    }
    usb = request.usb && request.mask;
    sink = request.mask ? captureRecord : nullptr;
  }
  MotionDiagnosticRecord records[8];
  const auto result = capture.read(request.after,records,8);
  auto reply = protoSocket.beginReply(item.replyTo);
  reply.add(proto_field_t::traceEnabled,capture.enabled(millis()));
  reply.add(proto_field_t::traceOverflow,result.overflow);
  // Decimal cursor text avoids loss of uint64 precision in browser JSON.
  char cursor[21];snprintf(cursor,sizeof(cursor),"%llu",(unsigned long long)result.next);
  reply.add(proto_field_t::traceCursor,cursor);
  reply.beginList(proto_field_t::traceRecords);
  for(size_t i=0;i<result.count;++i) {
    const auto &record=records[i];reply.beginItem();
    reply.add(proto_field_t::traceAt,record.at);reply.add(proto_field_t::traceCategory,record.category);
    reply.add(proto_field_t::traceKind,record.kind);reply.add(proto_field_t::shadeId,record.shadeId);
    reply.add(proto_field_t::traceTicket,record.ticket);reply.add(proto_field_t::traceValue,record.value);
    reply.endItem();
  }
  reply.endList();protoSocket.endReply(item.replyTo);
}
}
