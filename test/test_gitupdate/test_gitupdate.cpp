#include <unity.h>
#include "GitUpdatePolicy.h"
#include <string>

void setUp() {}
void tearDown() {}
using namespace GitUpdatePolicy;
static const std::string APP_TEXT = "sha256:" + std::string(64, 'c');
static const char *APP = APP_TEXT.c_str();
static Manifest valid() {
  Manifest m;
  m.schema = 2;
  m.features = 7;
  m.compatibleBoards = {BOARD, "lilygo-t-embed-cc1101-plus"};
  m.board = BOARD;
  m.layout = LAYOUT;
  m.flashBytes = FLASH_BYTES;
  m.psramBytes = PSRAM_BYTES;
  m.appCompat = "sha256:" + std::string(64, 'c');
  m.version = "v4.0.3";
  m.sourceSha = std::string(40, 'a');
  m.asset = FIRMWARE_ASSET;
  m.size = 1000000;
  m.sha256 = std::string(64, 'a');
  m.bootloaderSha = std::string(64, 'b');
  m.channel = "stable";
  return m;
}
static void accepts_exact_board_release() { TEST_ASSERT_EQUAL((int)CheckResult::compatible, (int)check(valid(), APP)); }
static void rejects_wrong_board_layout_and_hardware() {
  Manifest m=valid(); m.features=3; TEST_ASSERT_NOT_NULL(validate(m,APP));
  m=valid();m.compatibleBoards.clear();TEST_ASSERT_NOT_NULL(validate(m,APP));
  m=valid();m.compatibleBoards.push_back("esp32s3");TEST_ASSERT_NOT_NULL(validate(m,APP));
  m=valid(); m.board="esp32s3"; TEST_ASSERT_NOT_NULL(validate(m,APP));
  m=valid();m.layout="huge_app"; TEST_ASSERT_NOT_NULL(validate(m,APP));
  m=valid();m.flashBytes=8388608; TEST_ASSERT_NOT_NULL(validate(m,APP));
  m=valid();m.psramBytes=0; TEST_ASSERT_NOT_NULL(validate(m,APP));
}
static void rejects_wrong_web_application_and_generic_artifact() {
  Manifest m=valid();m.appCompat="4.0.1";TEST_ASSERT_NOT_NULL(validate(m,APP));
  m=valid();m.asset="firmware.bin";TEST_ASSERT_NOT_NULL(validate(m,APP));
}
static void rejects_bad_digest_size_revision_and_version() {
  Manifest m=valid();m.sha256[5]='g';TEST_ASSERT_NOT_NULL(validate(m,APP));
  m=valid();m.bootloaderSha[5]='g';TEST_ASSERT_NOT_NULL(validate(m,APP));
  m=valid();m.sourceSha.resize(39);TEST_ASSERT_NOT_NULL(validate(m,APP));
  m=valid();m.size=APP_BYTES+1;TEST_ASSERT_NOT_NULL(validate(m,APP));
  m=valid();m.size=0;TEST_ASSERT_NOT_NULL(validate(m,APP));
  m=valid();m.version="v4.0.3/../main";TEST_ASSERT_NOT_NULL(validate(m,APP));
  m=valid();m.version="v4.0.3-beta";TEST_ASSERT_NOT_NULL(validate(m,APP));
  m=valid();m.channel="beta";TEST_ASSERT_NOT_NULL(validate(m,APP));
}
static void versions_compare_numerically_and_reject_downgrades() {
  TEST_ASSERT_TRUE(newer("v4.10.0","v4.9.9"));
  TEST_ASSERT_FALSE(newer("v4.0.3","v4.0.3"));
  TEST_ASSERT_FALSE(newer("v4.0.2","v4.0.3"));
  TEST_ASSERT_FALSE(newer("v4.0.3","development"));
  TEST_ASSERT_FALSE(newer("v999999999999999.0.0","v4.0.3"));
}
static void permits_only_known_https_redirect_hosts() {
  TEST_ASSERT_TRUE(allowedUrl("https://api.github.com/repos/SidShaytay/ESPSomfy-RTS/releases/latest"));
  TEST_ASSERT_TRUE(allowedUrl("https://github.com/SidShaytay/ESPSomfy-RTS/releases/download/v4.0.3/firmware.bin"));
  TEST_ASSERT_TRUE(allowedUrl("https://release-assets.githubusercontent.com/github-production-release-asset/123?sig=abc"));
  for(const char *url: {"http://github.com/a", "https://github.com.evil/a", "https://github.com@evil/a", "https://github.com:444/a", "https://127.0.0.1/a", "https://raw.githubusercontent.com/a", "https://github.com/a\r\nHost: evil"}) TEST_ASSERT_FALSE(allowedUrl(url));
}
static void image_identity_must_match_authenticated_manifest() {
  Manifest m=valid();std::string marker=identity(m);std::string image(100,'x');image+=marker;image.push_back('\0');image+=std::string(100,'x');
  TEST_ASSERT_TRUE(hasIdentity(reinterpret_cast<const uint8_t *>(image.data()),image.size(),m));
  m.sourceSha[0]='b';TEST_ASSERT_FALSE(hasIdentity(reinterpret_cast<const uint8_t *>(image.data()),image.size(),m));
  m=valid();image.resize(100+marker.size()-1);TEST_ASSERT_FALSE(hasIdentity(reinterpret_cast<const uint8_t *>(image.data()),image.size(),m));
}
static void bounded_versions_urls_and_manifest_fail_closed() {
  for(const char *v : {"", "v01.2.3", "v1.02.3", "v1.2.03", "v1.2", "v1.2.3.4", "v4294967296.0.0", "dev-aaa", "v1.2.3+build"})
    TEST_ASSERT_FALSE(newer(v,"v0.0.0"));
  TEST_ASSERT_TRUE(newer("v4294967295.0.0","v4294967294.9.9"));
  TEST_ASSERT_FALSE(newer(nullptr,"v0.0.0"));
  TEST_ASSERT_FALSE(allowedUrl(nullptr));
  for(const char *u : {"https://github.com", "https://github.com:443/a", "https://user@github.com/a", "https://github.com\\evil/a", "https://github.com\t/a"})
    TEST_ASSERT_FALSE(allowedUrl(u));
  std::string url="https://github.com/"+std::string(2048,'x');
  TEST_ASSERT_FALSE(allowedUrl(url.c_str()));
  Manifest m=valid();m.features=263;TEST_ASSERT_NOT_NULL(validate(m,APP));
  m=valid();m.schema=1;TEST_ASSERT_NOT_NULL(validate(m,APP));
  m=valid();m.compatibleBoards[1]=BOARD;TEST_ASSERT_NOT_NULL(validate(m,APP));
  m=valid();m.appCompat="sha256:"+std::string(64,'G');TEST_ASSERT_NOT_NULL(validate(m,APP));
  m=valid();m.version=std::string("v1.2.3\0other",12);TEST_ASSERT_NOT_NULL(validate(m,APP));
  m=valid();m.size=1023;TEST_ASSERT_NOT_NULL(validate(m,APP));
  m=valid();m.size=1024;TEST_ASSERT_NULL(validate(m,APP));
  m.size=APP_BYTES;TEST_ASSERT_NULL(validate(m,APP));
}
static void duplicate_or_conflicting_image_identity_is_refused() {
  Manifest m=valid();std::string image=identity(m);image.push_back('\0');
  image+=image;
  TEST_ASSERT_FALSE(hasIdentity(reinterpret_cast<const uint8_t *>(image.data()),image.size(),m));
}
int main() {
  UNITY_BEGIN();
  RUN_TEST(bounded_versions_urls_and_manifest_fail_closed);
  RUN_TEST(duplicate_or_conflicting_image_identity_is_refused);
  RUN_TEST(accepts_exact_board_release);
  RUN_TEST(rejects_wrong_board_layout_and_hardware);
  RUN_TEST(rejects_wrong_web_application_and_generic_artifact);
  RUN_TEST(rejects_bad_digest_size_revision_and_version);
  RUN_TEST(versions_compare_numerically_and_reject_downgrades);
  RUN_TEST(permits_only_known_https_redirect_hosts);
  RUN_TEST(image_identity_must_match_authenticated_manifest);
  return UNITY_END();
}
