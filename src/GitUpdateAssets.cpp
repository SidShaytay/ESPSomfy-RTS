#include "GitUpdateAssets.h"
#if defined(ESPSOMFY_LILYGO_T_EMBED_CC1101_PLUS)
#include <LittleFS.h>
#include "TaskWdt.h"
#include <mbedtls/sha256.h>
#include <cstdint>
#include <cstring>
#include <cstdlib>

#ifndef ESPSOMFY_APP_COMPAT
#error "The Plus build requires the packaged web compatibility digest"
#endif
namespace {
constexpr const char *MANIFEST="/update-assets.txt";
constexpr size_t MAX_MANIFEST=16384, MAX_FILE=0x180000, MAX_FILES=128;
bool hexDigest(const char *text) {
  if(strlen(text)!=64) return false;
  for(size_t i=0;i<64;i++) if(!((text[i]>='0' && text[i]<='9') || (text[i]>='a' && text[i]<='f'))) return false;
  return true;
}
bool hashFile(File &file, char (&hex)[65]) {
  mbedtls_sha256_context context;
  mbedtls_sha256_init(&context);
  resetTaskWdt();
  bool ok=mbedtls_sha256_starts(&context,0)==0;
  uint8_t buffer[1024];
  size_t remaining=file.size();
  while(ok && remaining) {
    const size_t count=remaining<sizeof(buffer) ? remaining : sizeof(buffer);
    ok=file.read(buffer,count)==count && mbedtls_sha256_update(&context,buffer,count)==0;
    if(ok) remaining-=count;
    resetTaskWdt();
  }
  uint8_t digest[32];
  ok=ok && mbedtls_sha256_finish(&context,digest)==0;
  mbedtls_sha256_free(&context);
  if(!ok) return false;
  constexpr char digits[]="0123456789abcdef";
  for(size_t i=0;i<32;i++) { hex[2*i]=digits[digest[i]>>4];hex[2*i+1]=digits[digest[i]&15]; }
  hex[64]=0;
  return true;
}
bool line(File &file,char *buffer,size_t capacity) {
  for(size_t n=0;n+1<capacity;n++) {
    uint8_t value;
    if(file.read(&value,1)!=1) return false;
    if(value=='\n') { buffer[n]=0;return true; }
    if(value<32 || value>126) return false;
    buffer[n]=static_cast<char>(value);
  }
  return false;
}
bool safePath(const char *name) {
  const size_t size=strlen(name);
  if(size<2 || size>127 || name[0]!='/' || strcmp(name,MANIFEST)==0) return false;
  bool first=true;
  for(const char *p=name+1;*p;p++) {
    if(*p=='/') { if(first) return false;first=true;continue; }
    const bool simple=(*p>='a' && *p<='z') || (*p>='A' && *p<='Z') || (*p>='0' && *p<='9') || *p=='_';
    if(!simple && (first || (*p!='.' && *p!='-'))) return false;
    first=false;
  }
  return !first;
}
}
bool gitUpdateAssetsMatch() {
  const char *compat=ESPSOMFY_APP_COMPAT;
  if(strncmp(compat,"sha256:",7)!=0 || !hexDigest(compat+7)) return false;
  File file=LittleFS.open(MANIFEST,"r");
  if(!file || !file.size() || file.size()>MAX_MANIFEST) return false;
  char digest[65];
  if(!hashFile(file,digest) || strcmp(digest,compat+7)!=0) return false;
  file.close();
  file=LittleFS.open(MANIFEST,"r");
  char record[224];
  if(!file || !line(file,record,sizeof(record)) || strcmp(record,"ESPSOMFY_ASSETS_V1")!=0) return false;
  size_t count=0,total=0;
  char previous[128]="";
  while(file.available()) {
    if(++count>MAX_FILES || !line(file,record,sizeof(record)) || strlen(record)<68 || record[64]!=' ') return false;
    record[64]=0;
    if(!hexDigest(record)) return false;
    char *sizeText=record+65;
    if(*sizeText<'0' || *sizeText>'9' || (*sizeText=='0' && sizeText[1]!=' ')) return false;
    // Bound the decimal parser before conversion, including overflow inputs.
    size_t bytes=0;
    char *name=sizeText;
    while(*name>='0' && *name<='9') {
      const size_t digit=*name++-'0';
      if(bytes>(MAX_FILE-digit)/10) return false;
      bytes=bytes*10+digit;
    }
    if(*name++!=' ' || !safePath(name) || strcmp(previous,name)>=0 || bytes>MAX_FILE-total) return false;
    total+=bytes;
    strcpy(previous,name);
    File asset=LittleFS.open(name,"r");
    if(!asset || asset.size()!=bytes || !hashFile(asset,digest) || strcmp(record,digest)!=0) return false;
    asset.close();
  }
  file.close();
  return count>0;
}
#else
bool gitUpdateAssetsMatch() { return false; }
#endif
