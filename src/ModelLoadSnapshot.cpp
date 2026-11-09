#include "ModelLoadSnapshot.h"
#include <errno.h>
#include <stdlib.h>
#include <stdint.h>
#if defined(ESP_PLATFORM)
#include <esp_littlefs.h>
#endif
#if defined(ESPSOMFY_LILYGO_T_EMBED_CC1101)
#include <esp_heap_caps.h>
#endif
namespace {
bool mounted=false;
void *allocate(size_t n){
#if defined(ESPSOMFY_LILYGO_T_EMBED_CC1101)
  return heap_caps_malloc(n,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
#else
  return malloc(n);
#endif
}
void release(void *p,size_t){free(p);}
const ModelLoadIo operations{fopen,stat,fread,ferror,fclose,allocate,release};
}
void noteModelFilesystemMounted(bool value){mounted=value;}
bool modelFilesystemMounted(){
#if defined(ESP_PLATFORM)
  size_t total=0,used=0;
  return mounted && esp_littlefs_mounted(nullptr) &&
    esp_littlefs_info(nullptr,&total,&used)==ESP_OK && total>0 && used<=total;
#else
  return mounted;
#endif
}
const ModelLoadIo &modelLoadIo(){return operations;}
void ModelLoadSnapshot::clear(){
  if(bytes){volatile unsigned char *p=bytes;for(size_t i=0;i<length;++i)p[i]=0;releaseBytes(bytes,length);}
  bytes=nullptr;length=0;releaseBytes=nullptr;
}
ModelLoadStatus ModelLoadSnapshot::capture(const char *path,bool mounted,const ModelLoadIo &io){
  clear();if(!mounted||!path||!*path)return ModelLoadStatus::unavailable;
  struct stat info{};
  errno=0;const int status=io.stat(path,&info);const int statError=errno;
  if(status!=0){
    if(statError!=ENOENT)return ModelLoadStatus::unavailable;
    errno=0;FILE *probe=io.open(path,"rb");const int openError=errno;
    if(probe){io.close(probe);return ModelLoadStatus::unavailable;}
    return openError==ENOENT?ModelLoadStatus::absent:ModelLoadStatus::unavailable;
  }
  if(!S_ISREG(info.st_mode)||info.st_size<=0||uint64_t(info.st_size)>maximumBytes)return ModelLoadStatus::unavailable;
  FILE *file=io.open(path,"rb");if(!file)return ModelLoadStatus::unavailable;
  length=size_t(info.st_size);releaseBytes=io.release;bytes=static_cast<unsigned char *>(io.allocate(length));
  bool ok=bytes!=nullptr;
  size_t offset=0;
  while(ok&&offset<length){const size_t chunk=length-offset>1024?1024:length-offset;const size_t got=io.read(bytes+offset,1,chunk,file);ok=got==chunk&&!io.error(file);offset+=got;}
  unsigned char extra=0;
  if(ok)ok=io.read(&extra,1,1,file)==0&&!io.error(file);
  if(io.close(file)!=0)ok=false;
  if(!ok){clear();return ModelLoadStatus::unavailable;}
  return ModelLoadStatus::loaded;
}
