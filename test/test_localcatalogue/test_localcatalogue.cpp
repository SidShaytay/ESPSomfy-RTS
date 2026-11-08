#include <unity.h>
#include "LocalCatalogue.h"
#include <string.h>
using namespace LocalUi;
void setUp() {}
void tearDown() {}
struct Source : CatalogueSource {
  CatalogueEntry entries[SOMFY_MAX_SHADES] = {};
  unsigned reads = 0;
  bool read(CatalogueKind, uint8_t slot, CatalogueEntry &out) override {
    ++reads;
    out = entries[slot];
    return out.id != 0;
  }
};
static void empty_and_full_pages_are_bounded() {
  Source source;
  CataloguePage page;
  TEST_ASSERT_TRUE(cataloguePage(source, CatalogueKind::devices, 0, 255, page));
  TEST_ASSERT_EQUAL(0, page.total);
  TEST_ASSERT_EQUAL(0, page.count);
  TEST_ASSERT_EQUAL(SOMFY_MAX_SHADES, source.reads);
  for(uint8_t i=0;i<SOMFY_MAX_SHADES;i++) {
    source.entries[i].id=i+1;
    source.entries[i].sortOrder=i;
    strcpy(source.entries[i].name,"Repeated name");
  }
  TEST_ASSERT_TRUE(cataloguePage(source, CatalogueKind::devices, 255, 255, page));
  TEST_ASSERT_EQUAL(SOMFY_MAX_SHADES, page.total);
  TEST_ASSERT_EQUAL(30, page.offset);
  TEST_ASSERT_EQUAL(2, page.count);
  TEST_ASSERT_EQUAL(31, page.rows[0].id);
  TEST_ASSERT_EQUAL(32, page.rows[1].id);
  TEST_ASSERT_EQUAL(0, page.rows[2].id);
}
static void sparse_sorted_ids_and_room_filter_are_stable() {
  Source source;
  source.entries[3]={19, 2, -4, "Same name"};
  source.entries[12]={7, 2, -4, "Same name"};
  source.entries[30]={3, 0, -5, "Unassigned"};
  CataloguePage page;
  TEST_ASSERT_TRUE(cataloguePage(source,CatalogueKind::devices,0,2,page));
  TEST_ASSERT_EQUAL(2,page.total);
  TEST_ASSERT_EQUAL(7,page.rows[0].id);
  TEST_ASSERT_EQUAL(19,page.rows[1].id);
  TEST_ASSERT_TRUE(cataloguePage(source,CatalogueKind::devices,0,0,page));
  TEST_ASSERT_EQUAL(1,page.total);
  TEST_ASSERT_EQUAL(3,page.rows[0].id);
  source.entries[30]={};
  TEST_ASSERT_TRUE(cataloguePage(source,CatalogueKind::devices,0,0,page));
  TEST_ASSERT_EQUAL(0,page.count);
  TEST_ASSERT_EQUAL(0,page.rows[0].id);
}
static void original_utf8_names_survive_without_becoming_identity() {
  Source source;
  source.entries[2]={12,0,0,"\xc3\xa9\xe5\xb8\x98"};
  CataloguePage page;
  TEST_ASSERT_TRUE(cataloguePage(source,CatalogueKind::devices,0,255,page));
  TEST_ASSERT_EQUAL_STRING("\xc3\xa9\xe5\xb8\x98",page.rows[0].name);
  TEST_ASSERT_EQUAL(12,page.rows[0].id);
}
static void malformed_catalogues_fail_without_old_rows() {
  Source source;
  source.entries[0]={1,0,0,"First"};
  CataloguePage page;
  TEST_ASSERT_TRUE(cataloguePage(source,CatalogueKind::devices,0,255,page));
  source.entries[2]={1,0,1,"Duplicate ID"};
  TEST_ASSERT_FALSE(cataloguePage(source,CatalogueKind::devices,0,255,page));
  TEST_ASSERT_EQUAL(0,page.count);
  TEST_ASSERT_EQUAL(0,page.rows[0].id);
  source.entries[2]={255,0,1,"Invalid ID"};
  TEST_ASSERT_FALSE(cataloguePage(source,CatalogueKind::devices,0,255,page));
  source.entries[2]={2,0,1,"No terminator"};
  memset(source.entries[2].name,'x',sizeof(source.entries[2].name));
  TEST_ASSERT_FALSE(cataloguePage(source,CatalogueKind::devices,0,255,page));
  TEST_ASSERT_FALSE(cataloguePage(source,CatalogueKind(99),0,255,page));
}
static void group_and_room_limits_do_not_read_device_capacity() {
  Source source;
  for(uint8_t i=0;i<SOMFY_MAX_SHADES;i++) source.entries[i]={uint8_t(i+1),0,int8_t(i),"Name"};
  CataloguePage page;
  TEST_ASSERT_TRUE(cataloguePage(source,CatalogueKind::groups,0,255,page));
  TEST_ASSERT_EQUAL(SOMFY_MAX_GROUPS,page.total);
  TEST_ASSERT_EQUAL(SOMFY_MAX_GROUPS+catalogueRows,source.reads);
  source.reads=0;
  TEST_ASSERT_TRUE(cataloguePage(source,CatalogueKind::rooms,255,255,page));
  TEST_ASSERT_EQUAL(SOMFY_MAX_ROOMS,page.total);
  TEST_ASSERT_EQUAL(SOMFY_MAX_ROOMS+1,source.reads);
  TEST_ASSERT_FALSE(cataloguePage(source,CatalogueKind::rooms,0,0,page));
  TEST_ASSERT_EQUAL(0,page.count);
}
static void duplicate_id_in_another_room_is_not_a_valid_target() {
  Source source;
  source.entries[0]={7,1,0,"Room one"};
  source.entries[1]={7,2,0,"Room two"};
  CataloguePage page;
  TEST_ASSERT_FALSE(cataloguePage(source,CatalogueKind::devices,0,1,page));
  TEST_ASSERT_EQUAL(0,page.total);
}
static void source_replacement_during_page_copy_fails_closed() {
  struct ReplacingSource : Source {
    bool read(CatalogueKind kind,uint8_t slot,CatalogueEntry &out) override {
      const bool found=Source::read(kind,slot,out);
      if(reads>SOMFY_MAX_SHADES) ++out.id;
      return found;
    }
  } source;
  source.entries[0]={7,0,0,"Original"};
  CataloguePage page;
  TEST_ASSERT_FALSE(cataloguePage(source,CatalogueKind::devices,0,255,page));
  TEST_ASSERT_EQUAL(0,page.count);
}
static void selected_stable_id_survives_reorder_and_deletion_clamps_cursor() {
  Source source;
  for(uint8_t i=0;i<6;i++) source.entries[i]={uint8_t(i+1),0,int8_t(i),"Name"};
  CataloguePage page;
  TEST_ASSERT_TRUE(cataloguePage(source,CatalogueKind::devices,1,allRooms,page,2,true));
  TEST_ASSERT_EQUAL(1,page.cursor);
  source.entries[1].sortOrder=10;
  TEST_ASSERT_TRUE(cataloguePage(source,CatalogueKind::devices,1,allRooms,page,2,true));
  TEST_ASSERT_EQUAL(5,page.cursor); TEST_ASSERT_EQUAL(3,page.offset);
  TEST_ASSERT_EQUAL(2,page.rows[2].id);
  source.entries[1]={};
  TEST_ASSERT_TRUE(cataloguePage(source,CatalogueKind::devices,5,allRooms,page,2,true));
  TEST_ASSERT_EQUAL(4,page.cursor); TEST_ASSERT_EQUAL(3,page.offset);
  TEST_ASSERT_EQUAL(6,page.rows[1].id);
}
static void room_pages_include_all_and_unassigned_without_allocating_rooms() {
  Source source; CataloguePage page;
  TEST_ASSERT_TRUE(roomCataloguePage(source,0,page));
  TEST_ASSERT_EQUAL(2,page.total); TEST_ASSERT_EQUAL(2,page.count);
  TEST_ASSERT_EQUAL(allRooms,page.rows[0].id); TEST_ASSERT_EQUAL(0,page.rows[1].id);
  for(uint8_t i=0;i<SOMFY_MAX_ROOMS;i++) source.entries[i]={uint8_t(i+1),0,int8_t(i),"Room"};
  TEST_ASSERT_TRUE(roomCataloguePage(source,255,page));
  TEST_ASSERT_EQUAL(18,page.total); TEST_ASSERT_EQUAL(17,page.cursor);
  TEST_ASSERT_EQUAL(15,page.offset); TEST_ASSERT_EQUAL(16,page.rows[2].id);
  TEST_ASSERT_TRUE(roomCataloguePage(source,0,page,9,true));
  TEST_ASSERT_EQUAL(10,page.cursor); TEST_ASSERT_EQUAL(9,page.rows[1].id);
}
int main() {
  UNITY_BEGIN();
  RUN_TEST(empty_and_full_pages_are_bounded);
  RUN_TEST(sparse_sorted_ids_and_room_filter_are_stable);
  RUN_TEST(original_utf8_names_survive_without_becoming_identity);
  RUN_TEST(malformed_catalogues_fail_without_old_rows);
  RUN_TEST(group_and_room_limits_do_not_read_device_capacity);
  RUN_TEST(duplicate_id_in_another_room_is_not_a_valid_target);
  RUN_TEST(source_replacement_during_page_copy_fails_closed);
  RUN_TEST(selected_stable_id_survives_reorder_and_deletion_clamps_cursor);
  RUN_TEST(room_pages_include_all_and_unassigned_without_allocating_rooms);
  return UNITY_END();
}
