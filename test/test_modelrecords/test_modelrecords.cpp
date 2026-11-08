#include "ModelRecords.h"
#include <string.h>
#include <string>
#include <unity.h>
struct Source : ModelRecordSource {
  room_record_t roomRecord;
  shade_record_t shadeRecord;
  group_record_t groupRecord;
  unsigned reads = 0;
  Source() {
    roomRecord.roomId = 7;
    strcpy(roomRecord.name, "Bedroom");
    shadeRecord.shadeId = 22;
    shadeRecord.shadeType = shade_types::shutter;
    shadeRecord.tiltType = tilt_types::tiltmotor;
    shadeRecord.myTiltPos = 45;
    shadeRecord.currentTiltPos = 67;
    shadeRecord.lastRollingCode = 54321;
    shadeRecord.bitLength = 56;
    groupRecord.groupId = 11;
    groupRecord.bitLength = 56;
    groupRecord.lastRollingCode = 60001;
  }
  bool room(uint8_t slot, room_record_t &out) override {
    ++reads;
    if (slot != 5)
      return false;
    out = roomRecord;
    return true;
  }
  bool shade(uint8_t slot, shade_record_t &out) override {
    ++reads;
    if (slot != 31)
      return false;
    out = shadeRecord;
    return true;
  }
  bool group(uint8_t slot, group_record_t &out) override {
    ++reads;
    if (slot != 9)
      return false;
    out = groupRecord;
    return true;
  }
  void repeaters(uint32_t (&out)[SOMFY_MAX_REPEATERS]) override {
    memset(out, 0, sizeof(out));
    out[0] = 123456;
  }
};
struct Writer : ConfigWriter {
  std::string text;
  bool writeBytes(const uint8_t *p, size_t n) override {
    text.append(reinterpret_cast<const char *>(p), n);
    return true;
  }
};
struct Reader : ConfigReader {
  const std::string &text;
  size_t at = 0;
  Reader(const std::string &s) : text(s) {}
  bool readByte(uint8_t &b) override {
    if (at >= text.size())
      return false;
    b = uint8_t(text[at++]);
    return true;
  }
  uint32_t position() override { return at; }
};
static config_header_t header() {
  config_header_t h;
  h.version = SHADE_HDR_VER;
  h.length = SHADE_HDR_SIZE;
  h.roomRecordSize = ROOM_REC_SIZE;
  h.roomRecords = 1;
  h.shadeRecordSize = SHADE_REC_SIZE;
  h.shadeRecords = 1;
  h.groupRecordSize = GROUP_REC_SIZE;
  h.groupRecords = 1;
  h.repeaterRecords = 1;
  h.repeaterRecordSize = REPEATER_REC_SIZE;
  return h;
}
void setUp() {}
void tearDown() {}
static void
serialized_normalization_is_pure_and_retry_does_not_change_source() {
  Source source;
  const auto before = source.shadeRecord;
  const auto normalized = normalizedModelShadeRecord(source.shadeRecord);
  TEST_ASSERT_EQUAL(tilt_types::none, normalized.tiltType);
  TEST_ASSERT_FLOAT_WITHIN(0.001, -1, normalized.myTiltPos);
  TEST_ASSERT_FLOAT_WITHIN(0.001, 0, normalized.currentTiltPos);
  Writer a, b;
  TEST_ASSERT_TRUE(writeModelRecords(a, header(), source));
  TEST_ASSERT_TRUE(writeModelRecords(b, header(), source));
  TEST_ASSERT_EQUAL_STRING(a.text.c_str(), b.text.c_str());
  TEST_ASSERT_EQUAL(before.tiltType, source.shadeRecord.tiltType);
  TEST_ASSERT_FLOAT_WITHIN(0.001, before.myTiltPos,
                           source.shadeRecord.myTiltPos);
  TEST_ASSERT_FLOAT_WITHIN(0.001, before.currentTiltPos,
                           source.shadeRecord.currentTiltPos);
  TEST_ASSERT_EQUAL(54321, source.shadeRecord.lastRollingCode);
  Reader reader(a.text);
  config_header_t h;
  room_record_t room;
  shade_record_t shade;
  group_record_t group;
  readConfigHeader(reader, h);
  readRoomRecord(reader, room);
  readShadeRecord(reader, h, shade);
  readGroupRecord(reader, h, group);
  TEST_ASSERT_EQUAL(tilt_types::none, shade.tiltType);
  TEST_ASSERT_FLOAT_WITHIN(0.001, -1, shade.myTiltPos);
  TEST_ASSERT_FLOAT_WITHIN(0.001, 0, shade.currentTiltPos);
  TEST_ASSERT_EQUAL(54321, shade.lastRollingCode);
  TEST_ASSERT_EQUAL(60001, group.lastRollingCode);
}
static void tilting_blind_values_are_preserved() {
  Source source;
  source.shadeRecord.shadeType = shade_types::blind;
  const auto out = normalizedModelShadeRecord(source.shadeRecord);
  TEST_ASSERT_EQUAL(tilt_types::tiltmotor, out.tiltType);
  TEST_ASSERT_FLOAT_WITHIN(0.001, 45, out.myTiltPos);
  TEST_ASSERT_FLOAT_WITHIN(0.001, 67, out.currentTiltPos);
  TEST_ASSERT_EQUAL(54321, out.lastRollingCode);
}
static void mismatched_or_excessive_counts_are_refused() {
  Source source;
  Writer writer;
  auto h = header();
  h.shadeRecords = 2;
  TEST_ASSERT_FALSE(writeModelRecords(writer, h, source));
  h = header();
  h.groupRecords = SOMFY_MAX_GROUPS + 1;
  const auto reads = source.reads;
  TEST_ASSERT_FALSE(writeModelRecords(writer, h, source));
  TEST_ASSERT_EQUAL(reads, source.reads);
}
int main() {
  UNITY_BEGIN();
  RUN_TEST(serialized_normalization_is_pure_and_retry_does_not_change_source);
  RUN_TEST(tilting_blind_values_are_preserved);
  RUN_TEST(mismatched_or_excessive_counts_are_refused);
  return UNITY_END();
}
