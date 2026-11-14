#pragma once
#include "LiftCalibration.h"
#include "TxBuffer.h"

enum class LiftGuideStage : uint8_t {
  closed, reference, readyDown, startingDown, downTakeup, downHalf, downEnd,
  downConfirm, readyUp, startingUp, upTakeup, upHalf, upEnd, upConfirm,
  review, saving, complete, failed
};
enum class LiftGuideAction : uint8_t {
  open, stop, confirmOpen, startDown, startUp, moving, halfway, visibleEnd,
  confirmEndpoint, save, cancel
};
enum class LiftGuideEffect : uint8_t {
  none, driveUp, driveDown, stop, cancelStart, anchorOpen, anchorClosed, save, close
};
struct LiftGuideResult {
  bool accepted = false;
  LiftGuideEffect effect = LiftGuideEffect::none;
};
// A draft and operator observations only. The loop-owned service performs RF,
// authorization, persistence and source/target ownership checks separately.
class LiftGuide {
  LiftGuideStage phase = LiftGuideStage::closed;
  LiftCalibration profile;
  uint32_t up = 0, down = 0, ticket = 0, start = 0, movingAt = 0,
           halfwayAt = 0, endAt = 0;
  bool started = false, directionDown = true, awaitingQueue = false;
  bool endpoint(uint32_t now);
public:
  bool begin(LiftKind, uint32_t now);
  LiftGuideResult act(LiftGuideAction, uint32_t now, bool human = true);
  bool queued(uint32_t);
  void refused();
  void txEvent(tx_event_t, uint32_t ticket, uint32_t at);
  void saved(bool applied, bool durable);
  LiftGuideStage stage() const { return phase; }
  const LiftCalibration &calibration() const { return profile; }
  uint32_t upTime() const { return up; }
  uint32_t downTime() const { return down; }
};
