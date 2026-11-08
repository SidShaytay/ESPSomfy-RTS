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
  unsigned opens=0,applies=0,commits=0;
  ModelCommitState outcome=ModelCommitState::saved;
  Authorization authorization() override {return auth;}
  uint64_t revision() override {return generation;}
  bool read(CatalogueKind kind,uint8_t slot,CatalogueEntry &out) override {out={};if(kind!=CatalogueKind::rooms)return false;out=rooms[slot];return out.id;}
  bool prepare(Kind kind,uint8_t id,uint8_t &offered) override {++opens;offered=id?id:9;return kind==Kind::room;}
  bool read(Kind,uint8_t id,proto_field_t field,Value &out) override {
    out={};out.kind=protoField(field).kind;
    const CatalogueEntry *room=nullptr;for(const auto &r:rooms)if(r.id==id&&id)room=&r;
    if(id&&!room)return false;
    if(field==proto_field_t::name){if(room)strcpy(out.text,room->name);return true;}
    if(field==proto_field_t::sortOrder){out.integer=room?room->sortOrder:2;return true;}
    return false;
  }
  ApplyResult apply(model_edit_type_t type,JsonDocument &doc,Authorization,uint64_t) override {
    ++applies;++generation;
    const uint8_t id=type==model_edit_type_t::add_room?9:doc["roomId"].as<uint8_t>();
    if(type==model_edit_type_t::add_room)rooms[2]={9,0,2,"New room",0,1};
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
int main(){UNITY_BEGIN();RUN_TEST(stable_room_ids_and_new_drafts_need_explicit_selection);RUN_TEST(changed_list_never_retargets_a_press_and_retains_surviving_anchor);RUN_TEST(applied_add_adopts_id_and_returns_to_list_without_replaying);RUN_TEST(authority_history_and_clock_loss_close_and_discard);RUN_TEST(clock_high_water_survives_return_from_a_child_form);return UNITY_END();}
