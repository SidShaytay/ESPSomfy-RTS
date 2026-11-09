#pragma once
#include <cstdint>
struct GitUpdateBootStatus {bool pending=false,rollbackRequired=false;};
GitUpdateBootStatus gitUpdateBootStatus();
// Loop-owned health policy only. rollbackRequired is an intent, never permission
// to bypass the controller's movement, persistence or final restart checks.
class GitUpdateBootHealth {
  uint64_t startedAt=0,lastTime=0,healthySince=0,lastAttempt=0;
  bool pending=false,observing=false,attempted=false,expired=false;
public:
  enum class Action:uint8_t {none,confirm,rollbackRequired};
  void start(bool awaitingConfirmation,uint64_t bootTime=0) {
    startedAt=lastTime=bootTime;healthySince=lastAttempt=0;
    pending=awaitingConfirmation;observing=attempted=expired=false;
  }
  Action poll(bool healthy,uint64_t now) {
    if(!pending || expired)return Action::none;
    if(now==UINT64_MAX || now<lastTime || now<startedAt || now-startedAt>=180000) {
      expired=true;observing=false;return Action::rollbackRequired;
    }
    if(now-lastTime>1000)observing=false; // An unobserved interval proves no loop health.
    lastTime=now;
    if(!healthy){observing=false;return Action::none;}
    if(!observing){healthySince=now;observing=true;}
    if(now-healthySince<60000 || (attempted && now-lastAttempt<1000))return Action::none;
    attempted=true;lastAttempt=now;return Action::confirm;
  }
  void confirmed(){pending=false;}
  bool rollbackRequired()const{return expired;}
};
