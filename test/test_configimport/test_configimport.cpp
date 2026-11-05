#include <unity.h>
#include <map>
#include <string>
#include "ConfigRecords.h"
#include "RollingCode.h"
#include "format24_fixture.h"

void setUp() {}
void tearDown() {}

class BackupReader : public ConfigReader {
  size_t pos = 0;
  public:
    bool readByte(uint8_t &val) override {
      if(pos == sizeof(FORMAT24_BACKUP) - 1) return false;
      val = static_cast<uint8_t>(FORMAT24_BACKUP[pos++]);
      return true;
    }
    uint32_t position() override { return static_cast<uint32_t>(pos); }
};

struct ImportedModel {
  config_header_t header;
  room_record_t room;
  shade_record_t shades[2];
  group_record_t group;
};

static ImportedModel readModel(BackupReader &r) {
  ImportedModel model;
  TEST_ASSERT_TRUE(readConfigHeader(r, model.header));
  TEST_ASSERT_EQUAL_UINT8(24, model.header.version);
  TEST_ASSERT_EQUAL_UINT8(1, model.header.roomRecords);
  TEST_ASSERT_EQUAL_UINT8(2, model.header.shadeRecords);
  TEST_ASSERT_EQUAL_UINT8(1, model.header.groupRecords);
  TEST_ASSERT_EQUAL_UINT32(model.header.length, r.position());
  TEST_ASSERT_TRUE(readRoomRecord(r, model.room));
  for(auto &shade : model.shades) TEST_ASSERT_TRUE(readShadeRecord(r, model.header, shade));
  TEST_ASSERT_TRUE(readGroupRecord(r, model.header, model.group));
  TEST_ASSERT_EQUAL_UINT32(model.header.length + shadeModelBytes(model.header), r.position());
  return model;
}

static void test_original_format24_preserves_calibration_membership_and_record_boundaries() {
  BackupReader r;
  const ImportedModel model = readModel(r);
  TEST_ASSERT_EQUAL_STRING("Synthetic room", model.room.name);
  for(const auto &shade : model.shades) {
    TEST_ASSERT_TRUE(shade.paired);
    TEST_ASSERT_EQUAL_UINT32(20340, shade.upTime);
    TEST_ASSERT_EQUAL_UINT32(19800, shade.downTime);
    TEST_ASSERT_EQUAL_UINT32(4500, shade.tiltTime);
    TEST_ASSERT_EQUAL_UINT16(250, shade.stepSize);
    TEST_ASSERT_EQUAL_FLOAT(42.5, shade.myPos);
    TEST_ASSERT_EQUAL_FLOAT(12.25, shade.myTiltPos);
    TEST_ASSERT_EQUAL_FLOAT(66, shade.currentPos);
    TEST_ASSERT_EQUAL_FLOAT(33.5, shade.currentTiltPos);
    TEST_ASSERT_TRUE(shade.flipCommands);
    TEST_ASSERT_TRUE(shade.flipPosition);
    TEST_ASSERT_EQUAL_UINT8(4, shade.repeats);
    TEST_ASSERT_EQUAL_UINT8(model.room.roomId, shade.roomId);
  }
  TEST_ASSERT_EQUAL_HEX32(2097153, model.shades[0].linkedRemotes[0]);
  TEST_ASSERT_EQUAL_HEX32(2097154, model.shades[0].linkedRemotes[1]);
  TEST_ASSERT_EQUAL_HEX32(2097155, model.shades[0].linkedRemotes[2]);
  TEST_ASSERT_TRUE(model.group.hasRollingCode);
  TEST_ASSERT_EQUAL_UINT16(200, model.group.lastRollingCode);
  // The gap in the old file is compressed, retaining the same membership.
  TEST_ASSERT_EQUAL_UINT8(model.shades[0].shadeId, model.group.linkedShades[0]);
  TEST_ASSERT_EQUAL_UINT8(model.shades[1].shadeId, model.group.linkedShades[1]);
  TEST_ASSERT_EQUAL_UINT8(0, model.group.linkedShades[2]);
  TEST_ASSERT_EQUAL_UINT8(model.room.roomId, model.group.roomId);
  uint32_t repeaters[SOMFY_MAX_REPEATERS];
  TEST_ASSERT_TRUE(readRepeaterRecord(r, repeaters));
  TEST_ASSERT_EQUAL_HEX32(3145729, repeaters[0]);
  settings_record_t settings;
  TEST_ASSERT_TRUE(readSettingsRecord(r, model.header, settings));
  TEST_ASSERT_EQUAL_STRING("v2.4.7", settings.fwVersion);
  net_record_t net;
  TEST_ASSERT_TRUE(readNetRecord(r, model.header, net, true));
  TEST_ASSERT_EQUAL_INT8(18, net.ethMDIOPin);
  trans_record_t radio;
  TEST_ASSERT_TRUE(readTransRecord(r, radio));
  TEST_ASSERT_EQUAL_FLOAT(433.42, radio.frequency);
  TEST_ASSERT_FALSE(radio.noiseDetection); // Original 74-byte record lacks it.
  TEST_ASSERT_EQUAL_UINT32(expectedFileBytes(model.header), r.position());
  TEST_ASSERT_EQUAL_UINT32(sizeof(FORMAT24_BACKUP) - 1, r.position());
}

static void test_import_of_older_format24_keeps_counters_by_remote_address() {
  BackupReader r;
  const ImportedModel model = readModel(r);
  // Simulates the per-address ShadeCodes NVS values on the same controller.
  std::map<uint32_t, uint16_t> codes = {
    {1193046, 500}, {1193047, 700}, {6636321, 900},
    {2097153, 600}, {2097154, 650}
  };
  for(const auto &shade : model.shades) {
    codes[shade.remoteAddress] = rollingCodeAfterLoad(codes[shade.remoteAddress], shade.lastRollingCode);
  }
  codes[model.group.remoteAddress] = rollingCodeAfterLoad(codes[model.group.remoteAddress], model.group.lastRollingCode);
  TEST_ASSERT_EQUAL_UINT16(500, codes[model.shades[0].remoteAddress]);
  TEST_ASSERT_EQUAL_UINT16(800, codes[model.shades[1].remoteAddress]);
  TEST_ASSERT_EQUAL_UINT16(900, codes[model.group.remoteAddress]);
  // Linked remotes have addresses only in format24: restore reads their
  // existing NVS counters, including one remote linked to both shades.
  TEST_ASSERT_EQUAL_UINT16(600, codes[model.shades[0].linkedRemotes[0]]);
  TEST_ASSERT_EQUAL_UINT16(600, codes[model.shades[1].linkedRemotes[0]]);
  TEST_ASSERT_EQUAL_UINT16(650, codes[model.shades[0].linkedRemotes[1]]);
  // A replacement controller cannot recover linked codes from this format.
  TEST_ASSERT_EQUAL_UINT16(0, codes[model.shades[0].linkedRemotes[2]]);
  TEST_ASSERT_EQUAL_UINT16(501, nextRollingCode(false, 0, [&]() { return codes[1193046]; }));
  TEST_ASSERT_EQUAL_UINT16(901, nextRollingCode(false, 0, [&]() { return codes[6636321]; }));
}

int main(int argc, char **argv) {
  (void)argc;
  (void)argv;
  UNITY_BEGIN();
  RUN_TEST(test_original_format24_preserves_calibration_membership_and_record_boundaries);
  RUN_TEST(test_import_of_older_format24_keeps_counters_by_remote_address);
  return UNITY_END();
}
