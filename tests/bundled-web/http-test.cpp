#include "BundledWeb.h"
#include "GitUpdateAssets.h"
#include "BundledWebRoutes.h"
#include "ESPAsyncWebServer.h"
#include "HttpAccess.h"
#include "UpdateSha256.h"
#include <mbedtls/sha256.h>
#include <cassert>
#include <cstring>
#include <cstdio>
#include <new>
int hashFailure=0;unsigned activeHashes=0;
static unsigned yields=0;static bool stopAfterYield=false;static unsigned stopAt=0;
static bool stopped(){return stopAfterYield;}
static unsigned finalHash=0;
static void stopOnFinish(){if(FakeSha256::finishes==finalHash)stopAfterYield=true;}
static void stopOnFree(){if(FakeSha256::frees==finalHash)stopAfterYield=true;}
static void cooperate(){if(++yields==stopAt)stopAfterYield=true;}
static bool failNothrow=false;
void *operator new(std::size_t n,const std::nothrow_t &)noexcept {
 if(failNothrow)return nullptr;
 try{return ::operator new(n);}catch(...){return nullptr;}
}
int main(){
 assert(!bundledWebRoutesReady());
 AsyncWebServer failed;failNothrow=true;assert(!registerBundledWebRoutes(failed));failNothrow=false;
 assert(!bundledWebRoutesReady()&&!failed.handler);
 failed.failRegistry=true;assert(!registerBundledWebRoutes(failed));assert(!bundledWebRoutesReady()&&!failed.handler);
 AsyncWebServer server;assert(registerBundledWebRoutes(server));assert(server.handler&&bundledWebRoutesReady());
 const auto *index=BundledWeb::find("/");assert(index&&index==BundledWeb::find("/index.html"));
 assert(std::strcmp(BundledWeb::sourceVersion(),"4.0.1")==0);
 for(const char *path:{"/","/index.html","/index.js","/app.css","/appversion"}){
  AsyncWebServerRequest r;r.path=path;assert(isPublicRoute(path,false)&&server.handler->canHandle(&r));
  const auto *asset=BundledWeb::find(path);server.handler->handleRequest(&r);
  assert(r.result.code==200&&r.result.mime==asset->mime&&r.result.data==asset->data&&r.result.size==asset->size);
  assert(r.result.headers["Cache-Control"]=="no-cache");assert(r.result.headers["ETag"]=='"'+String(asset->sha256)+'"');
  assert((r.result.headers["Content-Encoding"]=="gzip")==asset->gzip);
  r.incomingEtag=r.result.headers["ETag"];server.handler->handleRequest(&r);assert(r.result.code==304&&r.result.size==0);
 }
 for(const char *path:{"/shades.cfg","/shades.tmp","/secrets.json","/update-assets.txt","/unknown.js","/index.js.gz","/../index.js","/sub/index.js","/backup/a.js","//index.html"}){
  AsyncWebServerRequest r;r.path=path;assert(!BundledWeb::find(path)&&!server.handler->canHandle(&r));
 }
 AsyncWebServerRequest post;post.verb=HTTP_POST;assert(!server.handler->canHandle(&post));
 post.verb=HTTP_GET;post.http=false;assert(!server.handler->canHandle(&post));
 assert(gitUpdateAssetsStatus()==UpdateAttestation::verified&&gitUpdateAssetsMatch()&&!activeHashes);
 assert(BundledWeb::verify(ESPSOMFY_APP_COMPAT)==UpdateAttestation::verified&&!activeHashes);
 assert(BundledWeb::verify("sha256:0000000000000000000000000000000000000000000000000000000000000000")==UpdateAttestation::mismatch);
 UpdateAttestationControl cancel;cancel.stop=[](){return true;};
 assert(BundledWeb::verify(ESPSOMFY_APP_COMPAT,cancel)==UpdateAttestation::interrupted&&!activeHashes);
 for(stopAt=1;stopAt<=4;++stopAt){yields=0;stopAfterYield=false;UpdateAttestationControl interrupted{stopped,cooperate};assert(gitUpdateAssetsStatus(interrupted)==UpdateAttestation::interrupted&&yields==stopAt&&!activeHashes);}
 for(int failure=1;failure<=3;++failure){hashFailure=failure;assert(BundledWeb::verify(ESPSOMFY_APP_COMPAT)==UpdateAttestation::hashError&&!activeHashes);}
 hashFailure=0;stopAt=0;
 for(bool onFree:{false,true}) {
  stopAfterYield=false;char hex[65];
  finalHash=onFree ? FakeSha256::frees+1 : FakeSha256::finishes+1;
  if(onFree)FakeSha256::afterFree=stopOnFree;else FakeSha256::afterFinish=stopOnFinish;
  assert(updateSha256(index->data,index->size,hex,{stopped,nullptr})==UpdateAttestation::interrupted);
  assert(hex[0]==0 && !activeHashes);
  FakeSha256::afterFinish=FakeSha256::afterFree=nullptr;
  stopAfterYield=false;
  finalHash=onFree ? FakeSha256::frees+5 : FakeSha256::finishes+5;
  if(onFree)FakeSha256::afterFree=stopOnFree;else FakeSha256::afterFinish=stopOnFinish;
  assert(BundledWeb::verify(ESPSOMFY_APP_COMPAT,{stopped,nullptr})==UpdateAttestation::interrupted);
  assert(!activeHashes);FakeSha256::afterFinish=FakeSha256::afterFree=nullptr;
 }
 puts("Immutable bundle HTTP: exact paths, no filesystem fallback, gzip/MIME/cache, source version and byte attestation passed");
}
