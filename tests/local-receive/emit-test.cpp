#include "LocalReceive.h"
#include "ProtoSync.h"
#include "PulseDecoder.h"
#include "SomfyFrame.h"
#include <ArduinoJson.h>
#include <assert.h>
#include <stdio.h>
using namespace LocalUi;
uint8_t rxmode = 0;
int64_t esp_timer_get_time() { return 1000000; }
struct Transceiver {
  void emitFrame(somfy_frame_t *, somfy_rx_t *);
};
struct Socket {
  bool enabled = false;
  unsigned events = 0;
  char buffer[PROTO_MAX_FRAME];
  BufferedJsonFormatter json;
  bool subscribed(proto_feed_t feed) {
    assert(feed == proto_feed_t::frames);
    return enabled;
  }
  ProtoWriter beginFeed(proto_type_t type) {
    assert(type == proto_type_t::frame);
    json.setBuffer(buffer, sizeof(buffer));
    json.beginObject();
    return ProtoWriter(json);
  }
  void endFeed(proto_feed_t feed) {
    assert(feed == proto_feed_t::frames);
    json.endObject();
    ++events;
  }
} protoSocket;
#include "production-emit.inc"
struct Sink : ReceiveSink {
  unsigned calls = 0;
  ReceiveObservation last;
  uint16_t pulseCount = 0;
  void received(const ReceiveObservation &value,
                ReceivePulses pulses) override {
    ++calls;
    last = value;
    pulseCount = pulses.count;
    if (pulses.count)
      assert(pulses.values[0] == 640);
  }
};
int main() {
  auto &hub = receiveHub();
  Sink learning, diagnostics;
  ReceiveSubscription a, b;
  assert(hub.subscribe(ReceiveChannel::learning, learning, a));
  assert(hub.subscribe(ReceiveChannel::diagnostics, diagnostics, b));
  Transceiver radio;
  somfy_frame_t frame{};
  frame.valid = true;
  frame.remoteAddress = 0xabc;
  frame.cmd = somfy_commands::Up;
  frame.rollingCode = 42;
  frame.rssi = -71;
  somfy_rx_t raw{};
  raw.bit_length = 56;
  raw.pulseCount = 1;
  raw.pulses[0] = 640;
  radio.emitFrame(&frame, &raw);
  assert(learning.calls == 1 && diagnostics.calls == 1 && !protoSocket.events);
  assert(learning.last.address == 0xabc && learning.last.rollingCode == 42 &&
         learning.last.observedAt == 1000 && learning.pulseCount == 1);
  protoSocket.enabled = true;
  radio.emitFrame(&frame, &raw);
  assert(protoSocket.events == 1 && learning.calls == 2 &&
         diagnostics.calls == 2);
  JsonDocument event;
  assert(deserializeJson(event, protoSocket.buffer) ==
         DeserializationError::Ok);
  assert(event[protoField(proto_field_t::remoteAddress).key].as<uint32_t>() ==
         0xabc);
  assert(event[protoField(proto_field_t::lastRollingCode).key].as<uint16_t>() ==
         42);
  protoSocket.enabled = false;
  radio.emitFrame(&frame, nullptr);
  assert(learning.calls == 2 && diagnostics.calls == 2);
  frame.valid = false;
  rxmode = 3;
  radio.emitFrame(&frame, &raw);
  assert(learning.calls == 3 && diagnostics.calls == 3 &&
         !learning.last.valid() && learning.last.scanning());
  assert(hub.unsubscribe(a, learning));
  assert(hub.unsubscribe(b, diagnostics));
  radio.emitFrame(&frame, &raw);
  assert(learning.calls == 3 && diagnostics.calls == 3);
  puts("Actual RX emit hook: independent web feed, exact metadata, null-RX "
       "exclusion, scan/invalid flags and detachment passed (no RF endpoint)");
}
