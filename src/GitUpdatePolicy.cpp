#include "GitUpdatePolicy.h"
#include <cstring>
#include <limits>

namespace GitUpdatePolicy {
static bool hex(const std::string &s, size_t size) {
  if(s.size() != size) return false;
  for(char c : s) if(!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return false;
  return true;
}
static bool version(const char *text, uint32_t (&parts)[3]) {
  if(!text) return false;
  size_t length=0;
  while(length < 48 && text[length]) ++length;
  if(length == 48 || *text++ != 'v') return false;
  for(size_t i=0;i<3;i++) {
    uint32_t n=0;
    if(*text < '0' || *text > '9') return false;
    // Strict semver has no leading zeroes and no unbounded integer conversion.
    if(*text == '0' && text[1] >= '0' && text[1] <= '9') return false;
    while(*text >= '0' && *text <= '9') {
      if(n > (std::numeric_limits<uint32_t>::max() - (*text-'0')) / 10) return false;
      n=n*10+(*text++-'0');
    }
    parts[i]=n;
    if(i<2 && *text++ != '.') return false;
  }
  return *text == 0;
}
CheckResult check(const Manifest &m) {
  uint32_t parts[3];
  if(m.schema != 3) return CheckResult::schema;
  if(m.board != BOARD || m.layout != LAYOUT || m.features != GitUpdateProfile::features ||
      m.compatibleBoards != std::vector<std::string>{BOARD, GitUpdateProfile::plusBoard} ||
      m.flashBytes != FLASH_BYTES || m.psramBytes != PSRAM_BYTES) return CheckResult::hardware;
  // Each firmware carries its own immutable web bytes. A new release may have
  // a different web digest; it must bind that digest in its own image marker.
  if(m.assetsMode != GitUpdateProfile::assetsMode || m.appCompat.size() != 71 || m.appCompat.compare(0,7,"sha256:") != 0 ||
      !hex(m.appCompat.substr(7),64)) return CheckResult::assets;
  if(m.asset != FIRMWARE_ASSET) return CheckResult::artifact;
  if(m.size < 1024 || m.size > APP_BYTES) return CheckResult::range;
  if(!hex(m.sha256,64) || !hex(m.sourceSha,40) || !hex(m.bootloaderSha,64)) return CheckResult::digest;
  if(m.version.size() > 47 || m.version.find('\0') != std::string::npos ||
      !version(m.version.c_str(),parts) || m.channel != "stable") return CheckResult::version;
  return CheckResult::compatible;
}
const char *reason(CheckResult result) {
  switch(result) {
    case CheckResult::compatible: return nullptr;
    case CheckResult::schema: return "Unsupported update manifest schema";
    case CheckResult::hardware: return "Release hardware, features or layout differs";
    case CheckResult::assets: return "Release requires bundled web assets with a valid exact digest";
    case CheckResult::artifact: return "Release firmware asset is not the shared profile";
    case CheckResult::range: return "Release firmware size exceeds supported slot";
    case CheckResult::digest: return "Release digest or source revision invalid";
    case CheckResult::version: return "Only stable semantic-version releases are supported";
  }
  return "Unknown update policy result";
}
const char *validate(const Manifest &m) { return reason(check(m)); }
bool stableVersion(const char *text) { uint32_t parts[3];return version(text,parts); }
bool newer(const char *available, const char *installed) {
  uint32_t a[3], b[3];
  if(!version(available,a) || !version(installed,b)) return false;
  for(size_t i=0;i<3;i++) if(a[i] != b[i]) return a[i] > b[i];
  return false;
}
bool allowedUrl(const char *url) {
  if(!url) return false;
  size_t length=0;
  while(length < 2048 && url[length]) ++length;
  if(length < 9 || length == 2048 || std::strncmp(url,"https://",8)!=0) return false;
  for(const unsigned char *p=reinterpret_cast<const unsigned char *>(url);*p;p++) if(*p <= 32 || *p == 127 || *p == '\\') return false;
  const char *host=url+8;
  const char *end=std::strchr(host,'/');
  if(!end) return false;
  const std::string authority(host,end-host);
  return authority == "api.github.com" || authority == "github.com" || authority == "release-assets.githubusercontent.com";
}
std::string identity(const Manifest &m) {
  return std::string("ESPSOMFY_UPDATE_ID:3|")+m.board+"|"+m.layout+"|"+m.sourceSha+"|"+m.version+"|"+m.assetsMode+"|"+m.appCompat;
}
bool hasIdentity(const uint8_t *data, size_t size, const Manifest &m) {
  if(check(m) != CheckResult::compatible || !data || size > APP_BYTES) return false;
  const std::string expected=identity(m);
  if(size <= expected.size()) return false;
  const char prefix[]="ESPSOMFY_UPDATE_ID:";
  size_t markers=0;
  bool matched=false;
  for(size_t i=0;i+sizeof(prefix)-1<=size;i++) {
    if(std::memcmp(data+i,prefix,sizeof(prefix)-1) != 0) continue;
    ++markers;
    if(i+expected.size()<size && data[i+expected.size()] == 0 &&
        std::memcmp(data+i,expected.data(),expected.size()) == 0) matched=true;
  }
  return matched && markers == 1;
}
}
