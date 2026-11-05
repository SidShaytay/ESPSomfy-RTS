#include <unity.h>
#include <string>
#include "ConfigRecords.h"
#include "ConfigValidation.h"

void setUp() {}
void tearDown() {}

class HeaderWriter : public ConfigWriter {
  public:
    std::string bytes;
    bool writeBytes(const uint8_t *buf, size_t len) override {
      bytes.append(reinterpret_cast<const char *>(buf), len);
      return true;
    }
};

class HeaderReader : public ConfigReader {
  const std::string &bytes;
  size_t offset = 0;
  public:
    explicit HeaderReader(const std::string &value) : bytes(value) {}
    bool readByte(uint8_t &val) override {
      if(offset >= bytes.size()) return false;
      val = static_cast<uint8_t>(bytes[offset++]);
      return true;
    }
    uint32_t position() override { return static_cast<uint32_t>(offset); }
};

static bool serializedCountsFit(uint8_t rooms, uint8_t shades, uint8_t groups) {
  config_header_t input;
  input.version = SHADE_HDR_VER;
  input.length = SHADE_HDR_SIZE;
  input.roomRecordSize = ROOM_REC_SIZE;
  input.roomRecords = rooms;
  input.shadeRecordSize = SHADE_REC_SIZE;
  input.shadeRecords = shades;
  input.groupRecordSize = GROUP_REC_SIZE;
  input.groupRecords = groups;
  HeaderWriter writer;
  TEST_ASSERT_TRUE(writeConfigHeader(writer, input));
  HeaderReader reader(writer.bytes);
  config_header_t parsed;
  TEST_ASSERT_TRUE(readConfigHeader(reader, parsed));
  TEST_ASSERT_EQUAL_UINT8(rooms, parsed.roomRecords);
  TEST_ASSERT_EQUAL_UINT8(shades, parsed.shadeRecords);
  TEST_ASSERT_EQUAL_UINT8(groups, parsed.groupRecords);
  return modelRecordCountsFit(parsed);
}

static void test_model_accepts_exactly_maximum_record_counts() {
  TEST_ASSERT_TRUE(serializedCountsFit(SOMFY_MAX_ROOMS, 0, 0));
  TEST_ASSERT_TRUE(serializedCountsFit(0, SOMFY_MAX_SHADES, 0));
  TEST_ASSERT_TRUE(serializedCountsFit(0, 0, SOMFY_MAX_GROUPS));
  TEST_ASSERT_TRUE(serializedCountsFit(SOMFY_MAX_ROOMS, SOMFY_MAX_SHADES, SOMFY_MAX_GROUPS));
}

static void test_model_rejects_one_record_beyond_each_array() {
  TEST_ASSERT_FALSE(serializedCountsFit(SOMFY_MAX_ROOMS + 1, 0, 0));
  TEST_ASSERT_FALSE(serializedCountsFit(0, SOMFY_MAX_SHADES + 1, 0));
  TEST_ASSERT_FALSE(serializedCountsFit(0, 0, SOMFY_MAX_GROUPS + 1));
}

int main(int argc, char **argv) {
  (void)argc;
  (void)argv;
  UNITY_BEGIN();
  RUN_TEST(test_model_accepts_exactly_maximum_record_counts);
  RUN_TEST(test_model_rejects_one_record_beyond_each_array);
  return UNITY_END();
}
