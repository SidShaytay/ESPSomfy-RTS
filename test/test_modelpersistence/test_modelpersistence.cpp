#include "ModelPersistence.h"
#include "MountedFilesystemGate.h"
#include <stdlib.h>
#include <string.h>
#include <string>
#include <unistd.h>
#include <unity.h>
#include <vector>
static std::string directory;
static unsigned call = 0, failAt = 0, writeCalls = 0;
static bool readError = false, corruptNext = false;
static unsigned readPass = 0, corruptPass = 0;
static bool failNow() { return ++call == failAt; }
static FILE *openFile(const char *p, const char *mode) {
  readError = false;
  if (mode[0] == 'r') {
    ++readPass;
    corruptNext = readPass == corruptPass;
  }
  return failNow() ? nullptr : fopen(p, mode);
}
static size_t writeFile(const void *p, size_t s, size_t n, FILE *f) {
  ++writeCalls;
  if (failNow())
    return n ? fwrite(p, s, n - 1, f) : 0;
  return fwrite(p, s, n, f);
}
static size_t readFile(void *p, size_t s, size_t n, FILE *f) {
  if (failNow()) {
    readError = true;
    return 0;
  }
  const auto count = fread(p, s, n, f);
  if (count && corruptNext) {
    static_cast<unsigned char *>(p)[0] ^= 1;
    corruptNext = false;
  }
  return count;
}
static int errorFile(FILE *f) { return failNow() || readError ? 1 : ferror(f); }
static int descriptorFile(FILE *f) { return failNow() ? -1 : fileno(f); }
static int flushFile(FILE *f) {
  const bool fail = failNow();
  const int r = fflush(f);
  return fail ? -1 : r;
}
static int syncFile(int fd) {
  const bool fail = failNow();
  const int r = fsync(fd);
  return fail ? -1 : r;
}
static int closeFile(FILE *f) {
  const bool fail = failNow();
  const int r = fclose(f);
  return fail ? -1 : r;
}
static int renameFile(const char *a, const char *b) {
  return failNow() ? -1 : rename(a, b);
}
static const ModelFileOperations operations{
    openFile, writeFile,      readFile,  errorFile,  flushFile,
    syncFile, descriptorFile, closeFile, renameFile, remove};
struct Records : ModelSerializer {
  unsigned serializations = 0;
  room_record_t room;
  shade_record_t shade;
  group_record_t group;
  Records() {
    room.roomId = 7;
    strcpy(room.name, "Bedroom");
    shade.shadeId = 22;
    shade.bitLength = 56;
    shade.lastRollingCode = 54321;
    shade.remoteAddress = 424242;
    group.groupId = 11;
    group.bitLength = 56;
    group.lastRollingCode = 60001;
    group.linkedShades[0] = 22;
  }
  bool write(ConfigWriter &w) override {
    ++serializations;
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
    strcpy(h.serverId, "123456789");
    writeConfigHeader(w, h);
    writeRoomRecord(w, room);
    writeShadeRecord(w, shade);
    writeGroupRecord(w, group);
    const uint32_t repeaters[SOMFY_MAX_REPEATERS] = {123456};
    return writeRepeaterRecord(w, repeaters);
  }
};
static std::string path(const char *name) { return directory + name; }
static void store(const char *name, const std::string &data) {
  FILE *f = fopen(path(name).c_str(), "wb");
  TEST_ASSERT_NOT_NULL(f);
  TEST_ASSERT_EQUAL(data.size(), fwrite(data.data(), 1, data.size(), f));
  TEST_ASSERT_EQUAL(0, fclose(f));
}
static std::string load(const char *name) {
  FILE *f = fopen(path(name).c_str(), "rb");
  if (!f)
    return {};
  std::string s;
  char b[128];
  size_t n;
  while ((n = fread(b, 1, sizeof(b), f)))
    s.append(b, n);
  fclose(f);
  return s;
}
void setUp() {
  mountedFilesystemGate().resume();
  char name[] = "/tmp/espsomfy-model-XXXXXX";
  char *p = mkdtemp(name);
  TEST_ASSERT_NOT_NULL(p);
  directory = p;
  call = failAt = writeCalls = readPass = corruptPass = 0;
  readError = corruptNext = false;
}
void tearDown() {
  mountedFilesystemGate().resume();
  remove(path("/shades.cfg").c_str());
  remove(path("/shades.cfg.new").c_str());
  rmdir(directory.c_str());
}
struct RecordReader : ConfigReader {
  const std::string &text;
  size_t at = 0;
  explicit RecordReader(const std::string &s) : text(s) {}
  bool readByte(uint8_t &out) override {
    if (at >= text.size())
      return false;
    out = uint8_t(text[at++]);
    return true;
  }
  uint32_t position() override { return uint32_t(at); }
};
static void verified_replacement_contains_actual_records_and_counters() {
  Records records;
  store("/shades.cfg", "OLD VALID FILE");
  const auto result = commitModelFile(directory.c_str(), records, operations);
  TEST_ASSERT_TRUE(result.saved());
  TEST_ASSERT_EQUAL(ModelCommitStage::none, result.stage);
  TEST_ASSERT_EQUAL(3, records.serializations);
  const auto data = load("/shades.cfg");
  TEST_ASSERT_NOT_EQUAL(std::string::npos, data.find("54321"));
  TEST_ASSERT_NOT_EQUAL(std::string::npos, data.find("60001"));
  RecordReader reader(data);
  config_header_t header;
  room_record_t room;
  shade_record_t shade;
  group_record_t group;
  TEST_ASSERT_TRUE(readConfigHeader(reader, header));
  readRoomRecord(reader, room);
  readShadeRecord(reader, header, shade);
  readGroupRecord(reader, header, group);
  TEST_ASSERT_EQUAL(7, room.roomId);
  TEST_ASSERT_EQUAL(22, shade.shadeId);
  TEST_ASSERT_EQUAL(11, group.groupId);
  TEST_ASSERT_EQUAL(54321, shade.lastRollingCode);
  TEST_ASSERT_EQUAL(60001, group.lastRollingCode);
  TEST_ASSERT_EQUAL(54321, records.shade.lastRollingCode);
  TEST_ASSERT_EQUAL(60001, records.group.lastRollingCode);
  TEST_ASSERT_TRUE(load("/shades.cfg.new").empty());
}
static void
every_injected_io_failure_is_truthful_and_preserves_old_before_replace() {
  Records baseline;
  TEST_ASSERT_TRUE(
      commitModelFile(directory.c_str(), baseline, operations).saved());
  const auto calls = call;
  const auto expected = load("/shades.cfg");
  for (unsigned at = 1; at <= calls; at++) {
    store("/shades.cfg", "OLD VALID FILE");
    store("/shades.cfg.new", "STALE TEMPORARY");
    call = writeCalls = 0;
    readError = false;
    failAt = at;
    Records records;
    const auto r = commitModelFile(directory.c_str(), records, operations);
    TEST_ASSERT_FALSE(r.saved());
    TEST_ASSERT_NOT_EQUAL(ModelCommitStage::none, r.stage);
    TEST_ASSERT_EQUAL_STRING(r.state == ModelCommitState::uncertain
                                 ? expected.c_str()
                                 : "OLD VALID FILE",
                             load("/shades.cfg").c_str());
    TEST_ASSERT_EQUAL(54321, records.shade.lastRollingCode);
    TEST_ASSERT_EQUAL(60001, records.group.lastRollingCode);
  }
}
static void
missing_original_and_stale_staging_are_supported_without_loading_model() {
  Records records;
  store("/shades.cfg.new", "UNCOMMITTED JUNK");
  TEST_ASSERT_TRUE(
      commitModelFile(directory.c_str(), records, operations).saved());
  TEST_ASSERT_FALSE(load("/shades.cfg").empty());
  TEST_ASSERT_EQUAL(3, records.serializations);
}
static void
short_write_is_sticky_even_when_record_functions_ignore_earlier_returns() {
  Records records;
  store("/shades.cfg", "OLD");
  failAt = 2;
  const auto result = commitModelFile(directory.c_str(), records, operations);
  TEST_ASSERT_EQUAL(ModelCommitStage::write, result.stage);
  TEST_ASSERT_EQUAL(1, writeCalls);
  TEST_ASSERT_EQUAL(1, records.serializations);
  TEST_ASSERT_EQUAL_STRING("OLD", load("/shades.cfg").c_str());
}
static void
exact_stream_comparison_rejects_mismatch_before_and_after_replacement() {
  for (unsigned pass = 1; pass <= 2; pass++) {
    Records records;
    store("/shades.cfg", "OLD");
    readPass = 0;
    corruptPass = pass;
    const auto result = commitModelFile(directory.c_str(), records, operations);
    TEST_ASSERT_EQUAL(ModelCommitStage::verifyBytes, result.stage);
    TEST_ASSERT_EQUAL(pass == 1 ? ModelCommitState::failed
                                : ModelCommitState::uncertain,
                      result.state);
    if (pass == 1)
      TEST_ASSERT_EQUAL_STRING("OLD", load("/shades.cfg").c_str());
    else
      TEST_ASSERT_NOT_EQUAL(std::string::npos,
                            load("/shades.cfg").find("54321"));
  }
}
static void trailing_bytes_or_serializer_refusal_never_replace_old_file() {
  struct Trailing : Records {
    bool write(ConfigWriter &w) override {
      const bool ok = Records::write(w);
      if (serializations == 1)
        w.writeChar('X');
      return ok;
    }
  } trailing;
  store("/shades.cfg", "OLD");
  const auto result = commitModelFile(directory.c_str(), trailing, operations);
  TEST_ASSERT_EQUAL(ModelCommitState::failed, result.state);
  TEST_ASSERT_EQUAL(ModelCommitStage::verifyBytes, result.stage);
  TEST_ASSERT_EQUAL_STRING("OLD", load("/shades.cfg").c_str());
  struct Refused : ModelSerializer {
    bool write(ConfigWriter &w) override {
      w.writeChar('X');
      return false;
    }
  } refused;
  const auto refusal = commitModelFile(directory.c_str(), refused, operations);
  TEST_ASSERT_EQUAL(ModelCommitStage::serialize, refusal.stage);
  TEST_ASSERT_EQUAL_STRING("OLD", load("/shades.cfg").c_str());
}
static void invalid_mount_or_missing_operation_fails_before_any_io() {
  Records records;
  std::string tooLong(128, 'x');
  const char *invalid[] = {nullptr, "", "relative", tooLong.c_str()};
  for (const char *prefix : invalid) {
    const auto result = commitModelFile(prefix, records, operations);
    TEST_ASSERT_EQUAL(ModelCommitStage::path, result.stage);
  }
  auto absent = operations;
  absent.sync = nullptr;
  TEST_ASSERT_EQUAL(ModelCommitStage::path,
                    commitModelFile(directory.c_str(), records, absent).stage);
  TEST_ASSERT_EQUAL(0, call);
  TEST_ASSERT_EQUAL(0, records.serializations);
}
static void sealed_mount_defers_model_io_and_retry_uses_current_ram_counters() {
  Records records;TEST_ASSERT_TRUE(commitModelFile(directory.c_str(),records,operations).saved());
  const auto before=load("/shades.cfg");records.shade.lastRollingCode=54322;
  call=0;records.serializations=0;mountedFilesystemGate().seal();
  const auto blocked=commitModelFile(directory.c_str(),records,operations);
  TEST_ASSERT_FALSE(blocked.saved());TEST_ASSERT_EQUAL(ModelCommitStage::filesystemBusy,blocked.stage);TEST_ASSERT_EQUAL(0,call);TEST_ASSERT_EQUAL(0,records.serializations);
  TEST_ASSERT_EQUAL_STRING(before.c_str(),load("/shades.cfg").c_str());
  TEST_ASSERT_TRUE(mountedFilesystemGate().drained());
  mountedFilesystemGate().resume();TEST_ASSERT_TRUE(commitModelFile(directory.c_str(),records,operations).saved());
  TEST_ASSERT_NOT_EQUAL(std::string::npos,load("/shades.cfg").find("54322"));
}
static void admitted_model_commit_retains_mount_until_final_readback_finishes() {
  struct SealingRecords:Records {
    bool tracked=true;
    bool write(ConfigWriter &writer)override {
      mountedFilesystemGate().seal();tracked=tracked&&!mountedFilesystemGate().drained();
      return Records::write(writer);
    }
  } records;
  TEST_ASSERT_TRUE(commitModelFile(directory.c_str(),records,operations).saved());
  TEST_ASSERT_TRUE(records.tracked);TEST_ASSERT_EQUAL(3,records.serializations);
  TEST_ASSERT_TRUE(mountedFilesystemGate().drained());
}
int main() {
  UNITY_BEGIN();
  RUN_TEST(admitted_model_commit_retains_mount_until_final_readback_finishes);
  RUN_TEST(sealed_mount_defers_model_io_and_retry_uses_current_ram_counters);
  RUN_TEST(verified_replacement_contains_actual_records_and_counters);
  RUN_TEST(
      every_injected_io_failure_is_truthful_and_preserves_old_before_replace);
  RUN_TEST(
      missing_original_and_stale_staging_are_supported_without_loading_model);
  RUN_TEST(
      short_write_is_sticky_even_when_record_functions_ignore_earlier_returns);
  RUN_TEST(
      exact_stream_comparison_rejects_mismatch_before_and_after_replacement);
  RUN_TEST(trailing_bytes_or_serializer_refusal_never_replace_old_file);
  RUN_TEST(invalid_mount_or_missing_operation_fails_before_any_io);
  return UNITY_END();
}
