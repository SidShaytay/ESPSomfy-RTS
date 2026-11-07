#include <cassert>
#include <fstream>
#include <iterator>
#include <string>
#include "GitUpdateAssets.h"
#include "LittleFS.h"
#include <esp_task_wdt.h>
std::string fixtureRoot,failRead;
TestFS LittleFS;
static std::string read(const char *path) {
  std::ifstream input(fixtureRoot+path,std::ios::binary);
  return std::string(std::istreambuf_iterator<char>(input),{});
}
static void write(const char *path,const std::string &data) {
  std::ofstream output(fixtureRoot+path,std::ios::binary);output<<data;
}
int main(int argc,char **argv) {
  assert(argc==2);fixtureRoot=argv[1];
  fakeTaskWdt.subscribed=true;
  assert(gitUpdateAssetsMatch());
  assert(fakeTaskWdt.resets>=9);
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
  write("/update-assets.txt",manifest+"unexpected\n");
  assert(!gitUpdateAssetsMatch());
  write("/update-assets.txt",manifest);
  assert(gitUpdateAssetsMatch());
  std::remove((fixtureRoot+"/update-assets.txt").c_str());
  assert(!gitUpdateAssetsMatch()); // old filesystem must bootstrap manually
}
