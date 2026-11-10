#pragma once
#include "LocalEntityView.h"
#include "ShadeTypes.h"
#include "SomfyFrame.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
namespace LocalUi {
constexpr uint8_t entityStatusRows=17;
// One stack projection for the current target. No model pointers/identities,
// credentials, rolling codes or commands are retained by the view.
struct EntityStatus {
  char room[21]{};
  float lift=-1,liftTarget=-1,tilt=-1,tiltTarget=-1,favourite=-1,tiltFavourite=-1;
  shade_types shadeType=shade_types::roller;
  tilt_types tiltType=tilt_types::none;
  radio_proto protocol=radio_proto::RTS;
  uint8_t bits=56,roomId=0,flags=0,members=0,missing=0,activeMembers=0;
  int8_t direction=0,tiltDirection=0;
  bool group=false,paired=false,sunSensor=false,radioReady=false,flip=false,unknownMotion=false;
};
static_assert(sizeof(EntityStatus)<=72,"Bounded temporary status projection");
inline bool entityStatusRow(const EntityStatus &s,uint8_t index,EntityAdmin::FormRow &row){
  row={};if(index>=entityStatusRows)return false;
  static const char *labels[]={"Room","Category","Protocol","Availability","Lift estimate","Lift target estimate","Tilt estimate","Tilt target estimate","Favourite lift","Favourite tilt","Estimated movement","Sun automation","Sunny","Windy","Members","Paired marker","Back"};
  snprintf(row.label,sizeof(row.label),"%s",labels[index]);
  auto text=[&](const char *value){snprintf(row.value,sizeof(row.value),"%s",value);};
  const bool rf=s.protocol==radio_proto::RTS||s.protocol==radio_proto::RTW||s.protocol==radio_proto::RTV;
  if(index>=4&&index<=9){
    if(s.group){text("Varies by member");return true;}
    const bool tilt=index==6||index==7||index==9;
    if(tilt&&s.tiltType==tilt_types::none){text("Not configured");return true;}
    if(!tilt&&(isToggleShade(s.shadeType)||s.shadeType==shade_types::drycontact||s.shadeType==shade_types::drycontact2||s.tiltType==tilt_types::tiltonly)){text("Not applicable");return true;}
    const float value=index==4?s.lift:index==5?s.liftTarget:index==6?s.tilt:index==7?s.tiltTarget:index==8?s.favourite:s.tiltFavourite;
    if(!isfinite(value)||value>100||value<0){text(index>=8&&isfinite(value)&&value==-1?"Unset":"Unknown");return true;}
    snprintf(row.value,sizeof(row.value),"%.1f%%",double(s.flip?100-value:value));return true;
  }
  switch(index){
    case 0:if(!s.roomId)text("Home / unassigned");else if(!s.room[0])snprintf(row.value,sizeof(row.value),"Missing #%u",unsigned(s.roomId));else snprintf(row.value,sizeof(row.value),"#%u %s",unsigned(s.roomId),s.room);break;
    case 1:{static const char *types[]={"Roller","Blind","Left curtain","Awning","Shutter","One-button garage","Garage","Right curtain","Centre curtain","Dry contact","Two-button contact","Left gate","Centre gate","Right gate","One-button left gate","One-button centre gate","One-button right gate"};text(s.group?"RF group":uint8_t(s.shadeType)<17?types[uint8_t(s.shadeType)]:"Unknown");break;}
    case 2:if(rf)snprintf(row.value,sizeof(row.value),"%s / %u-bit",s.protocol==radio_proto::RTS?"RTS":s.protocol==radio_proto::RTW?"RTW":"RTV",unsigned(s.bits));else text(s.protocol==radio_proto::GP_Relay?"GPIO relay":s.protocol==radio_proto::GP_Remote?"GPIO remote":"Unknown");break;
    case 3:text(rf?(s.radioReady?"Controller radio ready":"Controller radio unavailable"):"GPIO; see control availability");break;
    case 10:if(s.group){if(s.unknownMotion)text("Unknown; untracked members");else snprintf(row.value,sizeof(row.value),"%u active / %u present",unsigned(s.activeMembers),unsigned(s.members));break;}
      snprintf(row.value,sizeof(row.value),"Lift %s; tilt %s",s.direction<0?"up":s.direction>0?"down":"idle",s.tiltDirection<0?"up":s.tiltDirection>0?"down":"idle");break;
    case 11:text(!s.sunSensor?"Not configured":(s.flags&1)?(s.group?"Enabled for some members":"Enabled"):"Disabled");break;
    case 12:case 13:text(s.flags&(index==12?0x20:0x10)?(s.group?"Yes (any member)":"Yes"):"No");break;
    case 14:if(s.group)snprintf(row.value,sizeof(row.value),"%u present; %u missing",unsigned(s.members),unsigned(s.missing));else text("Not a group");break;
    case 15:text(s.group?"Not motor feedback":s.paired?"Paired (configured)":"Unpaired (configured)");break;
    default:break;
  }
  return true;
}
}
