#include <unity.h>
#include "LocalAdministration.h"
#include <stdlib.h>
#include <string.h>
using namespace LocalUi;
using namespace LocalUi::EntityAdmin;
struct Memory : ArduinoJson::Allocator {
  unsigned active=0;
  void *allocate(size_t n) override {void *p=malloc(n);if(p)++active;return p;}
  void deallocate(void *p) override {if(p){--active;free(p);}}
  void *reallocate(void *p,size_t n) override {return realloc(p,n);}
};
struct State : Backend,CatalogueSource {
  Authorization auth{true,1,1};uint64_t generation=1;
  CatalogueEntry rooms[16]={{4,0,0,"Office",0,1},{12,0,1,"Office",0,1}};
  CatalogueEntry devices[32]={{4,0,0,"Office",104,1},{27,0,1,"Office",127,1}};
  CatalogueEntry groups[16]={{4,0,0,"Office",504,1},{16,0,1,"Office",516,1}};
  unsigned opens=0,applies=0,commits=0;
  Kind preparedKind=Kind::room,appliedKind=Kind::room;
  ModelCommitState outcome=ModelCommitState::saved;
  CatalogueEntry *entries(Kind kind){return kind==Kind::room?rooms:kind==Kind::shade?devices:groups;}
  unsigned capacity(Kind kind){return kind==Kind::shade?32:16;}
  Authorization authorization() override {return auth;}
  uint64_t revision() override {return generation;}
  bool read(CatalogueKind kind,uint8_t slot,CatalogueEntry &out) override {
    const auto entityKind=kind==CatalogueKind::rooms?Kind::room:kind==CatalogueKind::devices?Kind::shade:Kind::group;
    TEST_ASSERT_TRUE(slot<capacity(entityKind));out=entries(entityKind)[slot];return out.id;
  }
  bool prepare(Kind kind,uint8_t id,uint8_t &offered) override {++opens;preparedKind=kind;offered=id?id:9;return true;}
  bool read(Kind kind,uint8_t id,proto_field_t field,Value &out) override {
    out={};out.kind=protoField(field).kind;
    const CatalogueEntry *item=nullptr;for(unsigned i=0;i<capacity(kind);i++)if(entries(kind)[i].id==id&&id)item=&entries(kind)[i];
    if(id&&!item)return false;
    if(field==proto_field_t::name){if(item)strcpy(out.text,item->name);}
    else if(field==proto_field_t::sortOrder)out.integer=item?item->sortOrder:2;
    else if(field==proto_field_t::remoteAddress)out.integer=(kind==Kind::shade?100:500)+(id?id:9);
    else if(field==proto_field_t::bitLength)out.integer=56;
    else if(field==proto_field_t::gpioUp||field==proto_field_t::gpioDown||field==proto_field_t::gpioMy)out.integer=255;
    return true;
  }
  ApplyResult apply(model_edit_type_t type,JsonDocument &doc,Authorization,uint64_t) override {
    ++applies;++generation;
    appliedKind=(type==model_edit_type_t::add_room||type==model_edit_type_t::save_room||type==model_edit_type_t::delete_room)?Kind::room:
      (type==model_edit_type_t::add_shade||type==model_edit_type_t::save_shade||type==model_edit_type_t::delete_shade)?Kind::shade:Kind::group;
    const bool adding=type==model_edit_type_t::add_room||type==model_edit_type_t::add_shade||type==model_edit_type_t::add_group;
    const bool deleting=type==model_edit_type_t::delete_room||type==model_edit_type_t::delete_shade||type==model_edit_type_t::delete_group;
    const char *key=appliedKind==Kind::room?"roomId":appliedKind==Kind::shade?"shadeId":"groupId";
    const uint8_t id=adding?9:doc[key].as<uint8_t>();
    for(unsigned i=0;i<capacity(appliedKind);i++){
      auto &item=entries(appliedKind)[i];
      if(adding&&!item.id){item={9,0,2,"New item",0,1};break;}
      if(deleting&&item.id==id){item={};break;}
    }
    return {edit_result_t::completed(id,{outcome,ModelCommitStage::none})};
  }
  CommitResult retryFileCommit(Authorization,uint64_t) override {++commits;return {{ModelCommitState::saved,ModelCommitStage::none}};}
  void activity() override {}
};
void setUp(){}void tearDown(){}
static void stable_room_ids_and_new_drafts_need_explicit_selection() {
  State state;Memory memory;Administration ui(state,state,memory);
  TEST_ASSERT_TRUE(ui.open(0));TEST_ASSERT_EQUAL_STRING("Administration",ui.view().title);
  ui.activate(1);TEST_ASSERT_EQUAL_STRING("Rooms",ui.view().title);
  TEST_ASSERT_EQUAL(3,ui.view().total);TEST_ASSERT_EQUAL_STRING("Add room",ui.view().rows[0].label);
  ui.turn(1,2);TEST_ASSERT_EQUAL(4,ui.view().id);ui.activate(3);
  TEST_ASSERT_EQUAL_STRING("Edit Room 4",ui.view().title);TEST_ASSERT_EQUAL(1,state.opens);
  TEST_ASSERT_EQUAL(0,state.applies);ui.back(4);
  TEST_ASSERT_EQUAL_STRING("Rooms",ui.view().title);TEST_ASSERT_EQUAL(4,ui.view().id);
  ui.back(5);TEST_ASSERT_EQUAL_STRING("Administration",ui.view().title);ui.back(6);
  TEST_ASSERT_TRUE(ui.closed());TEST_ASSERT_EQUAL(0,memory.active);
}
static void changed_list_never_retargets_a_press_and_retains_surviving_anchor() {
  State state;Memory memory;Administration ui(state,state,memory);ui.open(0);ui.activate(1);ui.turn(1,2);
  strcpy(state.rooms[0].name,"Renamed");++state.generation;ui.tick(3);
  TEST_ASSERT_EQUAL(4,ui.view().id);TEST_ASSERT_EQUAL(state.generation,ui.epoch());
  state.rooms[0]={};++state.generation;ui.activate(4);
  TEST_ASSERT_EQUAL(0,state.opens);TEST_ASSERT_EQUAL(0,ui.view().id);
  TEST_ASSERT_EQUAL_STRING("Rooms",ui.view().title);
  ui.activate(5);TEST_ASSERT_EQUAL_STRING("Add Room 9",ui.view().title);
  TEST_ASSERT_EQUAL(0,state.applies);
}
static void applied_add_adopts_id_and_returns_to_list_without_replaying() {
  State state;Memory memory;Administration ui(state,state,memory);ui.open(0);ui.activate(1);ui.activate(2);
  ui.turn(1,3);ui.turn(1,3);state.outcome=ModelCommitState::failed;ui.activate(4);
  TEST_ASSERT_EQUAL(Mode::result,ui.view().mode);TEST_ASSERT_EQUAL(9,ui.view().id);
  TEST_ASSERT_EQUAL(EntityAdmin::Notice::appliedNotSaved,ui.view().notice);TEST_ASSERT_EQUAL(1,state.applies);
  ui.turn(1,5);ui.activate(6);TEST_ASSERT_EQUAL(1,state.commits);TEST_ASSERT_EQUAL(1,state.applies);
  ui.activate(7);TEST_ASSERT_EQUAL_STRING("Rooms",ui.view().title);TEST_ASSERT_EQUAL(9,ui.view().id);
  TEST_ASSERT_EQUAL(0,memory.active);
}
static void authority_history_and_clock_loss_close_and_discard() {
  for(unsigned which=0;which<4;which++) {
    State state;Memory memory;Administration ui(state,state,memory);ui.open(10);ui.activate(11);ui.activate(12);
    if(which==0)state.auth.allowed=false;
    if(which==1)++state.auth.generation;
    if(which==2)++state.auth.configurationGeneration;
    ui.tick(which==3?9:13);
    TEST_ASSERT_EQUAL(Mode::closed,ui.view().mode);TEST_ASSERT_EQUAL(0,state.applies);
    ui.discard();TEST_ASSERT_TRUE(ui.closed());TEST_ASSERT_EQUAL(0,memory.active);
  }
}
static void clock_high_water_survives_return_from_a_child_form() {
  State state;Memory memory;Administration ui(state,state,memory);
  ui.open(10);ui.activate(11);ui.activate(12);ui.tick(1000);ui.back(1001);
  TEST_ASSERT_EQUAL_STRING("Rooms",ui.view().title);
  ui.activate(500);
  TEST_ASSERT_TRUE(ui.closed());TEST_ASSERT_EQUAL(EntityAdmin::Notice::expired,ui.view().notice);
  TEST_ASSERT_EQUAL(1,state.opens);TEST_ASSERT_EQUAL(0,state.applies);
}
static void all_three_kinds_keep_their_own_ids_and_return_selection() {
  State state;Memory memory;Administration ui(state,state,memory);ui.open(0);
  TEST_ASSERT_EQUAL(3,ui.view().total);
  TEST_ASSERT_EQUAL_STRING("Devices",ui.view().rows[1].label);
  TEST_ASSERT_EQUAL_STRING("Groups",ui.view().rows[2].label);
  ui.turn(1,1);ui.activate(1);
  TEST_ASSERT_EQUAL_STRING("Devices",ui.view().title);
  TEST_ASSERT_EQUAL_STRING("Add device",ui.view().rows[0].label);
  ui.turn(1,1);ui.activate(1);
  TEST_ASSERT_EQUAL(Kind::shade,state.preparedKind);
  TEST_ASSERT_EQUAL_STRING("Edit Shade 4",ui.view().title);
  TEST_ASSERT_EQUAL(24,ui.view().total);
  ui.back(1);TEST_ASSERT_EQUAL(4,ui.view().id);
  ui.back(1);TEST_ASSERT_EQUAL(1,ui.view().selected);
  ui.turn(1,1);ui.activate(1);ui.turn(1,1);ui.turn(1,1);ui.activate(1);
  TEST_ASSERT_EQUAL(Kind::group,state.preparedKind);
  TEST_ASSERT_EQUAL_STRING("Edit Group 16",ui.view().title);
  TEST_ASSERT_EQUAL(10,ui.view().total);
  TEST_ASSERT_EQUAL(0,state.applies);ui.discard();TEST_ASSERT_EQUAL(0,memory.active);
}
static void device_and_group_add_confirm_delete_and_retry_keep_kind_and_id() {
  for(unsigned type=1;type<3;type++) {
    State state;Memory memory;Administration ui(state,state,memory);ui.open(0);
    for(unsigned i=0;i<type;i++)ui.turn(1,1);
    ui.activate(1);ui.activate(1);
    const unsigned fields=type==1?21:7;
    for(unsigned i=0;i<fields;i++)ui.turn(1,1);
    ui.activate(1);TEST_ASSERT_EQUAL(Mode::confirm,ui.view().mode);
    TEST_ASSERT_EQUAL(0,state.applies);ui.activate(1); // Default Cancel.
    ui.activate(1);ui.turn(1,1);state.outcome=ModelCommitState::failed;ui.activate(1);
    TEST_ASSERT_EQUAL(1,state.applies);TEST_ASSERT_EQUAL(9,ui.view().id);
    TEST_ASSERT_EQUAL(type==1?Kind::shade:Kind::group,state.appliedKind);
    ui.turn(1,1);ui.activate(1);TEST_ASSERT_EQUAL(1,state.commits);TEST_ASSERT_EQUAL(1,state.applies);
    ui.activate(1);TEST_ASSERT_EQUAL(9,ui.view().id);
    ui.activate(1);for(unsigned i=0;i<fields+2;i++)ui.turn(1,1);
    ui.activate(1);ui.activate(1);TEST_ASSERT_EQUAL(1,state.applies); // Delete Cancel.
    ui.activate(1);ui.turn(1,1);state.outcome=ModelCommitState::saved;ui.activate(1);
    TEST_ASSERT_EQUAL(2,state.applies);ui.activate(1);TEST_ASSERT_EQUAL(0,ui.view().id);
    TEST_ASSERT_EQUAL(4,state.rooms[0].id);ui.discard();TEST_ASSERT_EQUAL(0,memory.active);
  }
}
static void every_device_is_paged_and_full_add_never_opens_a_draft() {
  for(unsigned type=0;type<3;type++) {
    State state;Memory memory;const auto kind=type==0?Kind::room:type==1?Kind::shade:Kind::group;
    for(unsigned i=0;i<state.capacity(kind);i++)state.entries(kind)[i]={uint8_t(i+1),0,int8_t(i),"Same",0,1};
    Administration ui(state,state,memory);ui.open(0);for(unsigned i=0;i<type;i++)ui.turn(1,1);ui.activate(1);
    ui.activate(1);TEST_ASSERT_EQUAL(0,state.opens);TEST_ASSERT_NOT_NULL(strstr(ui.view().rows[0].label,"full"));
    const auto count=state.capacity(kind);for(unsigned i=0;i<count;i++)ui.turn(1,1);
    TEST_ASSERT_EQUAL(count,ui.view().id);ui.activate(1);TEST_ASSERT_EQUAL(1,state.opens);
    TEST_ASSERT_EQUAL(kind,state.preparedKind);TEST_ASSERT_EQUAL(count,ui.view().id);
    ui.discard();TEST_ASSERT_EQUAL(0,state.applies);TEST_ASSERT_EQUAL(0,memory.active);
  }
}
static void device_history_change_cannot_turn_a_missing_id_into_a_group_or_add() {
  State state;Memory memory;Administration ui(state,state,memory);ui.open(0);ui.turn(1,1);ui.activate(1);
  ui.turn(1,1);ui.turn(1,1);TEST_ASSERT_EQUAL(27,ui.view().id);
  state.devices[1]={};++state.generation;ui.activate(1);
  TEST_ASSERT_EQUAL(0,state.opens);TEST_ASSERT_EQUAL(0,ui.view().id);
  TEST_ASSERT_EQUAL_STRING("Devices",ui.view().title);
  ui.activate(1);TEST_ASSERT_EQUAL(Kind::shade,state.preparedKind);
  TEST_ASSERT_EQUAL_STRING("Add Shade 9",ui.view().title);TEST_ASSERT_EQUAL(0,state.applies);
}
int main(){UNITY_BEGIN();
RUN_TEST(all_three_kinds_keep_their_own_ids_and_return_selection);
RUN_TEST(device_and_group_add_confirm_delete_and_retry_keep_kind_and_id);
RUN_TEST(every_device_is_paged_and_full_add_never_opens_a_draft);
RUN_TEST(device_history_change_cannot_turn_a_missing_id_into_a_group_or_add);
RUN_TEST(stable_room_ids_and_new_drafts_need_explicit_selection);RUN_TEST(changed_list_never_retargets_a_press_and_retains_surviving_anchor);RUN_TEST(applied_add_adopts_id_and_returns_to_list_without_replaying);RUN_TEST(authority_history_and_clock_loss_close_and_discard);RUN_TEST(clock_high_water_survives_return_from_a_child_form);return UNITY_END();}
