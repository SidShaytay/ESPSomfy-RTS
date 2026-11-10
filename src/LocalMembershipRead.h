#pragma once
#include "LocalAdvanced.h"
#include "GroupMembership.h"
#include "EntityFields.h"
#include <stdio.h>
#include <string.h>
namespace LocalUi {
namespace Advanced {
// Loop-only model reads. No authority grant, I/O, allocation, availability
// filter, protocol matching rule or commands. Cache physical slots once; label
// projection never rescans the device catalogue for each displayed row.
template<typename Model> class MembershipReader {
  Model &model;
  auto group(uint8_t id,uint8_t slot) -> decltype(model.getGroupById(id)) {
    return slot<SOMFY_MAX_GROUPS&&model.groups[slot].getGroupId()==id?&model.groups[slot]:nullptr;
  }
  bool row(uint8_t groupId,bool adding,const MembershipRows &rows,uint8_t index) {
    if(rows.count>32||index>=rows.count)return false;
    auto *g=group(groupId,rows.groupSlot);if(!g)return false;
    const uint8_t id=rows.ids[index],device=rows.devices[index];
    if(adding){
      if(!id||id>SOMFY_MAX_SHADES||rows.slots[index]!=255||device>=SOMFY_MAX_SHADES||model.shades[device].getShadeId()!=id)return false;
      return !inspectGroupMembership(g->linkedShades,id).occurrences;
    }
    if(rows.slots[index]>=32||g->linkedShades[rows.slots[index]]!=id)return false;
    if(!id||id==255)return device==255;
    return device<SOMFY_MAX_SHADES?model.shades[device].getShadeId()==id:device==255&&!model.getShadeById(id);
  }
  const char *protocol(uint8_t value,char (&unknown)[12]) {
    const auto *field=entityField(model_edit_type_t::save_shade,proto_field_t::proto);
    for(uint8_t i=0;i<field->choiceCount;i++)if(field->choices[i].value==value)return field->choices[i].label;
    snprintf(unknown,sizeof(unknown),"Unknown%u",unsigned(value));return unknown;
  }
  template<typename Remote> void identity(const char *kind,uint8_t id,Remote &remote,char (&out)[48]) {
    char unknown[12],bits[12];
    snprintf(bits,sizeof(bits),remote.bitLength?"%ub":"auto%ub",unsigned(remote.bitLength?remote.bitLength:model.transceiver.config.type));
    snprintf(out,sizeof(out),"%s#%u %lu %s %s",kind,unsigned(id),static_cast<unsigned long>(remote.getRemoteAddress()),protocol(uint8_t(remote.proto),unknown),bits);
  }
  template<typename Shade> void labelName(const char *prefix,uint8_t id,Shade &shade,char (&out)[32]) {
    if(!memchr(shade.name,0,sizeof(shade.name)))snprintf(out,sizeof(out),"%s#%u Invalid name",prefix,unsigned(id));
    else snprintf(out,sizeof(out),"%s#%u %s",prefix,unsigned(id),shade.name);
  }
public:
  explicit MembershipReader(Model &m):model(m){}
  bool read(uint8_t groupId,bool adding,EntityAdmin::Backend &catalogue,MembershipRows &out) {
    out={};memset(out.devices,255,sizeof(out.devices));memset(out.slots,255,sizeof(out.slots));
    auto *g=model.getGroupById(groupId);if(!g)return false;
    for(uint8_t slot=0;slot<SOMFY_MAX_GROUPS;slot++)if(&model.groups[slot]==g)out.groupSlot=slot;
    if(out.groupSlot==255)return false;
    out.full=!inspectGroupMembership(g->linkedShades,uint8_t(0)).freeSlots;
    if(!adding){
      out.count=32;
      for(uint8_t i=0;i<32;i++){
        out.ids[i]=g->linkedShades[i];out.slots[i]=i;
        if(!out.ids[i]||out.ids[i]==255)continue;
        for(uint8_t device=0;device<SOMFY_MAX_SHADES;device++)if(model.shades[device].getShadeId()==out.ids[i]){out.devices[i]=device;break;}
      }
      return true;
    }
    EntityAdmin::OrderIds ids{},devices{};uint8_t count=0;bool normalization=false;
    if(!catalogue.readOrder(EntityAdmin::Kind::shade,ids,devices,count,normalization)||count>32)return false;
    for(uint8_t i=0;i<count;i++){
      if(!ids[i]||ids[i]>SOMFY_MAX_SHADES||devices[i]>=SOMFY_MAX_SHADES||model.shades[devices[i]].getShadeId()!=ids[i])return false;
      if(inspectGroupMembership(g->linkedShades,ids[i]).occurrences)continue;
      const uint8_t at=out.count++;out.ids[at]=ids[i];out.devices[at]=devices[i];
    }
    return true;
  }
  bool label(uint8_t groupId,bool adding,const MembershipRows &rows,uint8_t index,EntityAdmin::Choice &out,char (&detail)[48]) {
    out={};detail[0]=0;if(!row(groupId,adding,rows,index))return false;
    const uint8_t id=rows.ids[index],device=rows.devices[index];out.value=id;
    char prefix[12];snprintf(prefix,sizeof(prefix),adding?"":"Slot%u ",unsigned(rows.slots[index])+1);
    if(!id){snprintf(out.label,sizeof(out.label),"%sFree",prefix);strcpy(detail,"Read only");}
    else if(id==255){snprintf(out.label,sizeof(out.label),"%s#255 Unused",prefix);strcpy(detail,"Explicit cleanup; no device");}
    else if(device==255){snprintf(out.label,sizeof(out.label),"%s#%u Missing",prefix,unsigned(id));strcpy(detail,"Controller reference only");}
    else {
      auto &shade=model.shades[device];labelName(prefix,id,shade,out.label);
      char unknown[12];snprintf(detail,sizeof(detail),"%s; marker %s",protocol(uint8_t(shade.proto),unknown),shade.paired?"paired":"unpaired");
    }
    return true;
  }
  bool select(uint8_t groupId,bool adding,const MembershipRows &rows,uint8_t index,MembershipSelection &out) {
    out={};if(!row(groupId,adding,rows,index)||!rows.ids[index])return false;
    out.id=rows.ids[index];out.slot=rows.slots[index];out.deviceSlot=rows.devices[index];out.groupSlot=rows.groupSlot;out.adding=adding;
    auto *g=group(groupId,rows.groupSlot);out.occurrences=inspectGroupMembership(g->linkedShades,out.id).occurrences;
    EntityAdmin::Choice name;char detail[48];if(!label(groupId,adding,rows,index,name,detail))return false;
    memcpy(out.label,name.label,sizeof(out.label));identity("Group",groupId,*g,out.groupIdentity);
    if(out.id==255){strcpy(out.deviceIdentity,"Unused placeholder #255");return true;}
    out.hasCandidate=true;
    if(!readRawTargetContext(model,{EntityKind::device,out.id},out.candidate))return false;
    if(out.candidate.targetExists)identity("Dev",out.id,model.shades[out.deviceSlot],out.deviceIdentity);
    else snprintf(out.deviceIdentity,sizeof(out.deviceIdentity),"Device #%u missing",unsigned(out.id));
    return true;
  }
  bool current(uint8_t groupId,const MembershipSelection &selected) {
    auto *g=group(groupId,selected.groupSlot);if(!g||!selected.id)return false;
    if(selected.adding){
      if(selected.id>SOMFY_MAX_SHADES||selected.slot!=255||inspectGroupMembership(g->linkedShades,selected.id).occurrences)return false;
    }else if(selected.slot>=32||g->linkedShades[selected.slot]!=selected.id)return false;
    if(selected.id==255)return !selected.adding&&!selected.hasCandidate&&!selected.candidate.ref.id;
    if(!selected.hasCandidate)return false;
    RawTargetContext now;
    if(!readRawTargetContext(model,{EntityKind::device,selected.id},now)||!selected.candidate.same(now))return false;
    if(selected.adding&&!now.targetExists)return false;
    return now.targetExists?selected.deviceSlot<SOMFY_MAX_SHADES&&model.shades[selected.deviceSlot].getShadeId()==selected.id:selected.deviceSlot==255;
  }
};
} // namespace Advanced
} // namespace LocalUi
