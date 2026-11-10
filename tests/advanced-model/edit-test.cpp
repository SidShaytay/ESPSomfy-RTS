#include "SomfyEditor.h"
#include "EntityFields.h"
#include "GroupMembership.h"
#include "BitLength.h"
#include "ModelLoadTrust.h"
#include "esp_log.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
struct SomfyShade {unsigned configs=0;void configChanged(){++configs;}};
struct SomfyRoom {};
struct SomfyGroup {
  uint8_t id=9,linkedShades[SOMFY_MAX_GROUPED_SHADES]{};
  unsigned events=0,configs=0;
  bool linkShade(uint8_t);bool unlinkShade(uint8_t);
  uint8_t getGroupId(){return id;}
  void emitState(){++events;}void configChanged(){++configs;}
};
#include "production-group-list.inc"
struct Model {
  SomfyGroup group;SomfyShade shade;
  ModelLoadTrust modelLoad{ModelLoadStatus::loaded};uint64_t configurationRevision=1;
  unsigned commits=0;bool exists=true;ModelCommitState file=ModelCommitState::saved;
  SomfyGroup *getGroupById(uint8_t id){return exists&&id==group.id?&group:nullptr;}
  SomfyShade *getShadeById(uint8_t id){return id==1||id==7?&shade:nullptr;}
  SomfyRoom *getRoomById(uint8_t){return nullptr;}
  ModelCommitResult commit(){++commits;return {file,ModelCommitStage::write};}
}somfy;
static unsigned replies=0,fileEvents=0;
static const char *TAG="Membership fixture";
struct Socket {
  void replyEdit(const proto_reply_to_t &,model_edit_type_t,const edit_result_t &){++replies;}
  void modelPersistence(ModelCommitReport,uint32_t){++fileEvents;}
  void room(SomfyRoom &){assert(false);}void order(sort_list_t){assert(false);}void repeaters(){assert(false);}
  void shadeRemoved(uint8_t){assert(false);}void groupRemoved(uint8_t){assert(false);}void roomRemoved(uint8_t){assert(false);}
}protoSocket;
#include "production-group-edit.inc"
#include "production-group-fanout.inc"
static edit_result_t request(model_edit_type_t type,uint8_t id){model_edit_t edit;edit.type=type;edit.doc["groupId"]=9;edit.doc["shadeId"]=id;return applyModelEdit(edit);}
static void reset(){somfy={};replies=fileEvents=0;}
static void classification_matches_actual_add_helper_for_every_physical_slot() {
  for(unsigned pattern=0;pattern<6;++pattern) {
    uint8_t slots[32]={};
    for(unsigned i=0;i<32;++i)slots[i]=pattern==0?0:pattern==1?255:pattern==2?7:pattern==3?uint8_t(100+i):pattern==4?(i==31?7:0):(i==31?0:255);
    const auto info=inspectGroupMembership(slots,7);
    const bool predicted=info.occurrences || info.freeSlots;
    const bool accepted=addToList(slots,32,uint8_t(7));assert(predicted==accepted);
  }
}
int main(){
  classification_matches_actual_add_helper_for_every_physical_slot();
  for(uint8_t staleId:{uint8_t(31),uint8_t(88),uint8_t(255)})
  for(auto file:{ModelCommitState::saved,ModelCommitState::failed,ModelCommitState::uncertain}){
    reset();somfy.file=file;somfy.group.linkedShades[0]=7;somfy.group.linkedShades[29]=staleId;somfy.group.linkedShades[31]=staleId;
    const auto result=request(model_edit_type_t::unlink_from_group,staleId);
    assert(result.applied&&result.ok==(file==ModelCommitState::saved)&&result.persistence.state==file);
    assert(result.id==9&&result.shadeId==staleId&&somfy.commits==1&&somfy.group.events==1);
    assert(somfy.configurationRevision==2&&somfy.group.configs==1&&replies==1&&fileEvents==1);
    assert(somfy.group.linkedShades[0]==7);for(unsigned i=1;i<SOMFY_MAX_GROUPED_SHADES;i++)assert(!somfy.group.linkedShades[i]);
  }
  for(auto type:{model_edit_type_t::link_to_group,model_edit_type_t::unlink_from_group}) {
    reset();if(type==model_edit_type_t::link_to_group)somfy.group.linkedShades[31]=7;
    const auto unchanged=request(type,7);
    assert(unchanged.ok&&unchanged.noChange&&!unchanged.applied&&!unchanged.durable());
    assert(somfy.configurationRevision==1&&!somfy.commits&&!somfy.group.events&&!somfy.group.configs&&!somfy.shade.configs&&replies==1&&!fileEvents);
    for(const char *field:{"groupId","shadeId"})for(const char *number:{"0","-1","257","18446744073709551615"}) {
      reset();somfy.group.id=!strcmp(field,"groupId")?1:9;somfy.group.linkedShades[0]=1;
      char input[100];snprintf(input,sizeof(input),!strcmp(field,"groupId")?"{\"groupId\":%s,\"shadeId\":1}":"{\"groupId\":9,\"shadeId\":%s}",number);
      model_edit_t edit;edit.type=type;assert(!deserializeJson(edit.doc,input));const auto rejected=applyModelEdit(edit);
      assert(!rejected.ok&&!rejected.applied&&!rejected.notFound&&somfy.configurationRevision==1&&!somfy.commits&&!somfy.group.events&&!somfy.group.configs&&!fileEvents&&replies==1);
    }
  }
  reset();memset(somfy.group.linkedShades,255,sizeof(somfy.group.linkedShades));
  const auto full=request(model_edit_type_t::link_to_group,7);assert(!full.ok&&!full.applied&&somfy.configurationRevision==1&&!somfy.commits&&!fileEvents);
  for(auto id:somfy.group.linkedShades)assert(id==255); // No silent placeholder cleanup during Add.
  reset();const auto refused=request(model_edit_type_t::link_to_group,31);assert(!refused.applied&&refused.notFound&&!somfy.commits&&somfy.configurationRevision==1);
  reset();somfy.exists=false;const auto missing=request(model_edit_type_t::unlink_from_group,31);assert(!missing.applied&&!somfy.commits&&somfy.configurationRevision==1);
  reset();somfy.group.linkedShades[0]=7;const auto known=request(model_edit_type_t::unlink_from_group,7);assert(known.applied&&!somfy.group.linkedShades[0]&&somfy.commits==1);
  puts("Actual group metadata mutation + complete shared fanout: exact bounds/history, idempotent success, all-slot repair and checked file outcomes passed (no radio endpoint)");
}
