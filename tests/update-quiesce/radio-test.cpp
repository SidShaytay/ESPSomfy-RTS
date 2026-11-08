#include "RadioUpdateActivity.h"
#include <cassert>
int main() {
  RadioUpdateActivity idle;
  idle.schedulerIdle = true;
  assert(idle.canPause());
  auto pending = idle;
  pending.receivedFrames = 1; assert(!pending.canPause());
  pending = idle; pending.receivingFrame = true; assert(!pending.canPause());
  pending = idle; pending.repeaterFrames = 1; assert(!pending.canPause());
  pending = idle; pending.schedulerIdle = false; assert(!pending.canPause());
  pending = idle; pending.suspended = true; assert(!pending.canPause());
  pending = idle; pending.scanning = true; assert(!pending.canPause());
}
