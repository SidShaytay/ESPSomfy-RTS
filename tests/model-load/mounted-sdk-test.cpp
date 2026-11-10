#include "ModelLoadSnapshot.h"
#include "esp_littlefs.h"
#include <cassert>
#include <cstring>
#include <cstdio>
namespace {
// Pinned SDK esp_littlefs_by_label(NULL) discovers subtype0x83. Arduino mounts
// the existing subtype0x82 partition explicitly by its "spiffs" label instead.
// Represent both partitions so unrelated native LittleFS cannot grant access.
bool namedMounted=true,nativeMounted=false;
int infoResult=ESP_OK;
size_t totalBytes=4096,usedBytes=512;
unsigned mountedQueries=0,infoQueries=0;
bool named(const char *label){return label&&strcmp(label,"spiffs")==0;}
}
bool esp_littlefs_mounted(const char *label){++mountedQueries;return named(label)?namedMounted:!label&&nativeMounted;}
int esp_littlefs_info(const char *label,size_t *total,size_t *used){
 ++infoQueries;if(!named(label))return -1;*total=totalBytes;*used=usedBytes;return infoResult;
}
int main(){
 noteModelFilesystemMounted(true);
 assert(modelFilesystemMounted()); // Regression: named subtype0x82, no0x83.
 assert(mountedQueries==1&&infoQueries==1);
 noteModelFilesystemMounted(false);assert(!modelFilesystemMounted());assert(mountedQueries==1&&infoQueries==1);
 noteModelFilesystemMounted(true);namedMounted=false;nativeMounted=true;
 assert(!modelFilesystemMounted());assert(infoQueries==1); // Unrelated mount is not ours.
 namedMounted=true;nativeMounted=false;infoResult=-1;assert(!modelFilesystemMounted());
 infoResult=ESP_OK;totalBytes=0;usedBytes=0;assert(!modelFilesystemMounted());
 totalBytes=4096;usedBytes=4097;assert(!modelFilesystemMounted());
 usedBytes=0;assert(modelFilesystemMounted()); // Empty but mounted is readable.
 puts("Model filesystem SDK boundary: exact named partition, no native-subtype fallback, mount/info/range failures passed");
}
