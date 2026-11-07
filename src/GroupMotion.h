#ifndef GROUP_MOTION_H
#define GROUP_MOTION_H
#include <stddef.h>
#include <math.h>
#include "SomfyFrame.h"

// Flag/sensor updates do not override a motor movement or its timed stop.
inline bool commandSupersedesPositioning(somfy_commands cmd) {
  return cmd != somfy_commands::Flag && cmd != somfy_commands::SunFlag && cmd != somfy_commands::Sensor;
}

// Group requests use client positions. A native start affects every paired
// member, so it is safe only when every member needs the same wire command.
// Intermediate targets closer than this are already reached. A start press
// alone travels further (about 1.4% at 20 s), so a smaller move would overshoot
// and leave members drifting apart on repeated requests.
static constexpr float GROUP_TARGET_DEADBAND = 1.0f;
struct GroupMemberMotion {
  float current = 0;
  int8_t direction = 0;     // current travel in stored coordinates
  bool stopPending = false; // a stop is queued or on air
  bool invertTarget = false; // awnings use the opposite physical convention
  bool flipCommands = false;
  bool idle = true;
  bool supported = true;
  bool targetSupported = true;
};
inline float groupStoredTarget(const GroupMemberMotion &m, float target) {
  return m.invertTarget ? 100.0f - target : target;
}
inline bool groupAtTarget(const GroupMemberMotion &m, float target) {
  // Endpoint requests must send even when the estimate already says "there".
  return target > 0 && target < 100 && fabsf(groupStoredTarget(m, target) - m.current) < GROUP_TARGET_DEADBAND;
}
inline bool groupMoveAhead(const GroupMemberMotion &m, float stored) {
  return (m.direction > 0 && stored > m.current) || (m.direction < 0 && stored < m.current);
}
inline int groupMoveDirection(const GroupMemberMotion &m, float target) {
  const float stored = groupStoredTarget(m, target);
  if(groupAtTarget(m, target)) return 0;
  int dir = stored == 0 ? -1 : stored == 100 ? 1 : stored > m.current ? 1 : stored < m.current ? -1 : 0;
  return m.flipCommands ? -dir : dir;
}
inline bool groupMoveCompatible(const GroupMemberMotion &m, float target, int wireDirection) {
  return m.idle && m.supported && wireDirection != 0 && groupMoveDirection(m, target) == wireDirection;
}
// Production and host tests share the orchestration. The adapter owns radio
// submission and adoption of the shared ticket by the real shade behaviours.
class GroupMotionOps {
  public:
    virtual ~GroupMotionOps() {}
    virtual size_t memberCount() = 0;
    virtual bool member(size_t index, GroupMemberMotion &out) = 0;
    virtual bool startGroup(int wireDirection, float target) = 0;
    virtual bool moveMember(size_t index, float storedTarget) = 0;
    // An already-correct member keeps still: forget any start still queued.
    virtual void keepMember(size_t index) {}
    // Reversals: stop every mover, then plan again once all are stopped, so
    // the restart can be one native press. False when unsupported.
    virtual bool deferUntilStopped(float target) { return false; }
    virtual void cancelDeferred() {}
    virtual bool stopMovingMember(size_t index) { return false; }
};
class GroupStopOps {
  public:
    virtual ~GroupStopOps() {}
    virtual size_t memberCount() = 0;
    virtual bool memberIdle(size_t index) = 0;
    virtual bool stopMember(size_t index) = 0;
};
inline bool runGroupStop(GroupStopOps &ops) {
  bool accepted = true;
  for(size_t i = 0; i < ops.memberCount(); i++) {
    // My on a completed member recalls its favorite, so only stop movers.
    if(!ops.memberIdle(i) && !ops.stopMember(i)) accepted = false;
  }
  return accepted;
}
inline bool runGroupTarget(GroupMotionOps &ops, float target) {
  if(!(target >= 0 && target <= 100)) return false;
  const size_t count = ops.memberCount();
  if(count == 0) return false;
  bool native = true;
  int wireDirection = 0;
  for(size_t i = 0; i < count; i++) {
    GroupMemberMotion m;
    if(!ops.member(i, m) || !m.targetSupported) return false; // validate before any movement
    if(i == 0) wireDirection = groupMoveDirection(m, target);
    native = native && groupMoveCompatible(m, target, wireDirection);
  }
  if(native) return ops.startGroup(wireDirection, target);
  // When every mover must stop first (a reversal or a passed target), stop
  // them all and plan again from rest, rather than restarting each member
  // separately and losing the shared start.
  bool movers = false, allStop = true, allSupported = true;
  for(size_t i = 0; i < count; i++) {
    GroupMemberMotion m;
    if(!ops.member(i, m)) return false;
    allSupported = allSupported && m.supported;
    if(m.idle) continue;
    movers = true;
    if(!m.stopPending && groupMoveAhead(m, groupStoredTarget(m, target))) allStop = false;
  }
  if(movers && allStop && allSupported && ops.deferUntilStopped(target)) {
    bool accepted = true;
    for(size_t i = 0; i < count; i++) {
      GroupMemberMotion m;
      if(ops.member(i, m) && !m.idle && !ops.stopMovingMember(i)) accepted = false;
    }
    if(!accepted) ops.cancelDeferred();
    return accepted;
  }
  // A native start would also move already-correct or opposite-direction
  // members. Fall back to their existing independent movement models.
  bool accepted = true;
  for(size_t i = 0; i < count; i++) {
    GroupMemberMotion m;
    if(!ops.member(i, m)) return false;
    const float stored = groupStoredTarget(m, target);
    if(m.idle && groupAtTarget(m, target)) { ops.keepMember(i); continue; }
    if(!ops.moveMember(i, stored)) accepted = false;
  }
  return accepted;
}
#endif
