#pragma once
#include "ShadeGpioPolicy.h"
#include "ShadeTypes.h"
namespace ShadeGpioPolicy {
// These are existing asserted outputs, not a new motor mapping. Every assigned
// physical pad is independently validated before this logical check.
inline uint8_t commandMask(radio_proto proto, shade_types type, somfy_commands cmd) {
  const bool single=isToggleShade(type)||type==shade_types::drycontact;
  const bool two=type==shade_types::drycontact2;
  if(proto==radio_proto::GP_Remote) {
    switch(cmd) {
      case somfy_commands::My: return !single ? my : 0;
      case somfy_commands::Up: return !single&&!two ? up : 0;
      case somfy_commands::Toggle:
      case somfy_commands::Down: return down;
      case somfy_commands::MyUp: return !single&&!two ? up|my : 0;
      case somfy_commands::MyDown: return !single&&!two ? down|my : 0;
      case somfy_commands::MyUpDown: return isToggleShade(type) ? up|down|my : 0;
      default: return 0;
    }
  }
  if(proto==radio_proto::GP_Relay) {
    if(type==shade_types::drycontact)
      return cmd==somfy_commands::My||cmd==somfy_commands::Toggle||
             cmd==somfy_commands::Up||cmd==somfy_commands::Down ? down : 0;
    // The existing two-contact relay maps closed to Up, open to Down.
    if(two) return cmd==somfy_commands::Up ? down : cmd==somfy_commands::Down ? up : 0;
    switch(cmd) {
      case somfy_commands::Up:return up;
      case somfy_commands::Down:return down;
      case somfy_commands::My:return isToggleShade(type) ? up|down : 0x80; // stop deasserts any configured direction
      case somfy_commands::Toggle:return up|down;
      default:return 0;
    }
  }
  return 0;
}
inline bool assigned(uint8_t required,uint8_t upPin,uint8_t downPin,uint8_t myPin) {
  const uint8_t available=(upPin!=255?up:0)|(downPin!=255?down:0)|(myPin!=255?my:0);
  return required==0x80 ? (available&(up|down))!=0 : required && (available&required)==required;
}
} // namespace ShadeGpioPolicy
