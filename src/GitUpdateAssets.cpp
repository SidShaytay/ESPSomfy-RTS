#include "GitUpdateAssets.h"
#include "GitUpdateProfile.h"
#if defined(ESPSOMFY_LILYGO_T_EMBED_CC1101)
#include <LittleFS.h>
#include "TaskWdt.h"
#include <mbedtls/sha256.h>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <dirent.h>
#include <errno.h>
#include "HttpAccess.h"

#ifndef ESPSOMFY_APP_COMPAT
#error "The verified update build requires the packaged web compatibility digest"
#endif
namespace {
constexpr const char *MANIFEST="/update-assets.txt";
constexpr size_t MAX_MANIFEST=16384, MAX_FILE=GitUpdateProfile::fsBytes, MAX_FILES=128;
bool hexDigest(const char *text) {
  if(strlen(text)!=64) return false;
  for(size_t i=0;i<64;i++) if(!((text[i]>='0' && text[i]<='9') || (text[i]>='a' && text[i]<='f'))) return false;
  return true;
}
UpdateAttestation hashFile(File &file, char (&hex)[65],UpdateAttestationControl control) {
  if(control.stopped()) return UpdateAttestation::interrupted;
  mbedtls_sha256_context context;
  mbedtls_sha256_init(&context);
  resetTaskWdt();
  UpdateAttestation result=mbedtls_sha256_starts(&context,0)==0 ? UpdateAttestation::verified : UpdateAttestation::hashError;
  uint8_t buffer[1024];
  size_t remaining=file.size();
  while(result == UpdateAttestation::verified && remaining) {
    if(control.stopped()) { result=UpdateAttestation::interrupted;break; }
    const size_t count=remaining<sizeof(buffer) ? remaining : sizeof(buffer);
    if(file.read(buffer,count)!=count) result=UpdateAttestation::readError;
    else if(mbedtls_sha256_update(&context,buffer,count)!=0) result=UpdateAttestation::hashError;
    remaining-=count;
    resetTaskWdt();
    control.cooperate();
    if(control.stopped()) result=UpdateAttestation::interrupted;
  }
  uint8_t digest[32];
  if(result == UpdateAttestation::verified && mbedtls_sha256_finish(&context,digest)!=0) result=UpdateAttestation::hashError;
  mbedtls_sha256_free(&context);
  if(result != UpdateAttestation::verified) return result;
  constexpr char digits[]="0123456789abcdef";
  for(size_t i=0;i<32;i++) { hex[2*i]=digits[digest[i]>>4];hex[2*i+1]=digits[digest[i]&15]; }
  hex[64]=0;
  return UpdateAttestation::verified;
}
bool line(File &file,char *buffer,size_t capacity,UpdateAttestationControl control) {
  for(size_t n=0;n+1<capacity;n++) {
    if(control.stopped()) return false;
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
bool servable(const char *name) {
  if(isWebAsset(name)) return true;
  const size_t length=strlen(name);
  if(length<4 || length>127 || strcmp(name+length-3,".gz")!=0) return false;
  char plain[128];
  memcpy(plain,name,length-3);plain[length-3]=0;
  return isWebAsset(plain);
}
// Re-scan the small authenticated manifest instead of retaining 16 KiB of
// filenames on the internal stack. No configuration file contents are read.
int listedRootAsset(File &manifest,const char *path,UpdateAttestationControl control) {
  if(control.stopped()) return -3;
  if(!manifest.seek(0)) return -2;
  char record[224];
  if(!line(manifest,record,sizeof(record),control)) return control.stopped() ? -3 : -2;
  for(size_t index=0;manifest.available() && index<MAX_FILES;index++) {
    if(control.stopped()) return -3;
    if(!line(manifest,record,sizeof(record),control)) return control.stopped() ? -3 : -2;
    const char *name=strchr(record,' ');
    if(!name || !(name=strchr(name+1,' '))) return -2;
    if(strcmp(name+1,path)==0) return static_cast<int>(index);
    resetTaskWdt();
    control.cooperate();
  }
  return control.stopped() ? -3 : -1;
}
UpdateAttestation rootAssets(File &manifest,size_t expectedRootFiles,UpdateAttestationControl control) {
  if(control.stopped()) return UpdateAttestation::interrupted;
  const char *mount=LittleFS.mountpoint();
  if(!mount || !mount[0]) return UpdateAttestation::directoryError;
  DIR *root=opendir(mount);
  if(!root) return UpdateAttestation::directoryError;
  UpdateAttestation result=UpdateAttestation::verified;
  size_t count=0,matched=0;
  uint32_t seen[4]={};
  bool sawManifest=false;
  while(true) {
    if(control.stopped()) { result=UpdateAttestation::interrupted;break; }
    errno=0;
    const dirent *entry=readdir(root);
    if(!entry) {
      if(errno) result=UpdateAttestation::directoryError;
      break;
    }
    control.cooperate();
    if(control.stopped()) { result=UpdateAttestation::interrupted;break; }
    if(++count>256) { result=UpdateAttestation::bounds;break; }
    const size_t length=strnlen(entry->d_name,sizeof(entry->d_name));
    if(length==0 || length>127) { result=UpdateAttestation::bounds;break; }
    if(strcmp(entry->d_name,".")==0 || strcmp(entry->d_name,"..")==0) continue;
    char path[129]="/";
    memcpy(path+1,entry->d_name,length+1);
    if(strcmp(path,MANIFEST)==0) { sawManifest=true;continue; }
    if(!servable(path)) continue;
    const int index=listedRootAsset(manifest,path,control);
    if(index==-3) { result=UpdateAttestation::interrupted;break; }
    if(index==-2) { result=UpdateAttestation::readError;break; }
    if(index<0) { result=UpdateAttestation::unexpectedAsset;break; }
    if(entry->d_type!=DT_REG) { result=UpdateAttestation::directoryError;break; }
    const uint32_t bit=uint32_t(1)<<(index%32);
    if(seen[index/32]&bit) { result=UpdateAttestation::directoryError;break; }
    seen[index/32]|=bit;
    ++matched;
    resetTaskWdt();
  }
  if(closedir(root)!=0 && result==UpdateAttestation::verified) result=UpdateAttestation::directoryError;
  if(result==UpdateAttestation::verified && (!sawManifest || matched!=expectedRootFiles)) result=UpdateAttestation::directoryError;
  return result;
}
}
UpdateAttestation gitUpdateAssetsStatus(UpdateAttestationControl control) {
  if(control.stopped()) return UpdateAttestation::interrupted;
  const char *compat=ESPSOMFY_APP_COMPAT;
  if(strncmp(compat,"sha256:",7)!=0 || !hexDigest(compat+7)) return UpdateAttestation::invalid;
  File file=LittleFS.open(MANIFEST,"r");
  if(!file) return UpdateAttestation::missing;
  if(file.isDirectory()) return UpdateAttestation::invalid;
  if(file.size()==0) return UpdateAttestation::invalid;
  if(file.size()>MAX_MANIFEST) return UpdateAttestation::bounds;
  char digest[65];
  auto hashed=hashFile(file,digest,control);
  if(hashed!=UpdateAttestation::verified) return hashed;
  if(strcmp(digest,compat+7)!=0) return UpdateAttestation::mismatch;
  if(control.stopped()) return UpdateAttestation::interrupted;
  if(!file.seek(0)) return UpdateAttestation::readError;
  char record[224];
  if(!line(file,record,sizeof(record),control) || strcmp(record,"ESPSOMFY_ASSETS_V1")!=0) return control.stopped() ? UpdateAttestation::interrupted : UpdateAttestation::invalid;
  size_t count=0,total=0,rootFiles=0;
  char previous[128]="";
  while(file.available()) {
    if(control.stopped()) return UpdateAttestation::interrupted;
    if(++count>MAX_FILES) return UpdateAttestation::bounds;
    if(!line(file,record,sizeof(record),control) || strlen(record)<68 || record[64]!=' ') return control.stopped() ? UpdateAttestation::interrupted : UpdateAttestation::invalid;
    record[64]=0;
    if(!hexDigest(record)) return UpdateAttestation::invalid;
    char *sizeText=record+65;
    if(*sizeText<'0' || *sizeText>'9' || (*sizeText=='0' && sizeText[1]!=' ')) return UpdateAttestation::invalid;
    size_t bytes=0;
    char *name=sizeText;
    while(*name>='0' && *name<='9') {
      const size_t digit=*name++-'0';
      if(bytes>(MAX_FILE-digit)/10) return UpdateAttestation::bounds;
      bytes=bytes*10+digit;
    }
    if(*name++!=' ' || !safePath(name) || strcmp(previous,name)>=0) return UpdateAttestation::invalid;
    if(bytes>MAX_FILE-total) return UpdateAttestation::bounds;
    total+=bytes;
    strcpy(previous,name);
    if(servable(name)) ++rootFiles;
    File asset=LittleFS.open(name,"r");
    if(!asset) return UpdateAttestation::missing;
    if(asset.isDirectory()) return UpdateAttestation::invalid;
    if(asset.size()!=bytes) return UpdateAttestation::mismatch;
    hashed=hashFile(asset,digest,control);
    if(hashed!=UpdateAttestation::verified) return hashed;
    if(strcmp(record,digest)!=0) return UpdateAttestation::mismatch;
    asset.close();
  }
  if(!count) return UpdateAttestation::invalid;
  const auto root=rootAssets(file,rootFiles,control);
  if(root!=UpdateAttestation::verified) return root;
  // Detect a manifest change during verification too. Future updater ownership
  // must exclude filesystem uploads; this remains a point-in-time check.
  if(control.stopped()) return UpdateAttestation::interrupted;
  if(!file.seek(0)) return UpdateAttestation::readError;
  hashed=hashFile(file,digest,control);
  if(hashed!=UpdateAttestation::verified) return hashed;
  return strcmp(digest,compat+7)==0 ? UpdateAttestation::verified : UpdateAttestation::mismatch;
}
bool gitUpdateAssetsMatch() { return updateAttested(gitUpdateAssetsStatus()); }
#else
UpdateAttestation gitUpdateAssetsStatus(UpdateAttestationControl) { return UpdateAttestation::unsupported; }
bool gitUpdateAssetsMatch() { return false; }
#endif
