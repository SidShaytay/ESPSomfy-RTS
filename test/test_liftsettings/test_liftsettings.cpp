#include <unity.h>
#include "LiftCalibrationJson.h"
void setUp() {}
void tearDown() {}
static ShadeSettings calibrated() {
  ShadeSettings s; s.calibration.kind=LiftKind::segmented;
  s.calibration.up={1000,3500,1000,500};
  s.calibration.down={2000,3000,1000,700}; return s;
}
static void test_unrelated_edits_preserve_profile_and_time_edits_revalidate() {
  auto current=calibrated(); JsonDocument doc; doc["name"]="Other";
  LiftCalibration c;
  TEST_ASSERT_TRUE(projectLiftCalibration(doc.as<JsonObjectConst>(),current,c));
  TEST_ASSERT_TRUE(sameLiftCalibration(current.calibration,c));
  doc["downTime"]=2000;
  TEST_ASSERT_FALSE(projectLiftCalibration(doc.as<JsonObjectConst>(),current,c));
  TEST_ASSERT_EQUAL_UINT32(10000,current.downTime);
  TEST_ASSERT_EQUAL_UINT32(2000,current.calibration.down.takeupMs);
}
static void test_profile_edit_checks_combined_times_and_phase_constraints() {
  ShadeSettings current; JsonDocument doc;
  doc["calibrationKind"]=2; doc["upTakeupMs"]=1000; doc["downTakeupMs"]=2000;
  LiftCalibration c; TEST_ASSERT_TRUE(projectLiftCalibration(doc.as<JsonObjectConst>(),current,c));
  TEST_ASSERT_EQUAL_UINT32(2000,c.down.takeupMs);
  doc["calibrationKind"]=1;
  TEST_ASSERT_FALSE(projectLiftCalibration(doc.as<JsonObjectConst>(),current,c));
  doc["upTakeupMs"]=0; doc["downTakeupMs"]=0; doc["upMidpointMs"]=3500;
  TEST_ASSERT_TRUE(projectLiftCalibration(doc.as<JsonObjectConst>(),current,c));
}
static void test_profile_edit_rejects_malformed_values_without_narrowing() {
  ShadeSettings current; LiftCalibration c; JsonDocument doc;
  for(const char *value : {"1", "257", "-1"}) {
    doc["calibrationKind"]=value;
    TEST_ASSERT_FALSE(projectLiftCalibration(doc.as<JsonObjectConst>(),current,c));
  }
  doc["calibrationKind"]=257;
  TEST_ASSERT_FALSE(projectLiftCalibration(doc.as<JsonObjectConst>(),current,c));
  doc["calibrationKind"]=2; doc["upTakeupMs"]=-1;
  TEST_ASSERT_FALSE(projectLiftCalibration(doc.as<JsonObjectConst>(),current,c));
  doc["upTakeupMs"]=1000; doc["upTime"]=1000;
  TEST_ASSERT_FALSE(projectLiftCalibration(doc.as<JsonObjectConst>(),current,c));
}
static void test_calibration_requires_a_lift_only_percentage_motor() {
  ShadeSettings current; LiftCalibration out; JsonDocument doc;
  doc["calibrationKind"] = 1;
  current.tiltType = tilt_types::integrated;
  TEST_ASSERT_FALSE(projectLiftCalibration(doc.as<JsonObjectConst>(), current, out));
  doc["tiltType"] = uint8_t(tilt_types::none);
  TEST_ASSERT_TRUE(projectLiftCalibration(doc.as<JsonObjectConst>(), current, out));
  doc["shadeType"] = uint8_t(shade_types::garage1);
  TEST_ASSERT_FALSE(projectLiftCalibration(doc.as<JsonObjectConst>(), current, out));
}
int main() {
  UNITY_BEGIN();
  RUN_TEST(test_calibration_requires_a_lift_only_percentage_motor);
  RUN_TEST(test_unrelated_edits_preserve_profile_and_time_edits_revalidate);
  RUN_TEST(test_profile_edit_checks_combined_times_and_phase_constraints);
  RUN_TEST(test_profile_edit_rejects_malformed_values_without_narrowing);
  return UNITY_END();
}
