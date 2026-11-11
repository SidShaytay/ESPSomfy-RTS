#include <unity.h>
#include "LocalUi.h"
#include <string.h>
using namespace LocalUi;
void setUp(){} void tearDown(){}
struct Source:CatalogueSource {
  CatalogueEntry devices[32]={},groups[16]={},rooms[16]={};
  bool read(CatalogueKind kind,uint8_t slot,CatalogueEntry &out)override {
    out=kind==CatalogueKind::rooms?rooms[slot]:kind==CatalogueKind::groups?groups[slot]:devices[slot];
    return out.id!=0;
  }
};
static void room_list_omits_all_rooms_and_empty_unassigned() {
  Source s;CataloguePage p;
  TEST_ASSERT_TRUE(roomCataloguePage(s,0,p));TEST_ASSERT_EQUAL(0,p.total);
  s.rooms[0]={8,0,0,"Bedroom"};
  TEST_ASSERT_TRUE(roomCataloguePage(s,0,p));TEST_ASSERT_EQUAL(1,p.total);TEST_ASSERT_EQUAL(8,p.rows[0].id);
  s.groups[0]={1,0,0,"Unassigned group"};
  TEST_ASSERT_TRUE(roomCataloguePage(s,0,p));TEST_ASSERT_EQUAL(2,p.total);
  TEST_ASSERT_EQUAL_STRING("Unassigned",p.rows[0].name);
}
static void catalogues_show_four_rows() {
  Source s;CataloguePage p;
  for(unsigned i=0;i<5;++i)s.devices[i]={uint8_t(i+1),0,int8_t(i),"Device"};
  TEST_ASSERT_TRUE(cataloguePage(s,CatalogueKind::devices,3,allRooms,p));
  TEST_ASSERT_EQUAL(4,p.count);TEST_ASSERT_EQUAL(0,p.offset);
}
struct Drive {
  Source source;Controller c;uint32_t now=0;Model model;
  Drive(){model.quickReady=model.ready=model.pageValid=model.authorized=true;model.authGeneration=2;model.authConfigurationGeneration=1;model.modelRevision=model.restoreRevision=1;}
  void tick(int turn=0,bool press=false,bool back=false){now+=50;const auto &v=c.view();model.catalogue=v.catalogue;model.room=v.room;
    model.pageValid=v.catalogue==CatalogueKind::rooms?roomCataloguePage(source,v.cursor,model.page,v.anchorId,v.anchored):cataloguePage(source,v.catalogue,v.cursor,v.room,model.page,v.anchorId,v.anchored,v.anchorGroup);
    c.tick(now,model,turn,press,true,back);}
  void click(){tick(0,true);tick(0,true);tick();tick();}
  void back(){tick(0,false,true);tick(0,false,true);tick();tick();}
};
static void home_has_two_destination_pages_and_back_remembers_origin(){
  Drive d;d.tick();for(unsigned i=0;i<4;i++)d.tick(1);d.click();
  TEST_ASSERT_EQUAL(Screen::catalogue,d.c.view().screen);TEST_ASSERT_EQUAL(CatalogueKind::devices,d.c.view().catalogue);
  d.back();TEST_ASSERT_EQUAL(Screen::quickHome,d.c.view().screen);TEST_ASSERT_EQUAL(4,d.c.view().selected);
  for(unsigned i=0;i<4;i++)d.tick(1);d.click();TEST_ASSERT_EQUAL(Screen::brightness,d.c.view().screen);
  d.back();TEST_ASSERT_EQUAL(Screen::quickHome,d.c.view().screen);TEST_ASSERT_EQUAL(8,d.c.view().selected);
}
static void mixed_room_contents_keep_group_identity_and_restore_room_selection(){
  Drive d;d.source.rooms[0]={8,0,0,"Bedroom"};d.source.devices[0]={7,8,0,"Device",700,1,0,false,true};d.source.groups[0]={7,8,1,"Group",800,1,0,false,true};
  d.tick();for(unsigned i=0;i<6;i++)d.tick(1);d.click();d.click();d.tick();
  TEST_ASSERT_EQUAL(Screen::catalogue,d.c.view().screen);TEST_ASSERT_EQUAL(CatalogueKind::roomContents,d.c.view().catalogue);
  TEST_ASSERT_EQUAL(2,d.model.page.total);d.tick(1);d.click();
  TEST_ASSERT_EQUAL(Screen::control,d.c.view().screen);TEST_ASSERT_EQUAL(EntityKind::group,d.c.view().entity.kind);TEST_ASSERT_EQUAL(7,d.c.view().entity.id);
  d.back();TEST_ASSERT_EQUAL(Screen::catalogue,d.c.view().screen);TEST_ASSERT_EQUAL(1,d.c.view().cursor);
  d.back();TEST_ASSERT_EQUAL(CatalogueKind::rooms,d.c.view().catalogue);TEST_ASSERT_EQUAL(0,d.c.view().cursor);
  d.back();TEST_ASSERT_EQUAL(Screen::quickHome,d.c.view().screen);TEST_ASSERT_EQUAL(6,d.c.view().selected);
}
static void full_mixed_catalogue_keeps_both_kind_anchors_and_rejects_duplicate_ids(){
  Source s;CataloguePage p;
  for(uint8_t i=0;i<32;++i)s.devices[i]={uint8_t(i+1),8,0,"Same",uint32_t(100+i),1};
  for(uint8_t i=0;i<16;++i)s.groups[i]={uint8_t(i+1),8,0,"Same",uint32_t(200+i),1};
  TEST_ASSERT_TRUE(cataloguePage(s,CatalogueKind::roomContents,0,8,p,7,true,true));
  TEST_ASSERT_EQUAL(48,p.total);TEST_ASSERT_EQUAL(38,p.cursor);TEST_ASSERT_EQUAL(36,p.offset);TEST_ASSERT_EQUAL(4,p.count);
  TEST_ASSERT_TRUE(p.rows[2].group);TEST_ASSERT_EQUAL(7,p.rows[2].id);TEST_ASSERT_EQUAL(206,p.rows[2].identity);
  TEST_ASSERT_TRUE(cataloguePage(s,CatalogueKind::roomContents,0,8,p,7,true,false));
  TEST_ASSERT_EQUAL(6,p.cursor);TEST_ASSERT_FALSE(p.rows[2].group);TEST_ASSERT_EQUAL(106,p.rows[2].identity);
  s.groups[1].id=1;TEST_ASSERT_FALSE(cataloguePage(s,CatalogueKind::roomContents,0,8,p));
  TEST_ASSERT_EQUAL(0,p.count);TEST_ASSERT_EQUAL(0,p.total);
}
static void final_room_page_restores_named_room_with_unassigned_prefix(){
  Source s;CataloguePage p;s.devices[0]={1,0,0,"Unassigned"};
  for(uint8_t i=0;i<16;++i)s.rooms[i]={uint8_t(i+1),0,int8_t(i),"Room"};
  TEST_ASSERT_TRUE(roomCataloguePage(s,0,p,16,true));TEST_ASSERT_EQUAL(17,p.total);TEST_ASSERT_EQUAL(16,p.cursor);
  TEST_ASSERT_EQUAL(16,p.offset);TEST_ASSERT_EQUAL(1,p.count);TEST_ASSERT_EQUAL(16,p.rows[0].id);
}
static void network_shortcut_restores_each_child_and_exact_home_origin(){
  Drive d;d.tick();for(unsigned i=0;i<9;++i)d.tick(1);d.click();
  TEST_ASSERT_EQUAL(Screen::networkMenu,d.c.view().screen);TEST_ASSERT_TRUE(d.c.view().networkHome);
  d.click();TEST_ASSERT_EQUAL(Screen::settings,d.c.view().screen);d.c.closeForm();
  TEST_ASSERT_EQUAL(Screen::networkMenu,d.c.view().screen);TEST_ASSERT_EQUAL(0,d.c.view().selected);
  d.tick(1);d.click();TEST_ASSERT_EQUAL(Screen::system,d.c.view().screen);TEST_ASSERT_EQUAL(3,d.c.view().systemSection);d.c.closeForm();
  TEST_ASSERT_EQUAL(Screen::networkMenu,d.c.view().screen);TEST_ASSERT_EQUAL(1,d.c.view().selected);
  d.back();TEST_ASSERT_EQUAL(Screen::quickHome,d.c.view().screen);TEST_ASSERT_EQUAL(9,d.c.view().selected);
  TEST_ASSERT_FALSE(d.c.view().networkHome);TEST_ASSERT_FALSE(d.c.view().routed);
  d.tick(1);d.click();TEST_ASSERT_EQUAL(Screen::system,d.c.view().screen);TEST_ASSERT_EQUAL(4,d.c.view().systemSection);d.c.closeForm();
  TEST_ASSERT_EQUAL(Screen::quickHome,d.c.view().screen);TEST_ASSERT_EQUAL(10,d.c.view().selected);
  d.tick(1);d.click();TEST_ASSERT_EQUAL(Screen::system,d.c.view().screen);TEST_ASSERT_EQUAL(2,d.c.view().systemSection);d.c.closeForm();
  TEST_ASSERT_EQUAL(Screen::quickHome,d.c.view().screen);TEST_ASSERT_EQUAL(11,d.c.view().selected);
}
static void second_read_identity_or_revision_change_discards_entire_page(){
  struct ChangingSource:Source {
    unsigned reads=0;bool revision=false;
    bool read(CatalogueKind kind,uint8_t slot,CatalogueEntry &out)override {
      const bool found=Source::read(kind,slot,out);
      if(kind==CatalogueKind::devices && !slot && ++reads==2) {
        if(revision)++out.revision;else ++out.identity;
      }
      return found;
    }
  };
  for(unsigned change=0;change<2;++change) {
    ChangingSource s;s.revision=change;s.devices[0]={1,8,0,"Device",100,1};CataloguePage p;
    TEST_ASSERT_FALSE(cataloguePage(s,CatalogueKind::roomContents,0,8,p));
    TEST_ASSERT_EQUAL(0,p.total);TEST_ASSERT_EQUAL(0,p.count);TEST_ASSERT_EQUAL(0,p.rows[0].id);
  }
}
static void unseen_home_shortcuts_and_unlock_cannot_activate_destinations(){
  Drive d;d.tick();for(unsigned i=0;i<10;++i)d.tick(1);
  for(unsigned phase=0;phase<4;++phase){d.now+=50;d.c.tick(d.now,d.model,0,phase<2,false);}
  TEST_ASSERT_EQUAL(Screen::quickHome,d.c.view().screen);TEST_ASSERT_EQUAL(10,d.c.view().selected);
  d.model.authorized=false;d.model.authMode=AuthMode::pin;d.click();
  TEST_ASSERT_EQUAL(Screen::unlock,d.c.view().screen);d.c.unlockResult(d.now,AuthResult::unlocked);
  d.model.authorized=true;d.tick();TEST_ASSERT_EQUAL(Screen::quickHome,d.c.view().screen);TEST_ASSERT_EQUAL(10,d.c.view().selected);
  FormInput input;Command command;TEST_ASSERT_FALSE(d.c.takeFormInput(input));TEST_ASSERT_FALSE(d.c.take(command));
  d.click();TEST_ASSERT_EQUAL(Screen::system,d.c.view().screen);TEST_ASSERT_EQUAL(4,d.c.view().systemSection);
}
int main(){UNITY_BEGIN();RUN_TEST(room_list_omits_all_rooms_and_empty_unassigned);RUN_TEST(catalogues_show_four_rows);RUN_TEST(home_has_two_destination_pages_and_back_remembers_origin);RUN_TEST(mixed_room_contents_keep_group_identity_and_restore_room_selection);RUN_TEST(full_mixed_catalogue_keeps_both_kind_anchors_and_rejects_duplicate_ids);RUN_TEST(final_room_page_restores_named_room_with_unassigned_prefix);RUN_TEST(network_shortcut_restores_each_child_and_exact_home_origin);RUN_TEST(second_read_identity_or_revision_change_discards_entire_page);RUN_TEST(unseen_home_shortcuts_and_unlock_cannot_activate_destinations);return UNITY_END();}
