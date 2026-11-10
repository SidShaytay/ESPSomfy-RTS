#include "LocalEntityStatus.h"
#include <unity.h>
#include <string.h>
#include <math.h>
using namespace LocalUi;
void setUp(){} void tearDown(){}
static void positions_are_estimates_reversed_bounded_and_never_invented(){
  EntityStatus s;s.lift=35.5f;s.liftTarget=80;s.favourite=20;s.tilt=12;s.tiltTarget=30;s.tiltFavourite=-1;s.tiltType=tilt_types::integrated;s.flip=true;
  EntityAdmin::FormRow row;TEST_ASSERT_TRUE(entityStatusRow(s,4,row));TEST_ASSERT_EQUAL_STRING("Lift estimate",row.label);TEST_ASSERT_EQUAL_STRING("64.5%",row.value);
  entityStatusRow(s,5,row);TEST_ASSERT_EQUAL_STRING("20.0%",row.value);entityStatusRow(s,8,row);TEST_ASSERT_EQUAL_STRING("80.0%",row.value);
  entityStatusRow(s,9,row);TEST_ASSERT_EQUAL_STRING("Unset",row.value);s.lift=NAN;entityStatusRow(s,4,row);TEST_ASSERT_EQUAL_STRING("Unknown",row.value);
  s.group=true;entityStatusRow(s,4,row);TEST_ASSERT_EQUAL_STRING("Varies by member",row.value);entityStatusRow(s,8,row);TEST_ASSERT_EQUAL_STRING("Varies by member",row.value);
}
static void categories_weather_room_and_readonly_navigation_are_honest(){
  EntityStatus s;s.roomId=4;strcpy(s.room,"Office");s.sunSensor=true;s.flags=0x21;EntityAdmin::FormRow row;
  entityStatusRow(s,0,row);TEST_ASSERT_EQUAL_STRING("#4 Office",row.value);entityStatusRow(s,11,row);TEST_ASSERT_EQUAL_STRING("Enabled",row.value);
  entityStatusRow(s,12,row);TEST_ASSERT_EQUAL_STRING("Yes",row.value);entityStatusRow(s,13,row);TEST_ASSERT_EQUAL_STRING("No",row.value);
  s.shadeType=shade_types::garage1;entityStatusRow(s,4,row);TEST_ASSERT_EQUAL_STRING("Not applicable",row.value);
  s.shadeType=shade_types::roller;s.tiltType=tilt_types::none;entityStatusRow(s,6,row);TEST_ASSERT_EQUAL_STRING("Not configured",row.value);
  s.group=true;s.members=2;s.missing=1;entityStatusRow(s,14,row);TEST_ASSERT_EQUAL_STRING("2 present; 1 missing",row.value);
  TEST_ASSERT_TRUE(entityStatusRow(s,entityStatusRows-1,row));TEST_ASSERT_EQUAL_STRING("Back",row.label);TEST_ASSERT_FALSE(entityStatusRow(s,entityStatusRows,row));
}
int main(){UNITY_BEGIN();RUN_TEST(positions_are_estimates_reversed_bounded_and_never_invented);RUN_TEST(categories_weather_room_and_readonly_navigation_are_honest);return UNITY_END();}
