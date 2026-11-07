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
  if(!text || *text++ != 'v') return false;
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
const char *validate(const Manifest &m, const char *installedApp) {
  uint32_t parts[3];
  if(m.schema != 1) return "Unsupported update manifest schema";
  if(m.board != BOARD || m.layout != LAYOUT) return "Release board or partition layout differs";
  if(m.flashBytes != FLASH_BYTES || m.psramBytes != PSRAM_BYTES) return "Release hardware requirements differ";
  if(!installedApp || m.appCompat != installedApp) return "Release needs different web assets; firmware-only update refused";
  if(m.asset != FIRMWARE_ASSET) return "Release firmware asset is not exact-board";
  if(m.size < 1024 || m.size > APP_BYTES) return "Release firmware size exceeds supported slot";
  if(!hex(m.sha256,64) || !hex(m.sourceSha,40) || !hex(m.bootloaderSha,64)) return "Release digest or source revision invalid";
  if(m.version.size() > 47 || !version(m.version.c_str(),parts) || m.channel != "stable") return "Only stable semantic-version releases are supported";
  return nullptr;
}
bool newer(const char *available, const char *installed) {
  uint32_t a[3], b[3];
  if(!version(available,a) || !version(installed,b)) return false;
  for(size_t i=0;i<3;i++) if(a[i] != b[i]) return a[i] > b[i];
  return false;
}
bool allowedUrl(const char *url) {
  if(!url || std::strlen(url)>2047 || std::strncmp(url,"https://",8)!=0) return false;
  for(const unsigned char *p=reinterpret_cast<const unsigned char *>(url);*p;p++) if(*p <= 32 || *p == 127 || *p == '\\') return false;
  const char *host=url+8;
  const char *end=std::strchr(host,'/');
  if(!end) return false;
  const std::string authority(host,end-host);
  return authority == "api.github.com" || authority == "github.com" || authority == "release-assets.githubusercontent.com";
}
std::string identity(const Manifest &m) {
  return std::string("ESPSOMFY_UPDATE_ID:1|")+m.board+"|"+m.layout+"|"+m.sourceSha+"|"+m.version+"|"+m.appCompat;
}
bool hasIdentity(const uint8_t *data, size_t size, const Manifest &m) {
  const std::string expected=identity(m);
  if(!data || size <= expected.size()) return false;
  for(size_t i=0;i+expected.size()<size;i++)
    if(data[i] == 'E' && data[i+expected.size()] == 0 && std::memcmp(data+i,expected.data(),expected.size()) == 0) return true;
  return false;
}
}
