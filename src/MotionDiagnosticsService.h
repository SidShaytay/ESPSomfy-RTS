#pragma once
#include "MotionDiagnostics.h"
struct MotionDiagnosticsRequest {
  uint64_t after = 0;
  uint32_t ttl = 0;
  uint8_t mask = 0;
  bool configure = false, usb = false;
};
struct loop_item_t;
namespace MotionTrace {
  inline void (*sink)(uint8_t,uint8_t,uint8_t,uint32_t,int32_t,uint32_t) = nullptr;
  inline void record(uint8_t category, uint8_t kind, uint8_t shade, uint32_t ticket, int32_t value, uint32_t now) {
    if(sink) sink(category,kind,shade,ticket,value,now);
  }
  void poll();
  void runProtocol(const loop_item_t &);
}
