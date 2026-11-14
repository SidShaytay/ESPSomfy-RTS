#pragma once
#include "LiftGuideSession.h"
#include "LiftGuideView.h"
struct loop_item_t;
namespace LiftCal {
LiftGuideSource webSource(uint32_t client);
LiftGuideSource localSource(uint32_t session, uint32_t configuration);
bool available(uint8_t id);
bool begin(const LiftGuideSource &, uint8_t id, LiftKind, uint32_t &token);
bool act(const LiftGuideSource &, uint32_t token, LiftGuideAction, bool human = true);
bool snapshot(const LiftGuideSource &, uint32_t token, LiftGuideSnapshot &);
void poll();
void runProtocol(const loop_item_t &);
}
