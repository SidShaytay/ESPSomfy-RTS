#include "GitUpdateImage.h"
#include <cassert>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>
int hashFailure=0;unsigned activeHashes=0;
static unsigned yields=0,stopAt=0;
static bool stop(){return stopAt && yields>=stopAt;}
static void cooperate(){++yields;}
int main(int argc,char **argv) {
  assert(argc==6 || argc==7);const std::string mode=argv[1];
  std::ifstream file(argv[2],std::ios::binary);std::vector<uint8_t> bytes{std::istreambuf_iterator<char>(file),{}};
  using namespace GitUpdatePolicy;
  Manifest expected;expected.schema=3;expected.board=BOARD;expected.compatibleBoards={BOARD,GitUpdateProfile::plusBoard};
  expected.layout=LAYOUT;expected.assetsMode="bundled";expected.features=15;expected.flashBytes=FLASH_BYTES;expected.psramBytes=PSRAM_BYTES;
  expected.size=bytes.size();expected.sha256=argv[3];expected.appCompat=argv[4];expected.bootloaderSha=std::string(64,'b');
  expected.sourceSha=std::string(40,'a');expected.version="v4.0.3";expected.asset=FIRMWARE_ASSET;expected.channel="stable";
  auto *manifest=&expected;
  if(mode=="manual" || mode=="manualDev" || mode=="actual" || mode.rfind("manual:",0)==0)manifest=nullptr;
  if(mode=="wrongExpectedDigest")expected.sha256[0]=expected.sha256[0]=='a'?'b':'a';
  if(mode=="wrongExpectedWeb")expected.appCompat[7]=expected.appCompat[7]=='a'?'b':'a';
  if(mode=="wrongExpectedVersion")expected.version="v4.0.4";
  if(mode=="wrongExpectedSource")expected.sourceSha=std::string(40,'c');
  if(mode=="wrongExpectedSize")++expected.size;
  if(mode=="startHash")hashFailure=1;
  if(mode=="updateHash")hashFailure=2;
  if(mode=="finishHash")hashFailure=3;
  if(mode=="cancel")stopAt=1;
  if(mode=="cancelLate")stopAt=8;
  GitUpdateImage::Identity identity;strcpy(identity.sourceSha,"old stale output");
  const std::string boot=argc==7 ? argv[6] : std::string("ESPSOMFY_BOOT_ID:0:24576:")+std::string(64,'b');
  const auto result=GitUpdateImage::verify(bytes.data(),bytes.size(),boot.c_str(),manifest,identity,{stop,cooperate});
  const bool good=std::string(argv[5])=="yes";
  assert((result==GitUpdateImage::Result::verified)==good);assert(!activeHashes);
  if(good) {assert(mode=="actual" || std::strcmp(identity.sourceSha,expected.sourceSha.c_str())==0);assert(std::strcmp(identity.appCompat,argv[4])==0);assert(std::strcmp(identity.appVersion,"4.0.1")==0);assert(std::strcmp(identity.imageSha,argv[3])==0);}
  else assert(identity.sourceSha[0]==0 && identity.version[0]==0 && identity.appCompat[0]==0 && identity.imageSha[0]==0);
}
