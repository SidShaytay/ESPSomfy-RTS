#pragma once
#include "LocalEntityView.h"
#include <stdio.h>
#include <string.h>
namespace LocalUi { namespace Advanced {
enum class GroupProgrammingEffect:uint8_t {none,close,openMemory,sendGroup,saveList};
// UI state only. The containing form supplies the exact group/device witness,
// authorization, existing FIFO and shared-domain effects.
class GroupProgrammingGuide {
  enum class Stage:uint8_t {closed,memoryChoice,memoryReview,memoryPending,memoryObserved,
    groupReview,groupPending,groupObserved,listReview,listPending,refused};
  Stage stage=Stage::closed;
  uint8_t cursor=0;
  bool adding=false,oneButton=false;
  GroupProgrammingEffect close(){*this=GroupProgrammingGuide{};return GroupProgrammingEffect::close;}
  static void text(char *out,size_t capacity,const char *value){snprintf(out,capacity,"%s",value);}
public:
  void begin(bool add,bool toggle){*this=GroupProgrammingGuide{};adding=add;oneButton=toggle;stage=Stage::memoryChoice;}
  bool pending()const{return stage==Stage::memoryPending||stage==Stage::groupPending||stage==Stage::listPending;}
  GroupProgrammingEffect pendingEffect()const{return stage==Stage::memoryPending?GroupProgrammingEffect::openMemory:
    stage==Stage::groupPending?GroupProgrammingEffect::sendGroup:stage==Stage::listPending?GroupProgrammingEffect::saveList:GroupProgrammingEffect::none;}
  void turn(int direction){
    if(!direction||stage==Stage::closed)return;
    const uint8_t count=stage==Stage::memoryChoice?3:pending()||stage==Stage::refused?1:2;
    cursor=uint8_t((cursor+(direction>0?1:count-1))%count);
  }
  GroupProgrammingEffect activate(){
    switch(stage){
      case Stage::memoryChoice:
        if(!cursor)return close();
        stage=cursor==1?Stage::memoryReview:Stage::groupReview;cursor=0;break;
      case Stage::memoryReview:
        if(!cursor){stage=Stage::memoryChoice;break;}
        stage=Stage::memoryPending;cursor=0;return GroupProgrammingEffect::openMemory;
      case Stage::memoryObserved:
        if(!cursor)return close();
        stage=Stage::groupReview;cursor=0;break;
      case Stage::groupReview:
        if(!cursor)return close();
        stage=Stage::groupPending;cursor=0;return GroupProgrammingEffect::sendGroup;
      case Stage::groupObserved:
        if(!cursor)return close();
        stage=Stage::listReview;cursor=0;break;
      case Stage::listReview:
        if(!cursor)return close();
        stage=Stage::listPending;cursor=0;return GroupProgrammingEffect::saveList;
      case Stage::memoryPending:case Stage::groupPending:case Stage::listPending:case Stage::refused:return close();
      case Stage::closed:break;
    }
    return GroupProgrammingEffect::none;
  }
  GroupProgrammingEffect back(){return stage==Stage::closed?GroupProgrammingEffect::none:close();}
  void complete(bool accepted){
    if(!pending())return;
    stage=!accepted?Stage::refused:stage==Stage::memoryPending?Stage::memoryObserved:
      stage==Stage::groupPending?Stage::groupObserved:Stage::closed;
    cursor=0;
  }
  void render(EntityAdmin::FormView &out)const{
    for(auto &row:out.rows)row={};out.hint[0]=0;out.offset=0;out.selected=cursor;
    out.total=out.rowCount=2;out.protectedFrame=true;out.mode=EntityAdmin::Mode::groupProgrammingReview;
    text(out.rows[0].label,sizeof(out.rows[0].label),"Cancel");
    const char *heading="",*detail="",*hint="",*action="";
    switch(stage){
      case Stage::memoryChoice:
        out.mode=EntityAdmin::Mode::stackedRows;out.total=out.rowCount=3;
        text(out.rows[1].label,sizeof(out.rows[1].label),"Review LCD open memory");
        text(out.rows[2].label,sizeof(out.rows[2].label),"I observed learn mode");
        text(out.hint,sizeof(out.hint),"Use the motor instructions. LCD has sent nothing.");break;
      case Stage::memoryReview:
        heading="Open selected motor memory";detail="Device Prog; repeat count: 40";
        hint=oneButton?"Prog may move this motor. Check instructions first.":"Motor may jog. Observe learn mode before proceeding.";
        action="Send device Prog";break;
      case Stage::memoryObserved:
        heading="Device request accepted";detail="Software cannot confirm motor learn mode.";
        hint="Observe the motor; a jog alone is not proof.";action="I observed learn mode";break;
      case Stage::groupReview:
        heading=adding?"Pair motor to group":"Unpair motor from group";detail="Group Prog; repeat count: 1";
        hint="Other motors in learn mode may also be affected.";action="Send group Prog";break;
      case Stage::groupObserved:
        heading="Group request accepted";detail="Software cannot confirm motor membership.";
        hint="A jog can mean add or remove. Verify intended result.";action="I verified intended result";
        text(out.rows[0].label,sizeof(out.rows[0].label),"Close");break;
      case Stage::listReview:
        heading="Controller list only";detail=adding?"Add device to group list":"Remove device from group list";
        hint="Your observation; saves configuration. Sends no RF.";action=adding?"Save list: Add":"Save list: Remove";break;
      case Stage::memoryPending:case Stage::groupPending:case Stage::listPending:
        out.mode=EntityAdmin::Mode::validation;out.total=out.rowCount=1;
        text(out.rows[0].label,sizeof(out.rows[0].label),"Cancel queued request");
        text(out.hint,sizeof(out.hint),"FIFO; expires in 5s. Sent frames cannot be recalled.");break;
      case Stage::refused:
        out.mode=EntityAdmin::Mode::stackedRows;out.total=out.rowCount=1;
        text(out.rows[0].label,sizeof(out.rows[0].label),"Close");text(out.rows[0].value,sizeof(out.rows[0].value),"Request refused");
        text(out.hint,sizeof(out.hint),"No automatic retry. Inspect motor state before retry.");break;
      case Stage::closed:out.mode=EntityAdmin::Mode::closed;out.total=out.rowCount=0;break;
    }
    if(*heading){text(out.rows[2].label,sizeof(out.rows[2].label),heading);text(out.rows[2].value,sizeof(out.rows[2].value),detail);
      text(out.hint,sizeof(out.hint),hint);text(out.rows[1].label,sizeof(out.rows[1].label),action);}
    for(uint8_t i=0;i<out.rowCount;++i)out.rows[i].selected=i==out.selected;
  }
};
static_assert(sizeof(GroupProgrammingGuide)<=8,"Only bounded guide state; no authority or model copies");
} }
