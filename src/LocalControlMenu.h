#pragma once
#include "LocalControlPolicy.h"
namespace LocalUi {
constexpr ControlAction repeatsNavigation=ControlAction(uint8_t(ControlAction::count)+1);
constexpr ControlAction calibrationNavigation=ControlAction(uint8_t(ControlAction::count)+2);
inline bool calibrationControl(const ControlDescriptor &d) {
  return !d.group && d.tiltType==tilt_types::none &&
    (d.proto==radio_proto::RTS || d.proto==radio_proto::RTW || d.proto==radio_proto::RTV) &&
    (d.shadeType==shade_types::roller || d.shadeType==shade_types::blind ||
     d.shadeType==shade_types::shutter || d.shadeType==shade_types::awning ||
     d.shadeType==shade_types::ldrapery || d.shadeType==shade_types::rdrapery || d.shadeType==shade_types::cdrapery);
}
struct ControlMenu {
  ControlAction actions[uint8_t(ControlAction::count)+2]={};
  uint8_t count=0;
  int index(ControlAction action) const {
    for(uint8_t i=0;i<count;i++) if(actions[i]==action) return i;
    return -1;
  }
};
// The sentinel is a navigation item only. It never reaches request preparation.
inline ControlMenu controlMenu(ControlMask mask,bool primary,bool repeats=false,bool calibration=false) {
  ControlMenu menu;
  if(!primary && calibration) menu.actions[menu.count++]=calibrationNavigation;
  const ControlMask main=controlActionBit(ControlAction::up)|controlActionBit(ControlAction::stop)|
    controlActionBit(ControlAction::down)|controlActionBit(ControlAction::toggle);
  for(uint8_t i=0;i<uint8_t(ControlAction::count);i++) {
    const auto action=ControlAction(i);
    if((mask&controlActionBit(action)) && bool(main&controlActionBit(action))==primary)
      menu.actions[menu.count++]=action;
  }
  if(!primary && repeats) menu.actions[menu.count++]=repeatsNavigation;
  if(primary && ((mask&~main&allControlActions)||repeats||calibration)) menu.actions[menu.count++]=ControlAction::count;
  return menu;
}
inline ControlMenu repeatMenu(const ControlDescriptor &descriptor) {
  ControlMenu menu;const auto mask=repeatableControls(descriptor);
  for(uint8_t i=0;i<uint8_t(ControlAction::count);i++)
    if(mask&controlActionBit(ControlAction(i))) menu.actions[menu.count++]=ControlAction(i);
  return menu;
}
inline ControlAction defaultControlAction(ControlMask mask) {
  const auto menu=controlMenu(mask,true);
  return menu.index(ControlAction::stop)>=0?ControlAction::stop:menu.count?menu.actions[0]:ControlAction::stop;
}
inline const char *controlLabel(ControlAction action) {
  static const char *labels[]={"UP","STOP","DOWN","Position","Tilt up","Tilt STOP","Tilt down",
    "Tilt position","Go to favourite","Favorite (80-bit)","Toggle","Step up","Step down","Sun on","Sun off","Save current as favourite","Clear saved favourite","More"};
  if(action==calibrationNavigation) return "Guided calibration";
  if(action==repeatsNavigation) return "Send with repeats";
  return uint8_t(action)<=uint8_t(ControlAction::count)?labels[uint8_t(action)]:"Unavailable";
}
inline bool controlNeedsValue(ControlAction action) {
  return action==ControlAction::position || action==ControlAction::tiltPosition ||
    action==ControlAction::stepUp || action==ControlAction::stepDown;
}
inline bool controlIsStep(ControlAction action) {
  return action==ControlAction::stepUp || action==ControlAction::stepDown;
}
} // namespace LocalUi
