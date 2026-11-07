#ifndef PREFERENCES_STUB_H
#define PREFERENCES_STUB_H
#include <Arduino.h>
#include <cstdint>
#include <map>
#include <string>

// Only the rolling-code persistence boundary used by the production TX path.
class Preferences {
  std::string prefix;
  inline static std::map<std::string,uint16_t> values;
  public:
    bool begin(const char *space, bool = false) { prefix=space; prefix+=':'; return true; }
    void end() {}
    uint16_t getUShort(const char *key, uint16_t fallback=0) {
      auto found=values.find(prefix+key);
      return found==values.end() ? fallback : found->second;
    }
    size_t putUShort(const char *key,uint16_t value) { values[prefix+key]=value; return sizeof(value); }
    static void resetTestState() { values.clear(); }
};
#endif
