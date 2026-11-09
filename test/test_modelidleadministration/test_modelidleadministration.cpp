#include "ModelIdleForAdministration.h"
#include <unity.h>
struct Model {
 struct Shade {uint8_t id=255;bool idle=true;uint8_t getShadeId()const{return id;}bool idleForAdministration()const{return idle;}} shades[32];
 struct Group {uint8_t id=255;int8_t direction=0;somfy_frame_t lastFrame;uint8_t linkedShades[32]{};uint8_t getGroupId()const{return id;}} groups[16];
 Shade *getShadeById(uint8_t id){for(auto &shade:shades)if(id&&id!=255&&shade.id==id)return &shade;return nullptr;}
};
void setUp(){}void tearDown(){}
static void every_configured_physical_shade_must_be_idle(){
 for(unsigned slot=0;slot<32;++slot){Model m;TEST_ASSERT_TRUE(modelIdleForAdministration(m));
  m.shades[slot].idle=false;TEST_ASSERT_TRUE(modelIdleForAdministration(m));
  m.shades[slot].id=32-slot;TEST_ASSERT_FALSE(modelIdleForAdministration(m));
  m.shades[slot].idle=true;TEST_ASSERT_TRUE(modelIdleForAdministration(m));
 }
}
static void tracked_completed_groups_ignore_stale_command_direction(){
 Model m;m.shades[31].id=7;m.groups[15].id=9;
 m.groups[15].linkedShades[31]=7;m.groups[15].direction=-1;
 m.groups[15].lastFrame.valid=true;m.groups[15].lastFrame.cmd=somfy_commands::Up;
 TEST_ASSERT_TRUE(modelIdleForAdministration(m));
 m.shades[31].idle=false;TEST_ASSERT_FALSE(modelIdleForAdministration(m));
 m.shades[31].idle=true;m.groups[15].linkedShades[0]=8;
 TEST_ASSERT_FALSE(modelIdleForAdministration(m));
}
static void unknown_group_motion_is_not_an_administration_grant(){
 for(unsigned slot=0;slot<16;++slot){Model m;m.groups[slot].id=slot+1;
  TEST_ASSERT_TRUE(modelIdleForAdministration(m));
  m.groups[slot].lastFrame.valid=true;m.groups[slot].lastFrame.cmd=somfy_commands::My;
  TEST_ASSERT_FALSE(modelIdleForAdministration(m));
  m.groups[slot].lastFrame.cmd=somfy_commands::Sensor;TEST_ASSERT_TRUE(modelIdleForAdministration(m));
  m.groups[slot].direction=1;TEST_ASSERT_FALSE(modelIdleForAdministration(m));
 }
}
int main(){UNITY_BEGIN();RUN_TEST(every_configured_physical_shade_must_be_idle);RUN_TEST(tracked_completed_groups_ignore_stale_command_direction);RUN_TEST(unknown_group_motion_is_not_an_administration_grant);return UNITY_END();}
