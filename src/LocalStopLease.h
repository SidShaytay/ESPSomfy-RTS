#pragma once
#include <stdint.h>
namespace LocalUi {
// A bounded context for one shade or group. Members are in stable model-ID order.
// configuration covers all relevant type/protocol/pairing/command settings.
struct StopContext {
  uint8_t id = 0, members = 0;
  bool group = false;
  uint32_t identity = 0, securityGeneration = 0;
  uint64_t configuration = 0;
  uint8_t memberIds[32] = {};
  uint32_t memberIdentities[32] = {};
  bool same(const StopContext &other) const {
    if(!id || !members || members > 32 || id != other.id || group != other.group ||
       members != other.members || identity != other.identity ||
       securityGeneration != other.securityGeneration || configuration != other.configuration)
      return false;
    for(uint8_t i=0;i<members;i++)
      if(memberIds[i] != other.memberIds[i] || memberIdentities[i] != other.memberIdentities[i])
        return false;
    return true;
  }
};
class StopLease {
  StopContext context;
  uint32_t eligible = 0;
  bool sampled = false, frozen = false;
public:
  void observe(const StopContext &current, uint32_t moving, bool authorized, bool expired) {
    if(authorized) {
      context = current;
      eligible = current.members < 32 ? moving & ((uint32_t(1) << current.members) - 1) : moving;
      sampled = current.id && current.members && current.members <= 32;
      frozen = false;
    } else if(!expired || !sampled || !context.same(current)) {
      eligible = 0;
      sampled = false;
      frozen = false;
    } else {
      // Intersection is irreversible until a new authorized observation. A
      // member first moving after expiry never enters the frozen eligible set.
      eligible &= moving;
      frozen = true;
    }
  }
  uint32_t allowed(const StopContext &current) const {
    return frozen && sampled && context.same(current) ? eligible : 0;
  }
};
} // namespace LocalUi
