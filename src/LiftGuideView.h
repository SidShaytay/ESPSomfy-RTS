#pragma once
#include "LiftGuide.h"
struct LiftGuideChoice { LiftGuideAction action; const char *label; };
struct LiftGuideView {
  const char *title = "Calibration", *hint = "";
  LiftGuideChoice choices[4]{};
  uint8_t count = 0;
};
LiftGuideView liftGuideView(LiftGuideStage, bool applied = false, bool durable = false, bool reversalsKnown = true);
