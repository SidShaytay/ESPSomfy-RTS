#include "SomfyEditor.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
struct SomfyGroup {
  uint8_t linkedShades[SOMFY_MAX_GROUPED_SHADES]{};
  unsigned events=0;
  bool linkShade(uint8_t);
  bool unlinkShade(uint8_t);
  uint8_t getGroupId(){return 9;}
  void emitState(){++events;}
};
#include "production-group-list.inc"
struct Model {
  SomfyGroup group;
  unsigned commits=0;
  bool exists=true;
  ModelCommitState file=ModelCommitState::saved;
  SomfyGroup *getGroupById(uint8_t id){return exists && id==9?&group:nullptr;}
  void *getShadeById(uint8_t id){return id==7?this:nullptr;}
  ModelCommitResult commit(){++commits;return {file,ModelCommitStage::write};}
}somfy;
static edit_result_t done(uint8_t id,ModelCommitResult file){return edit_result_t::completed(id,file);}
static edit_result_t apply(model_edit_t &edit){
  JsonObject obj=edit.doc.as<JsonObject>();
  switch(edit.type){
#include "production-group-edit.inc"
  default:return edit_result_t::error("unsupported");
  }
}
static edit_result_t request(model_edit_type_t type,uint8_t id){
  model_edit_t edit;edit.type=type;edit.doc["groupId"]=9;edit.doc["shadeId"]=id;return apply(edit);
}
int main(){
  for(uint8_t staleId : {uint8_t(31),uint8_t(88)})
  for(auto file:{ModelCommitState::saved,ModelCommitState::failed,ModelCommitState::uncertain}){
    somfy={};somfy.file=file;somfy.group.linkedShades[0]=7;somfy.group.linkedShades[29]=staleId;somfy.group.linkedShades[31]=staleId;
    const auto result=request(model_edit_type_t::unlink_from_group,staleId);
    assert(result.applied);assert(result.ok==(file==ModelCommitState::saved));assert(result.persistence.state==file);
    assert(result.id==9 && result.shadeId==staleId && somfy.commits==1 && somfy.group.events==1);
    assert(somfy.group.linkedShades[0]==7);
    for(unsigned i=1;i<SOMFY_MAX_GROUPED_SHADES;i++)assert(somfy.group.linkedShades[i]==0);
  }
  somfy={};const auto refused=request(model_edit_type_t::link_to_group,31);assert(!refused.applied && refused.notFound && !somfy.commits);
  somfy={};somfy.exists=false;const auto missing=request(model_edit_type_t::unlink_from_group,31);assert(!missing.applied && !somfy.commits);
  somfy={};somfy.group.linkedShades[0]=7;const auto known=request(model_edit_type_t::unlink_from_group,7);assert(known.applied && !somfy.group.linkedShades[0] && somfy.commits==1);
  puts("Actual group metadata repair branch: 9 cases passed (no radio endpoint)");
}
