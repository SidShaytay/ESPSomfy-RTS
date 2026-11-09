#include "BundledWeb.h"
#include <cassert>
#include <cstring>
#include <string>
#include <cstdint>
int hashFailure=0;unsigned activeHashes=0;
static unsigned filesystemReads=0;
struct File {
 size_t read(uint8_t *out,size_t size){constexpr char value[]="9.9.9\n";const auto n=size<sizeof(value)-1?size:sizeof(value)-1;std::memcpy(out,value,n);return n;}
 void close(){}
};
struct FS {
 bool exists(const char *){++filesystemReads;return true;}
 File open(const char *,const char *){++filesystemReads;return {};}
} LittleFS;
struct ConfigSettings {
 struct Version {std::string name;void parse(const char *value){name=value;}}appVersion;
 bool getAppVersion();
};
void _trim(char *value){const auto n=std::strlen(value);if(n&&value[n-1]=='\n')value[n-1]=0;}
#include "actual-version.inc"
int main(){
 ConfigSettings settings;assert(settings.getAppVersion());
#if defined(ESPSOMFY_BUNDLED_WEB)
 assert(settings.appVersion.name=="4.0.1"&&filesystemReads==0);
#else
 assert(settings.appVersion.name=="9.9.9"&&filesystemReads==2);
#endif
}
