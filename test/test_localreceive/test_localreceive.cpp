#include "LocalReceive.h"
#include "PulseDecoder.h"
#include "SomfyFrame.h"
#include <string.h>
#include <unity.h>
using namespace LocalUi;
struct Sink : ReceiveSink {
  ReceiveObservation last;
  unsigned calls = 0;
  uint16_t pulseCount = 0;
  unsigned int firstPulse = 0;
  bool clipped = false;
  void received(const ReceiveObservation &observation,
                ReceivePulses pulses) override {
    ++calls;
    last = observation;
    pulseCount = pulses.count;
    clipped = pulses.clipped;
    if (pulses.count)
      firstPulse = pulses.values[0];
  }
};
void setUp() {}
void tearDown() {}
static void named_subscriptions_never_overwrite_and_tokens_reject_aba() {
  ReceiveHub hub;
  Sink learning, diagnostics, other;
  ReceiveSubscription first, second, denied;
  TEST_ASSERT_TRUE(hub.subscribe(ReceiveChannel::learning, learning, first));
  TEST_ASSERT_FALSE(hub.subscribe(ReceiveChannel::learning, other, denied));
  TEST_ASSERT_TRUE(
      hub.subscribe(ReceiveChannel::diagnostics, diagnostics, second));
  TEST_ASSERT_FALSE(hub.unsubscribe(first, other));
  auto wrong = first;
  wrong.channel = ReceiveChannel::diagnostics;
  TEST_ASSERT_FALSE(hub.unsubscribe(wrong, learning));
  TEST_ASSERT_TRUE(hub.unsubscribe(first, learning));
  ReceiveSubscription next;
  TEST_ASSERT_TRUE(hub.subscribe(ReceiveChannel::learning, learning, next));
  TEST_ASSERT_NOT_EQUAL(first.generation, next.generation);
  TEST_ASSERT_FALSE(hub.unsubscribe(first, learning));
  TEST_ASSERT_FALSE(hub.observing(first, learning));
  TEST_ASSERT_TRUE(hub.observing(next, learning));
  TEST_ASSERT_TRUE(hub.unsubscribe(next, learning));
  TEST_ASSERT_TRUE(hub.unsubscribe(second, diagnostics));
  TEST_ASSERT_FALSE(hub.active());
}
static void publish_immutable_bounded_observation_and_pulses() {
  ReceiveHub hub;
  Sink sink;
  ReceiveSubscription token;
  hub.subscribe(ReceiveChannel::diagnostics, sink, token);
  ReceiveObservation observation;
  observation.observedAt = 99;
  observation.address = 0xabc;
  observation.flags = ReceiveObservation::validFlag;
  unsigned int values[MAX_TIMINGS]{};
  values[0] = 640;
  TEST_ASSERT_TRUE(
      hub.publish(observation, {values, uint16_t(MAX_TIMINGS + 10), false}));
  TEST_ASSERT_EQUAL(0, observation.sequence);
  TEST_ASSERT_EQUAL(1, sink.last.sequence);
  TEST_ASSERT_EQUAL(99, sink.last.observedAt);
  TEST_ASSERT_EQUAL(MAX_TIMINGS, sink.pulseCount);
  TEST_ASSERT_TRUE(sink.clipped);
  TEST_ASSERT_EQUAL(640, sink.firstPulse);
  TEST_ASSERT_TRUE(hub.publish(observation, {nullptr, 10, false}));
  TEST_ASSERT_EQUAL(2, sink.last.sequence);
  TEST_ASSERT_EQUAL(0, sink.pulseCount);
  TEST_ASSERT_TRUE(sink.clipped);
  TEST_ASSERT_TRUE(hub.unsubscribe(token, sink));
}
static void decoded_adapter_is_subscription_independent_receive_only() {
  ReceiveHub hub;
  Sink learning, diagnostics;
  ReceiveSubscription a, b;
  hub.subscribe(ReceiveChannel::learning, learning, a);
  hub.subscribe(ReceiveChannel::diagnostics, diagnostics, b);
  somfy_frame_t frame{};
  frame.valid = true;
  frame.remoteAddress = 123;
  frame.rollingCode = 456;
  frame.proto = radio_proto::RTW;
  frame.cmd = somfy_commands::Up;
  frame.rawCmd = 2;
  frame.encKey = 0xa7;
  frame.hwsync = 4;
  frame.rssi = -72;
  somfy_rx_t raw{};
  raw.bit_length = 80;
  raw.pulseCount = 1;
  raw.pulses[0] = 1280;
  TEST_ASSERT_TRUE(publishDecodedReceive(hub, frame, raw, 1000, true));
  TEST_ASSERT_EQUAL(1, learning.calls);
  TEST_ASSERT_EQUAL(1, diagnostics.calls);
  TEST_ASSERT_TRUE(learning.last.scanning());
  TEST_ASSERT_TRUE(learning.last.valid());
  TEST_ASSERT_EQUAL(80, learning.last.bits);
  TEST_ASSERT_EQUAL(456, learning.last.rollingCode);
  TEST_ASSERT_EQUAL(123, learning.last.address);
  TEST_ASSERT_EQUAL(-72, learning.last.rssi);
  TEST_ASSERT_EQUAL(1280, diagnostics.firstPulse);
  frame.cmd = somfy_commands::StepUp;
  frame.stepSize = 9;
  frame.valid = false;
  TEST_ASSERT_TRUE(publishDecodedReceive(hub, frame, raw, 1001, false));
  TEST_ASSERT_FALSE(learning.last.valid());
  TEST_ASSERT_EQUAL(9, learning.last.stepSize);
  TEST_ASSERT_FALSE(learning.last.scanning());
  hub.unsubscribe(a, learning);
  hub.unsubscribe(b, diagnostics);
}
static void callback_changes_start_next_event_and_reentrancy_refuses() {
  ReceiveHub hub;
  Sink oldSink, newSink;
  ReceiveSubscription oldToken, newToken, first;
  struct Changing : ReceiveSink {
    ReceiveHub &hub;
    Sink &oldSink, &newSink;
    ReceiveSubscription &oldToken, &newToken;
    unsigned calls = 0;
    Changing(ReceiveHub &h, Sink &o, Sink &n, ReceiveSubscription &a,
             ReceiveSubscription &b)
        : hub(h), oldSink(o), newSink(n), oldToken(a), newToken(b) {}
    void received(const ReceiveObservation &observation,
                  ReceivePulses) override {
      ++calls;
      TEST_ASSERT_FALSE(hub.publish(observation));
      if (calls == 1) {
        TEST_ASSERT_TRUE(hub.unsubscribe(oldToken, oldSink));
        TEST_ASSERT_TRUE(
            hub.subscribe(ReceiveChannel::diagnostics, newSink, newToken));
      }
    }
  } changing(hub, oldSink, newSink, oldToken, newToken);
  hub.subscribe(ReceiveChannel::learning, changing, first);
  hub.subscribe(ReceiveChannel::diagnostics, oldSink, oldToken);
  ReceiveObservation observation;
  TEST_ASSERT_TRUE(hub.publish(observation));
  TEST_ASSERT_EQUAL(0, oldSink.calls);
  TEST_ASSERT_EQUAL(0, newSink.calls);
  TEST_ASSERT_TRUE(hub.publish(observation));
  TEST_ASSERT_EQUAL(1, newSink.calls);
  TEST_ASSERT_EQUAL(2, newSink.last.sequence);
  hub.unsubscribe(first, changing);
  hub.unsubscribe(newToken, newSink);
}
int main() {
  UNITY_BEGIN();
  RUN_TEST(named_subscriptions_never_overwrite_and_tokens_reject_aba);
  RUN_TEST(publish_immutable_bounded_observation_and_pulses);
  RUN_TEST(decoded_adapter_is_subscription_independent_receive_only);
  RUN_TEST(callback_changes_start_next_event_and_reentrancy_refuses);
  return UNITY_END();
}
