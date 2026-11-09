#pragma once
#include "LocalReceive.h"
namespace LocalUi {
enum class LearnCheck : uint8_t {
  ready,
  inactive,
  expired,
  clock,
  sequence,
  busy,
  changed,
  regressed,
  invalid
};
struct LearnCandidate {
  uint64_t observedAt = 0, revision = 0;
  uint32_t address = 0;
  uint16_t rollingCode = 0, highwater = 0;
  int16_t rssi = 0;
  uint16_t lastCode = 0;
  uint8_t protocol = 0, bits = 0, command = 0, flags = 0;
};
static_assert(sizeof(LearnCandidate) <= 32, "Eight fixed candidate records");
struct LearnSelection {
  uint64_t session = 0, revision = 0;
  uint32_t address = 0;
  uint16_t rollingCode = 0;
  uint8_t protocol = 0, bits = 0;
};
// Receive-only loop object, suitable for the existing wiping PSRAM session.
// Selection is NOT link permission: caller still needs fresh auth/raw context
// and the checked domain's NVS/all-alias monotonic guard immediately on apply.
class LearnCandidates final : public ReceiveSink {
  ReceiveHub &hub;
  ReceiveSubscription subscription;
  LearnCandidate records[8];
  LearnSelection selected;
  uint64_t startedAt = 0, observedAt = 0, lastSequence = 0;
  LearnCheck lifecycle = LearnCheck::inactive;
  uint8_t total = 0, chosen = 255;
  bool watching = false, overflow = false;
  static constexpr uint8_t regressedFlag = 1, formatFlag = 2;
  void finish(LearnCheck);
  LearnCheck eligible(const LearnCandidate &) const;
  void received(const ReceiveObservation &, ReceivePulses) override;

public:
  static constexpr uint64_t windowMs = 30000;
  explicit LearnCandidates(ReceiveHub &value) : hub(value) {}
  ~LearnCandidates() override { end(); }
  LearnCandidates(const LearnCandidates &) = delete;
  LearnCandidates &operator=(const LearnCandidates &) = delete;
  bool begin(uint64_t now);
  void end() { finish(LearnCheck::inactive); }
  LearnCheck tick(uint64_t now);
  LearnCheck status() const { return lifecycle; }
  uint8_t count() const { return total; }
  bool overflowed() const { return overflow; }
  bool candidate(uint8_t, LearnCandidate &) const;
  LearnCheck select(uint8_t, uint64_t now, LearnSelection &);
  LearnCheck validate(const LearnSelection &, uint64_t now);
};
static_assert(sizeof(LearnCandidates) <= 448,
              "Bounded receive state, no frame or raw pulses");
} // namespace LocalUi
