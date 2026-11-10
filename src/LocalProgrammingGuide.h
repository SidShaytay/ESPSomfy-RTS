#pragma once
#include "LocalEntityView.h"
#include <stdio.h>
#include <string.h>
namespace LocalUi { namespace Advanced {
enum class ProgrammingEffect : uint8_t {none,close,enqueue,markPaired,markUnpaired};
// Loop-owned UI state only. The containing form owns authority/time/context,
// the existing FIFO and all domain effects. A response is never inferred.
class ProgrammingGuide {
  enum class Stage:uint8_t {closed,intent,memory,draft,repeat,review,pending,observe,marker,refused};
  Stage stage=Stage::closed;
  uint8_t cursor=0,repeatCount=1;
  bool markerBefore=false,oneButton=false,pair=true,accepted=false,observed=false;
  ProgrammingEffect close(){*this=ProgrammingGuide{};return ProgrammingEffect::close;}
  static void text(char *out,size_t capacity,const char *value){snprintf(out,capacity,"%s",value);}
  void review(EntityAdmin::FormView &out,const char *heading,const char *detail,const char *hint,const char *action)const{
    out.mode=EntityAdmin::Mode::programmingReview;out.total=out.rowCount=2;
    text(out.rows[0].label,sizeof(out.rows[0].label),"Cancel");
    text(out.rows[1].label,sizeof(out.rows[1].label),action);
    text(out.rows[2].label,sizeof(out.rows[2].label),heading);
    text(out.rows[2].value,sizeof(out.rows[2].value),detail);
    text(out.hint,sizeof(out.hint),hint);
  }
public:
  void begin(bool pairedMarker,bool oneButtonMotor){
    *this=ProgrammingGuide{};markerBefore=pairedMarker;oneButton=oneButtonMotor;stage=Stage::intent;
  }
  bool pending()const{return stage==Stage::pending;}
  uint8_t repeats()const{return repeatCount;}
  void turn(int direction){
    if(!direction||stage==Stage::closed)return;
    if(stage==Stage::repeat){
      if(direction>0&&repeatCount<127)++repeatCount;
      else if(direction<0&&repeatCount>1)--repeatCount;
      return;
    }
    const uint8_t count=stage==Stage::intent||stage==Stage::draft?3:
      stage==Stage::pending||stage==Stage::refused?1:2;
    cursor=uint8_t((cursor+(direction>0?1:count-1))%count);
  }
  ProgrammingEffect activate(){
    switch(stage){
      case Stage::intent:
        if(!cursor)return close();
        pair=cursor==1;stage=Stage::memory;cursor=0;break;
      case Stage::memory:
        if(!cursor)return close();
        stage=Stage::draft;cursor=0;break;
      case Stage::draft:
        if(!cursor)return close();
        stage=cursor==1?Stage::repeat:Stage::review;cursor=0;break;
      case Stage::repeat:stage=Stage::draft;cursor=1;break;
      case Stage::review:
        if(!cursor){stage=Stage::draft;cursor=2;break;}
        stage=Stage::pending;cursor=0;return ProgrammingEffect::enqueue;
      case Stage::observe:
        if(!cursor)return close();
        if(!accepted)return close();
        observed=true;stage=Stage::marker;cursor=0;break;
      case Stage::marker:
        if(!cursor)return close();
        if(!accepted||!observed)return close();
        {const auto effect=pair?ProgrammingEffect::markPaired:ProgrammingEffect::markUnpaired;close();return effect;}
      case Stage::pending:
      case Stage::refused:return close();
      case Stage::closed:break;
    }
    return ProgrammingEffect::none;
  }
  ProgrammingEffect back(){
    if(stage==Stage::closed)return ProgrammingEffect::none;
    if(stage==Stage::repeat){stage=Stage::draft;cursor=1;return ProgrammingEffect::none;}
    if(stage==Stage::review){stage=Stage::draft;cursor=2;return ProgrammingEffect::none;}
    if(stage==Stage::marker){stage=Stage::observe;cursor=0;return ProgrammingEffect::none;}
    return close();
  }
  void complete(bool requestAccepted){
    if(stage!=Stage::pending)return;
    accepted=requestAccepted;observed=false;stage=requestAccepted?Stage::observe:Stage::refused;cursor=0;
  }
  void render(EntityAdmin::FormView &out)const{
    for(auto &row:out.rows)row={};
    out.hint[0]=0;out.total=out.rowCount=0;out.offset=0;out.selected=cursor;out.protectedFrame=true;
    out.mode=EntityAdmin::Mode::stackedRows;
    char detail[48];
    switch(stage){
      case Stage::intent:
        out.total=out.rowCount=3;
        text(out.rows[0].label,sizeof(out.rows[0].label),"Cancel");
        text(out.rows[1].label,sizeof(out.rows[1].label),"Pair controller");
        text(out.rows[2].label,sizeof(out.rows[2].label),"Unpair controller");
        snprintf(out.hint,sizeof(out.hint),"Configured marker: %s. Motor state is not verified.",markerBefore?"Paired":"Unpaired");break;
      case Stage::memory:
        review(out,"Open motor learn mode","Follow the motor's own pairing instructions.",
          "LCD has sent nothing. Confirm the observed learn mode.","I observed learn mode");break;
      case Stage::draft:
        out.total=out.rowCount=3;
        text(out.rows[0].label,sizeof(out.rows[0].label),"Cancel");
        text(out.rows[1].label,sizeof(out.rows[1].label),"Repeats");
        snprintf(out.rows[1].value,sizeof(out.rows[1].value),"%u",unsigned(repeatCount));
        text(out.rows[2].label,sizeof(out.rows[2].label),"Review and send");
        text(out.rows[2].value,sizeof(out.rows[2].value),pair?"Pair request":"Unpair request");
        text(out.hint,sizeof(out.hint),oneButton?"Prog may operate this one-button motor. Review carefully.":"Motor must be in its documented learn mode.");break;
      case Stage::repeat:
        out.mode=EntityAdmin::Mode::fields;out.total=out.rowCount=1;out.selected=0;
        text(out.rows[0].label,sizeof(out.rows[0].label),"Repeats (1-127)");
        snprintf(out.rows[0].value,sizeof(out.rows[0].value),"%u",unsigned(repeatCount));
        text(out.hint,sizeof(out.hint),"Turn: adjust. Press or Back: draft.");break;
      case Stage::review:
        snprintf(detail,sizeof(detail),"Prog request; repeat count: %u",unsigned(repeatCount));
        review(out,pair?"Pair controller":"Unpair controller",detail,
          oneButton?"Prog may move this motor. Check its learn mode first.":"Changes motor membership. Observe its physical response.","Send programming request");break;
      case Stage::pending:
        out.mode=EntityAdmin::Mode::validation;out.total=out.rowCount=1;
        text(out.rows[0].label,sizeof(out.rows[0].label),"Cancel queued request");
        text(out.hint,sizeof(out.hint),"FIFO; expires in 5s. Accepted frames cannot be undone.");break;
      case Stage::observe:
        review(out,"Request accepted","Motor result is not confirmed by software.",
          "A jog can mean add or remove. Verify intended result.","I verified intended result");
        text(out.rows[0].label,sizeof(out.rows[0].label),"Close");break;
      case Stage::marker:
        review(out,"Paired marker only",pair?"Set marker: Paired":"Set marker: Unpaired",
          "Your observation; saves a marker, not motor pairing.",pair?"Save marker: Paired":"Save marker: Unpaired");break;
      case Stage::refused:
        out.total=out.rowCount=1;text(out.rows[0].label,sizeof(out.rows[0].label),"Close");
        text(out.rows[0].value,sizeof(out.rows[0].value),"Programming request refused");
        text(out.hint,sizeof(out.hint),"No automatic retry. Review motor state before trying again.");break;
      case Stage::closed:out.mode=EntityAdmin::Mode::closed;break;
    }
    for(uint8_t i=0;i<out.rowCount;++i)out.rows[i].selected=i==out.selected;
  }
};
static_assert(sizeof(ProgrammingGuide)<=16,"Only bounded guide state; authority and FIFO stay with the form");
} }
