#include <cassert>
#include <fstream>
#include <iterator>
#include <string>
#include "GitUpdateAssets.h"
#include "LittleFS.h"
#include <esp_task_wdt.h>
#include "dirent.h"
#include <mbedtls/sha256.h>
std::string fixtureRoot,failRead;
int rootFailure=0,hashFailure=0;
bool failSeek=false,stopRequested=false;
unsigned activeFiles=0,activeDirectories=0,activeHashes=0,yields=0,stopAfter=0;
static bool stopped() { return stopRequested; }
static void cooperate() { if(++yields==stopAfter) stopRequested=true; }
std::string assetTestCompatibility=INITIAL_COMPAT;
std::vector<Entry> rootEntries={{"appversion",DT_REG},{"index.js.gz",DT_REG},{"empty.css",DT_REG},{"folder",DT_DIR},{"folder.css.gz",DT_REG},{"update-assets.txt",DT_REG}};
TestFS LittleFS;
bool fileDirectory(const char *name) {
  for(const auto &entry:rootEntries) if(std::string("/")+entry.name==name) return entry.kind==DT_DIR;
  return false;
}
static std::string read(const char *path) {
  std::ifstream input(fixtureRoot+path,std::ios::binary);
  return std::string(std::istreambuf_iterator<char>(input),{});
}
static void write(const char *path,const std::string &data) {
  std::ofstream output(fixtureRoot+path,std::ios::binary);output<<data;
}
static std::string compatibility(const std::string &bytes) {
  mbedtls_sha256_context ctx;mbedtls_sha256_init(&ctx);
  assert(mbedtls_sha256_starts(&ctx,0)==0);
  assert(mbedtls_sha256_update(&ctx,reinterpret_cast<const unsigned char *>(bytes.data()),bytes.size())==0);
  uint8_t digest[32];assert(mbedtls_sha256_finish(&ctx,digest)==0);mbedtls_sha256_free(&ctx);
  const char *digits="0123456789abcdef";std::string out="sha256:";
  for(auto b:digest) {out+=digits[b>>4];out+=digits[b&15];}return out;
}
int main(int argc,char **argv) {
  assert(argc==2);fixtureRoot=argv[1];
  fakeTaskWdt.subscribed=true;
  stopRequested=true;
  assert(gitUpdateAssetsStatus({stopped,cooperate})==UpdateAttestation::interrupted);
  assert(activeFiles==0 && activeDirectories==0 && activeHashes==0);
  stopRequested=false;
  for(unsigned nth=1;nth<=50;nth++) {
    yields=0;stopAfter=nth;
    const auto result=gitUpdateAssetsStatus({stopped,cooperate});
    assert(result==(stopRequested ? UpdateAttestation::interrupted : UpdateAttestation::verified));
    assert(activeFiles==0 && activeDirectories==0 && activeHashes==0);
    stopRequested=false;
  }
  assert(gitUpdateAssetsMatch());
  assert(fakeTaskWdt.resets>=9);
  rootEntries.push_back({"unexpected.js",DT_REG});
  assert(!gitUpdateAssetsMatch());
  rootEntries.pop_back();
  for(auto kind:{DT_DIR,0}) {
    rootEntries.push_back({"unlisted.js",static_cast<unsigned char>(kind)});
    assert(gitUpdateAssetsStatus()==UpdateAttestation::unexpectedAsset);
    rootEntries.pop_back();
  }
  for(const char *extra:{"index.js","unknown.js","unknown.css.gz","appversion.gz"}) {
    rootEntries.push_back({extra,DT_REG});
    assert(gitUpdateAssetsStatus()==UpdateAttestation::unexpectedAsset);
    rootEntries.pop_back();
  }
  const auto previousType=rootEntries.front().kind;
  rootEntries.front().kind=DT_DIR;
  assert(gitUpdateAssetsStatus()==UpdateAttestation::invalid);
  rootEntries.front().kind=previousType;
  for(int failure:{1,2,3}) {
    rootFailure=failure;
    assert(gitUpdateAssetsStatus()==UpdateAttestation::directoryError);
  }
  rootFailure=0;
  const auto originals=rootEntries;
  rootEntries.clear();
  assert(gitUpdateAssetsStatus()==UpdateAttestation::directoryError);
  rootEntries=originals;
  rootEntries.erase(rootEntries.begin()); // metadata iterator cannot omit a verified root asset
  assert(gitUpdateAssetsStatus()==UpdateAttestation::directoryError);
  rootEntries=originals;
  rootEntries.push_back(rootEntries.front());
  assert(gitUpdateAssetsStatus()==UpdateAttestation::directoryError);
  rootEntries=originals;
  rootEntries.push_back({std::string(128,'a'),DT_REG});
  assert(gitUpdateAssetsStatus()==UpdateAttestation::bounds);
  rootEntries=originals;
  for(size_t i=rootEntries.size();i<257;i++) rootEntries.push_back({"private.cfg",DT_REG});
  assert(gitUpdateAssetsStatus()==UpdateAttestation::bounds);
  rootEntries=originals;
  for(int failure:{1,2,3}) {
    hashFailure=failure;
    assert(gitUpdateAssetsStatus()==UpdateAttestation::hashError);
  }
  hashFailure=0;
  failSeek=true;
  assert(gitUpdateAssetsStatus()==UpdateAttestation::readError);
  failSeek=false;
  rootEntries.push_back({"shades.cfg",DT_REG});
  rootEntries.push_back({"shades.tmp",DT_REG});
  write("/shades.cfg","private config outside asset manifest");
  assert(gitUpdateAssetsMatch());
  std::string asset=read("/index.js.gz");
  std::string corrupt=asset;corrupt[4]^=1;
  write("/index.js.gz",corrupt);
  assert(!gitUpdateAssetsMatch()); // same size, different payload
  write("/index.js.gz",asset);
  assert(gitUpdateAssetsMatch());
  failRead="/index.js.gz";
  assert(!gitUpdateAssetsMatch()); // short read cannot confirm health
  failRead.clear();
  std::string version=read("/appversion");
  std::remove((fixtureRoot+"/appversion").c_str());
  assert(!gitUpdateAssetsMatch());
  write("/appversion",version);
  std::string manifest=read("/update-assets.txt");
  // Authenticate intentionally malformed manifests to exercise the parser,
  // independently of the earlier manifest-digest rejection gate.
  const std::string header="ESPSOMFY_ASSETS_V1\n";
  const std::string zero(64,'0');
  for(const std::string &bad:{std::string("bad-header\n"),header,header+zero+" 000 /empty.css\n",
      header+zero+" 184467440737095516160 /empty.css\n",
      header+zero+" 0 /../private.cfg\n",header+zero+" 0 /empty.css",
      header+std::string(225,'x')+"\n",header+zero+" 0 /empty.css\n"+zero+" 0 /empty.css\n"}) {
    write("/update-assets.txt",bad);assetTestCompatibility=compatibility(bad);
    assert(!gitUpdateAssetsMatch());
  }
  std::string tooMany=header;
  const std::string emptyHash=compatibility("").substr(7);
  for(unsigned i=0;i<129;i++) {
    char name[32];snprintf(name,sizeof(name),"/extra%03u.css",i);
    write(name,"");
    tooMany+=emptyHash+" 0 "+name+"\n";
  }
  write("/update-assets.txt",tooMany);assetTestCompatibility=compatibility(tooMany);
  assert(gitUpdateAssetsStatus()==UpdateAttestation::bounds);
  for(unsigned i=0;i<129;i++) {
    char name[32];snprintf(name,sizeof(name),"/extra%03u.css",i);
    std::remove((fixtureRoot+name).c_str());
  }
  write("/update-assets.txt",std::string(16385,'x'));
  assert(gitUpdateAssetsStatus()==UpdateAttestation::bounds);
  assetTestCompatibility=INITIAL_COMPAT;
  write("/update-assets.txt",manifest);
  write("/update-assets.txt",manifest+"unexpected\n");
  assert(!gitUpdateAssetsMatch());
  write("/update-assets.txt",manifest);
  assert(gitUpdateAssetsMatch());
  std::remove((fixtureRoot+"/update-assets.txt").c_str());
  assert(!gitUpdateAssetsMatch()); // old filesystem must bootstrap manually
}
