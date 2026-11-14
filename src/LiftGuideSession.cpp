#include "LiftGuideSession.h"

bool LiftGuideSession::release() {
  if(!held) return true;
  if(!backend.release(lease)) return false;
  held = false;
  lease = 0;
  return true;
}
void LiftGuideSession::fail() {
  guide.refused();
  stopAfterStart = false;
  if(pending && !started) backend.cancel(pending);
  pending = 0;
  release();
}
bool LiftGuideSession::fresh(uint64_t now) {
  LiftGuideContext current;
  if(!held || now < observedAt || now - admittedAt >= maximumMs ||
      !backend.current(source) || !backend.owns(lease) ||
      !backend.capture(context.id,current) || !context.same(current)) {
    if(held) fail();
    return false;
  }
  observedAt = now;
  return true;
}
bool LiftGuideSession::begin(const LiftGuideSource &caller, uint8_t id, LiftKind kind,
                             uint64_t now, uint32_t &token) {
  if(!caller.valid() || !backend.current(caller)) return false;
  // A lost Begin reply must not strand its owner or start a second draft.
  if(held) {
    if(!source.same(caller) || context.id != id || guide.calibration().kind != kind || !fresh(now)) return false;
    token = generation;
    return true;
  }
  if(!generation || generation == UINT32_MAX) return false;
  LiftGuideContext target;
  LiftGuide next;
  if(!next.begin(kind,uint32_t(now)) || !backend.capture(id,target) || target.id != id || !target.same(target)) return false;
  uint64_t owner = 0;
  if(!backend.acquire(owner)) return false;
  lease = owner; held = true;
  source = caller; context = target;
  guide = next;
  pending = 0; started = stopAfterStart = stopping = false;
  savedResult = {};
  admittedAt = observedAt = now;
  if(!fresh(now)) return false;
  ++generation;
  token = generation;
  return true;
}
bool LiftGuideSession::send(somfy_commands command, bool stop) {
  const auto result = backend.send(context.id,command,stop);
  if(!result.sent || !result.ticket) { fail(); return false; }
  pending = result.ticket;
  started = false;
  stopping = stop;
  if(!stop && !guide.queued(pending)) { backend.cancel(pending); fail(); return false; }
  return true;
}
bool LiftGuideSession::act(const LiftGuideSource &caller, uint32_t token,
                           LiftGuideAction action, uint64_t now, bool human) {
  if(token != generation || !source.same(caller) || !backend.current(caller)) return false;
  if(!held) {
    return action == LiftGuideAction::cancel && guide.act(action,uint32_t(now),human).accepted;
  }
  if(!fresh(now)) return false;
  const auto result = guide.act(action,uint32_t(now),human);
  if(!result.accepted) return false;
  using E = LiftGuideEffect;
  switch(result.effect) {
    case E::driveUp:return send(somfy_commands::Up,false);
    case E::driveDown:return send(somfy_commands::Down,false);
    case E::anchorOpen:case E::anchorClosed:
      if(!backend.anchor(context.id,result.effect == E::anchorClosed)) { fail(); return false; }
      if(!backend.capture(context.id,context)) { fail(); return false; }
      started = false;
      return true;
    case E::cancelStart:
      if(pending && !backend.cancel(pending)) {
        // The worker may already have started, before loop saw that event.
        // An explicit Stop waits for the full start outcome, never overtakes it.
        stopAfterStart = true;
      } else { pending = 0; release(); }
      return true;
    case E::stop:
      if(pending) { stopAfterStart = true; return true; }
      return send(somfy_commands::My,true);
    case E::save:
      savedResult = backend.save(context.id,guide.calibration(),guide.upTime(),guide.downTime());
      guide.saved(savedResult.applied,savedResult.durable);
      release();
      return true;
    case E::close:
      if(pending && !started) backend.cancel(pending);
      pending = 0; stopAfterStart = false;
      release();
      return true;
    case E::none:return true;
  }
  return false;
}
void LiftGuideSession::txEvent(const somfy_tx_event_t &event) {
  if(!held || !pending || pending != event.ticket) return;
  LiftGuideContext current;
  if(!backend.current(source) || !backend.owns(lease) ||
      !backend.capture(context.id,current) || !context.same(current)) { fail(); return; }
  if(event.type == tx_event_t::started) started = true;
  guide.txEvent(event.type,event.ticket,event.at);
  if(event.type == tx_event_t::dropped || event.type == tx_event_t::aborted) {
    pending = 0;
    fail();
    return;
  }
  if(event.type != tx_event_t::finished) return;
  pending = 0;
  if(stopping) {
    stopping = false;
    const bool midpoint = guide.stage() == LiftGuideStage::reversePositionStopping;
    if(guide.stopped()) {
      if(midpoint && (!backend.anchorHalf(context.id) || !backend.capture(context.id,context))) fail();
    } else release();
    return;
  }
  if(stopAfterStart) {
    stopAfterStart = false;
    send(somfy_commands::My,true);
  }
}
bool LiftGuideSession::snapshot(const LiftGuideSource &caller, uint32_t token,
                                uint64_t now, LiftGuideSnapshot &out) {
  if(token != generation || !source.same(caller) || !backend.current(caller)) return false;
  if(held && !fresh(now)) return false;
  out = {};
  out.token = generation; out.id = context.id; out.stage = guide.stage();
  out.calibration = guide.calibration(); out.upTime = guide.upTime(); out.downTime = guide.downTime();
  out.reversalsKnown = guide.reversalsKnown();
  out.owned = held; out.applied = savedResult.applied; out.durable = savedResult.durable;
  return true;
}
void LiftGuideSession::poll(uint64_t now) { if(held) fresh(now); }
void LiftGuideSession::interrupt() { if(held) fail(); }
