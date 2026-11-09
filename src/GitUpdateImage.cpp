#include "GitUpdateImage.h"
#include "UpdateSha256.h"
#include "HttpAccess.h"
#include <cstring>
namespace GitUpdateImage { namespace {
constexpr size_t maximumManifest=16384,maximumAssets=128,maximumPayload=0x180000;
uint32_t little(const uint8_t *p){return uint32_t(p[0])|uint32_t(p[1])<<8|uint32_t(p[2])<<16|uint32_t(p[3])<<24;}
uint32_t big(const uint8_t *p){return uint32_t(p[0])<<24|uint32_t(p[1])<<16|uint32_t(p[2])<<8|uint32_t(p[3]);}
bool hex(const char *p,size_t n) {
  for(size_t i=0;i<n;++i)if(!((p[i]>='0'&&p[i]<='9')||(p[i]>='a'&&p[i]<='f')))return false;
  return true;
}
bool asciiName(char c){return (c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_';}
bool cooperate(UpdateAttestationControl control){control.cooperate();return !control.stopped();}
Result hash(const uint8_t *p,size_t n,char (&out)[65],UpdateAttestationControl control) {
  const auto status=updateSha256(p,n,out,control);
  return status==UpdateAttestation::verified ? Result::verified :
      status==UpdateAttestation::interrupted ? Result::interrupted : Result::hashError;
}
// Split comparisons are intentional. A live parser must not embed another
// complete sentinel in its own image and invalidate exactly-one marker checks.
bool prefix(const uint8_t *p,size_t n,const char *suffix,size_t suffixSize) {
  return n>=9+suffixSize && std::memcmp(p,"ESPSOMFY_",9)==0 && std::memcmp(p+9,suffix,suffixSize)==0;
}
struct Segment {size_t begin=0,size=0;};
bool contained(size_t at,size_t count,const Segment (&segments)[16],size_t used) {
  for(size_t i=0;i<used;++i)
    if(at>=segments[i].begin && at-segments[i].begin<=segments[i].size && count<=segments[i].size-(at-segments[i].begin))return true;
  return false;
}
bool field(const uint8_t *&p,const uint8_t *end,char *out,size_t capacity,char separator) {
  size_t n=0;
  while(p<end && *p!=uint8_t(separator)) {
    if(!*p || *p<32 || *p>126 || n+1>=capacity)return false;
    out[n++]=char(*p++);
  }
  if(!n || p==end)return false;
  out[n]=0;++p;return true;
}
bool identity(const uint8_t *p,size_t n,Identity &out) {
  const auto *end=p+n;char board[64],layout[64],mode[12];
  // The complete marker has already been bounded and NUL-terminated by caller.
  p+=19;
  if(end-p<2 || p[0]!='3' || p[1]!='|')return false;
  p+=2;
  if(!field(p,end,board,sizeof(board),'|') || std::strcmp(board,GitUpdateProfile::board) ||
     !field(p,end,layout,sizeof(layout),'|') || std::strcmp(layout,GitUpdateProfile::layout) ||
     !field(p,end,out.sourceSha,sizeof(out.sourceSha),'|') || std::strlen(out.sourceSha)!=40 || !hex(out.sourceSha,40) ||
     !field(p,end,out.version,sizeof(out.version),'|') || !field(p,end,mode,sizeof(mode),'|') || std::strcmp(mode,"bundled") ||
     !field(p,end,out.appCompat,sizeof(out.appCompat),'\0') || p!=end || std::strlen(out.appCompat)!=71 ||
     std::strncmp(out.appCompat,"sha256:",7) || !hex(out.appCompat+7,64))return false;
  if(GitUpdatePolicy::stableVersion(out.version))return true;
  const size_t versionSize=std::strlen(out.version);
  return (versionSize==16 || versionSize==22) && std::strncmp(out.version,"dev-",4)==0 &&
      std::memcmp(out.version+4,out.sourceSha,12)==0 && (versionSize==16 || std::strcmp(out.version+16,"-dirty")==0);
}
struct Record {char digest[65]={},path[128]={},logical[128]={};size_t size=0;bool gzip=false;};
bool record(const uint8_t *&p,const uint8_t *end,Record &out) {
  if(end-p<69)return false;
  std::memcpy(out.digest,p,64);out.digest[64]=0;p+=64;
  if(!hex(out.digest,64) || *p++!=' ')return false;
  if(p==end || *p<'0' || *p>'9')return false;
  const bool zero=*p=='0';size_t digits=0,size=0;
  while(p<end && *p>='0' && *p<='9') {
    if((zero && digits) || size>(maximumPayload-size_t(*p-'0'))/10)return false;
    size=size*10+size_t(*p++-'0');++digits;
  }
  if(p==end || *p++!=' ' || p==end || *p!='/')return false;
  size_t n=0;
  while(p<end && *p!='\n') {
    if(n+1>=sizeof(out.path))return false;
    const char c=char(*p++);
    if((n==0 && c!='/') || (n==1 && !asciiName(c)) || (n>1 && !asciiName(c) && c!='.' && c!='-'))return false;
    out.path[n++]=c;
  }
  if(p==end || n<2)return false;
  ++p;out.path[n]=0;out.size=size;
  if(std::strstr(out.path,".."))return false;
  std::memcpy(out.logical,out.path,n+1);
  out.gzip=n>=3 && std::strcmp(out.logical+n-3,".gz")==0;
  if(out.gzip)out.logical[n-3]=0;
  return isWebAsset(out.logical) && !(out.gzip && std::strcmp(out.logical,"/appversion")==0);
}
Result bundle(const uint8_t *bytes,size_t size,size_t at,const Segment (&segments)[16],size_t used,Identity &out,UpdateAttestationControl control) {
  constexpr size_t magicSize=9+sizeof("WEB_BUNDLE_V1"); // Prefix + WEB_BUNDLE_V1 + terminating NUL.
  if(at>size || size-at<magicSize+4)return Result::bundle;
  const size_t manifestSize=big(bytes+at+magicSize),start=at+magicSize+4;
  if(!manifestSize || manifestSize>maximumManifest || manifestSize>size-start || !contained(at,magicSize+4+manifestSize,segments,used))return Result::bundle;
  const auto *manifest=bytes+start,*end=manifest+manifestSize;
  constexpr char header[]="ESPSOMFY_ASSETS_V1\n";
  if(manifestSize<sizeof(header) || std::memcmp(manifest,header,sizeof(header)-1))return Result::bundle;
  char digest[65];auto status=hash(manifest,manifestSize,digest,control);if(status!=Result::verified)return status;
  if(std::strcmp(digest,out.appCompat+7))return Result::bundle;
  const auto *begin=manifest+sizeof(header)-1,*p=begin;
  size_t offset=start+manifestSize,count=0,total=0;char previous[128]={};uint8_t required=0;
  while(p<end) {
    if(!cooperate(control))return Result::interrupted;
    const auto *current=p;Record entry;
    if(!record(p,end,entry) || ++count>maximumAssets || std::strcmp(previous,entry.path)>=0 || entry.size>maximumPayload-total || entry.size>size-offset)return Result::bundle;
    std::strcpy(previous,entry.path);total+=entry.size;
    // Re-scan the bounded manifest instead of putting a 16KiB name table on
    // the worker's internal stack. Reject compressed/plain aliases exactly.
    const auto *earlier=begin;
    while(earlier<current) {
      Record seen;if(!record(earlier,end,seen) || std::strcmp(seen.logical,entry.logical)==0)return Result::bundle;
      if(!cooperate(control))return Result::interrupted;
    }
    if(!contained(offset,entry.size,segments,used))return Result::bundle;
    status=hash(bytes+offset,entry.size,digest,control);if(status!=Result::verified)return status;
    if(std::strcmp(digest,entry.digest))return Result::bundle;
    if(entry.gzip && (entry.size<18 || bytes[offset]!=0x1f || bytes[offset+1]!=0x8b ||
        bytes[offset+2]!=8 || bytes[offset+3]!=0 || little(bytes+offset+4)!=0 ||
        little(bytes+offset+entry.size-4)>8u*1024u*1024u))return Result::bundle;
    if(std::strcmp(entry.logical,"/index.html")==0)required|=1;
    if(std::strcmp(entry.logical,"/index.js")==0)required|=2;
    if(std::strcmp(entry.logical,"/app.css")==0)required|=4;
    if(std::strcmp(entry.logical,"/appversion")==0) {
      if(!entry.size || entry.size>32)return Result::bundle;
      required|=8;size_t first=0,last=entry.size;
      const auto *version=bytes+offset;
      const auto whitespace=[](uint8_t c){return c==' '||c=='\n'||c=='\r'||c=='\t';};
      while(first<last && whitespace(version[first]))++first;
      while(last>first && whitespace(version[last-1]))--last;
      if(last-first==0 || last-first>=sizeof(out.appVersion))return Result::bundle;
      for(size_t i=first;i<last;++i) {
        const char c=char(version[i]);
        if((i==first && (!asciiName(c) || c=='_')) || (!asciiName(c) && c!='.' && c!='-'))return Result::bundle;
      }
      std::memcpy(out.appVersion,version+first,last-first);out.appVersion[last-first]=0;
    }
    offset+=entry.size;
  }
  return count && required==15 ? Result::verified : Result::bundle;
}
} // namespace
Result verify(const uint8_t *bytes,size_t size,const char *bootloaderIdentity,const GitUpdatePolicy::Manifest *expected,Identity &identityOut,UpdateAttestationControl control) {
  identityOut={};Identity out;
  if(control.stopped())return Result::interrupted;
  if(!bytes || size<1024 || size>GitUpdateProfile::appBytes)return Result::bounds;
  if(expected && (GitUpdatePolicy::check(*expected)!=GitUpdatePolicy::CheckResult::compatible || expected->size!=size))return Result::identity;
  if(bytes[0]!=0xe9 || bytes[1]<1 || bytes[1]>16 || bytes[2]!=2 || bytes[3]!=0x4f || bytes[12]!=9 || bytes[13]!=0 || bytes[23]!=1)return Result::header;
  size_t position=24;uint8_t checksum=0xef;Segment segments[16];
  for(size_t i=0;i<bytes[1];++i) {
    if(size-position<8)return Result::segments;
    const size_t length=little(bytes+position+4);const auto address=little(bytes+position);position+=8;
    if(length>size-position)return Result::segments;
    if(i==0 && (length<256 || address<0x3c000000 || address>=0x3e000000 || little(bytes+position)!=0xabcd5432 ||
        std::memcmp(bytes+position+112,"v5.5.5\0",7)))return Result::description;
    segments[i]={position,length};
    for(size_t j=0;j<length;++j) {
      checksum^=bytes[position+j];
      if((j&1023)==0 && !cooperate(control))return Result::interrupted;
    }
    position+=length;
  }
  const size_t end=(position+16)/16*16;
  if(end>size || size-end!=32 || bytes[end-1]!=checksum)return Result::checksum;
  char digest[65];auto status=hash(bytes,end,digest,control);if(status!=Result::verified)return status;
  uint8_t appended[32];
  constexpr char digits[]="0123456789abcdef";
  for(size_t i=0;i<32;++i)appended[i]=uint8_t((std::strchr(digits,digest[2*i])-digits)*16+(std::strchr(digits,digest[2*i+1])-digits));
  if(std::memcmp(bytes+end,appended,32))return Result::digest;
  status=hash(bytes,size,out.imageSha,control);if(status!=Result::verified)return status;
  if(expected && expected->sha256!=out.imageSha)return Result::digest;
  size_t markerAt=0,bootAt=0,bundleAt=0,markers=0,boots=0,bundles=0;
  for(size_t i=0;i<size;++i) {
    if((i&1023)==0 && !cooperate(control))return Result::interrupted;
    if(prefix(bytes+i,size-i,"UPDATE_ID:",10)){markerAt=i;++markers;}
    if(prefix(bytes+i,size-i,"BOOT_ID:",8)){bootAt=i;++boots;}
    if(prefix(bytes+i,size-i,"WEB_BUNDLE_V1\0",14)){bundleAt=i;++bundles;}
  }
  if(markers!=1)return Result::identity;
  const size_t markerLength=strnlen(reinterpret_cast<const char *>(bytes+markerAt),size-markerAt<320?size-markerAt:320);
  if(markerLength>=320 || markerLength==size-markerAt || !contained(markerAt,markerLength+1,segments,bytes[1]) || !identity(bytes+markerAt,markerLength+1,out))return Result::identity;
  if(expected && (expected->sourceSha!=out.sourceSha || expected->version!=out.version || expected->appCompat!=out.appCompat))return Result::identity;
  if(boots!=1 || !bootloaderIdentity)return Result::bootloader;
  const size_t bootLength=strnlen(bootloaderIdentity,128);
  if(bootLength>=128 || bootLength<65 || !prefix(reinterpret_cast<const uint8_t *>(bootloaderIdentity),bootLength,"BOOT_ID:0:",10) ||
      bootLength+1>size-bootAt || !contained(bootAt,bootLength+1,segments,bytes[1]) || std::memcmp(bytes+bootAt,bootloaderIdentity,bootLength+1))return Result::bootloader;
  if(expected && expected->bootloaderSha!=bootloaderIdentity+bootLength-64)return Result::bootloader;
  if(bundles!=1)return Result::bundle;
  status=bundle(bytes,size,bundleAt,segments,bytes[1],out,control);
  if(status!=Result::verified)return status;
  if(control.stopped())return Result::interrupted;
  identityOut=out;return Result::verified;
}
}
