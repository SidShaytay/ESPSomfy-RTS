#pragma once
#include <stdint.h>
struct somfy_frame_t;
struct somfy_rx_t;
namespace LocalUi {
// Decoded real RX metadata only. Sequence belongs to this boot's loop-owned
// hub; it is not a counter, proof of RF identity or permission to link/send.
struct ReceiveObservation {
  uint64_t observedAt = 0, sequence = 0;
  uint32_t address = 0;
  uint16_t rollingCode = 0;
  int16_t rssi = 0;
  uint8_t bits = 0, protocol = 0, command = 0, rawCommand = 0, encodingKey = 0,
          hardwareSync = 0;
  uint8_t stepSize = 0, flags = 0;
  static constexpr uint8_t validFlag = 1, scanningFlag = 2;
  bool valid() const { return (flags & validFlag) != 0; }
  bool scanning() const { return (flags & scanningFlag) != 0; }
};
static_assert(sizeof(ReceiveObservation) <= 32, "Bounded shared RX metadata");
// Values are borrowed ONLY for the callback. Consumers needing history copy
// into their own admitted bounded storage; count is clamped to MAX_TIMINGS.
struct ReceivePulses {
  const unsigned int *values = nullptr;
  uint16_t count = 0;
  bool clipped = false;
};
class ReceiveSink {
public:
  virtual ~ReceiveSink() = default;
  // Both references/views expire on return. Copy bounded metadata if needed.
  virtual void received(const ReceiveObservation &, ReceivePulses) = 0;
};
enum class ReceiveChannel : uint8_t { learning, diagnostics };
struct ReceiveSubscription {
  uint64_t generation = 0;
  ReceiveChannel channel = ReceiveChannel::learning;
};
// All operations are loop-owned, including registration and destruction. A
// sink must outlive registration. No heap, receiver mode changes or commands.
class ReceiveHub {
  struct Slot {
    ReceiveSink *sink = nullptr;
    uint64_t generation = 0;
  } slots[2];
  uint64_t registrations = 0, sequence = 0;
  bool delivering = false;

public:
  ReceiveHub() = default;
  ReceiveHub(const ReceiveHub &) = delete;
  ReceiveHub &operator=(const ReceiveHub &) = delete;
  bool active() const { return slots[0].sink || slots[1].sink; }
  bool subscribe(ReceiveChannel, ReceiveSink &, ReceiveSubscription &);
  bool observing(ReceiveSubscription, const ReceiveSink &) const;
  bool unsubscribe(ReceiveSubscription, ReceiveSink &);
  // Nested publication refuses. Registration during a callback starts with
  // the NEXT event; a callback may detach a later sink before it is invoked.
  bool publish(ReceiveObservation, ReceivePulses = {});
};
static_assert(sizeof(ReceiveHub) <= 64,
              "Two fixed observers, no growing registry");
ReceiveHub &receiveHub();
// Adapter for the actual decoded RX boundary. Invalid packets remain visible
// to diagnostics; learning separately validates bits/protocol/validity/age.
bool publishDecodedReceive(ReceiveHub &, const somfy_frame_t &,
                           const somfy_rx_t &, uint64_t observedAt,
                           bool scanning);
} // namespace LocalUi
