#include <cassert>
#include <cstdint>
#include "GitUpdateHealth.h"
int main() {
  GitUpdateHealth health;
  assert(!health.proven(true, 100, 60000));
  assert(!health.proven(true, 60099, 60000));
  assert(health.proven(true, 60100, 60000));
  // Losing any health prerequisite restarts the continuous observation.
  assert(!health.proven(false, 70000, 60000));
  assert(!health.proven(true, 80000, 60000));
  assert(!health.proven(true, 139999, 60000));
  assert(health.proven(true, 140000, 60000));
  GitUpdateHealth wrapped;
  assert(!wrapped.proven(true, UINT32_MAX - 30000, 60000));
  assert(!wrapped.proven(true, 29998, 60000));
  assert(wrapped.proven(true, 29999, 60000));
}
