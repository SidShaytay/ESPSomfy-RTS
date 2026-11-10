#pragma once
#include "LocalControlPolicy.h"
namespace LocalUi {
struct ControlMenu {
  ControlAction actions[uint8_t(ControlAction::count)]={};
  uint8_t count=0;
  int index(ControlAction action) const {
    for(uint8_t i=0;i<count;i++) if(actions[i]==action) return i;
    return -1;
  }
};
// The sentinel is a navigation item only. It never reaches request preparation.
inline ControlMenu controlMenu(ControlMask mask,bool primary) {
  ControlMenu menu;
  const ControlMask main=controlActionBit(ControlAction::up)|controlActionBit(ControlAction::stop)|
    controlActionBit(ControlAction::down)|controlActionBit(ControlAction::toggle);
  for(uint8_t i=0;i<uint8_t(ControlAction::count);i++) {
    const auto action=ControlAction(i);
    if((mask&controlActionBit(action)) && bool(main&controlActionBit(action))==primary)
      menu.actions[menu.count++]=action;
  }
  if(primary && (mask&~main&allControlActions)) menu.actions[menu.count++]=ControlAction::count;
  return menu;
}
inline ControlAction defaultControlAction(ControlMask mask) {
  const auto menu=controlMenu(mask,true);
  return menu.index(ControlAction::stop)>=0?ControlAction::stop:menu.count?menu.actions[0]:ControlAction::stop;
}
inline const char *controlLabel(ControlAction action) {
  static const char *labels[]={"UP","STOP","DOWN","Position","Tilt up","Tilt STOP","Tilt down",
    "Tilt position","MY position","Favorite (80-bit)","Toggle","Step up","Step down","Sun on","Sun off","Set favourite here","Clear favourite","More"};
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
