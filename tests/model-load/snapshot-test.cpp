#include "ModelLoadSnapshot.h"
#include "ModelLoadTrust.h"
#include <cassert>
#include <cstring>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <sys/stat.h>
namespace {
unsigned opens=0, closes=0, frees=0;bool readFail=false, closeFail=false, allocFail=false, missing=false, statFail=false, directory=false;size_t declared=4;
FILE *openFile(const char *,const char *) { ++opens;if(missing){errno=ENOENT;return nullptr;}return reinterpret_cast<FILE *>(1); }
int statFile(const char *,struct stat *out) {if(statFail){errno=EIO;return -1;}if(missing){errno=ENOENT;return -1;}out->st_mode=directory?S_IFDIR:S_IFREG;out->st_size=declared;return 0;}
size_t readFile(void *out,size_t,size_t count,FILE *) {if(readFail)return 0;if(count==1)return 0;memcpy(out,"abc\n",count);return count;}
int errorFile(FILE *){return readFail;}
int closeFile(FILE *){++closes;return closeFail?-1:0;}
void *allocate(size_t n){return allocFail?nullptr:malloc(n);}
void release(void *p,size_t n){for(size_t i=0;i<n;++i)assert(static_cast<char *>(p)[i]==0);++frees;free(p);}
ModelLoadIo io{openFile,statFile,readFile,errorFile,closeFile,allocate,release};
void reset(){readFail=closeFail=allocFail=missing=statFail=directory=false;declared=4;}
}
int main(){
 ModelLoadTrust trust;assert(!trust.usable()&&!trust.mayCommit());
 trust.bootResult(ModelLoadStatus::absent);assert(trust.usable()&&trust.mayCommit());
 trust.fail();assert(!trust.usable()&&!trust.mayCommit());
 trust.beginRecoveryCommit();assert(!trust.usable()&&trust.mayCommit());
 trust.finishRecoveryCommit(false);assert(!trust.usable()&&!trust.mayCommit());
 trust.beginRecoveryCommit();trust.finishRecoveryCommit(true);assert(trust.usable());
 ModelLoadSnapshot snapshot;
 assert(snapshot.capture("/fs/shades.cfg",false,io)==ModelLoadStatus::unavailable && !opens);
 assert(snapshot.capture("/fs/shades.cfg",true,io)==ModelLoadStatus::loaded);
 assert(snapshot.size()==4 && !memcmp(snapshot.data(),"abc\n",4));snapshot.clear();assert(frees==1);
 missing=true;assert(snapshot.capture("/fs/shades.cfg",true,io)==ModelLoadStatus::absent);
 statFail=true;assert(snapshot.capture("/fs/shades.cfg",true,io)==ModelLoadStatus::unavailable);reset();
 directory=true;assert(snapshot.capture("/fs/shades.cfg",true,io)==ModelLoadStatus::unavailable);reset();
 declared=ModelLoadSnapshot::maximumBytes+1;assert(snapshot.capture("/fs/shades.cfg",true,io)==ModelLoadStatus::unavailable);reset();
 allocFail=true;assert(snapshot.capture("/fs/shades.cfg",true,io)==ModelLoadStatus::unavailable);reset();
 readFail=true;assert(snapshot.capture("/fs/shades.cfg",true,io)==ModelLoadStatus::unavailable && !snapshot.data());reset();
 closeFail=true;assert(snapshot.capture("/fs/shades.cfg",true,io)==ModelLoadStatus::unavailable && !snapshot.data());reset();
 declared=0;assert(snapshot.capture("/fs/shades.cfg",true,io)==ModelLoadStatus::unavailable);reset();
 assert(closes>0);puts("Model load snapshot: mounted, exact absence, bounded allocation, complete reads/close and wipe passed");
}
