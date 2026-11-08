#include "GitUpdateCertificate.h"
#include <esp_crt_bundle.h>
#include <cassert>
#include <thread>
#include <vector>
#include <atomic>
static time_t now=1709164800; // 2024-02-29 00:00:00 UTC
static time_t clockNow() { return now; }
static int opaqueSentinel=123,originalResult=0;
static uint32_t originalFlags=0;
static unsigned called=0;
static bool missingCallback=false,failAttach=false,leaveCallback=false,nullOpaque=false;
static int bundle(void *opaque,mbedtls_x509_crt *,int,uint32_t *flags) {
  assert(opaque==(nullOpaque ? nullptr : &opaqueSentinel));++called;
  *flags&=~MBEDTLS_X509_BADCERT_NOT_TRUSTED;
  *flags|=originalFlags;
  return originalResult;
}
esp_err_t esp_crt_bundle_attach(void *config) {
  if(failAttach) return ESP_FAIL;
  if(leaveCallback) return ESP_OK;
  auto *ssl=static_cast<mbedtls_ssl_config *>(config);
  mbedtls_ssl_conf_verify(ssl,missingCallback ? nullptr : bundle,nullOpaque ? nullptr : &opaqueSentinel);return ESP_OK;
}
static mbedtls_x509_crt cert() { return {{2024,2,28,0,0,0},{2024,3,1,0,0,0}}; }
static uint32_t check(GitUpdateCertificate &session,mbedtls_ssl_config &config,mbedtls_x509_crt certificate,int depth=0,int expected=0,uint32_t flags=MBEDTLS_X509_BADCERT_NOT_TRUSTED) {
  const auto result=config.MBEDTLS_PRIVATE(f_vrfy)(config.MBEDTLS_PRIVATE(p_vrfy),&certificate,depth,&flags);
  assert(result==expected);(void)session;return flags;
}
int main() {
  assert(GitUpdateCertificate::attach(nullptr)==ESP_ERR_INVALID_STATE);
  GitUpdateCertificate session(1700000000,clockNow);mbedtls_ssl_config config;
  {
    GitUpdateCertificate::Scope scope(session);assert(GitUpdateCertificate::attach(nullptr)==ESP_ERR_INVALID_ARG);assert(GitUpdateCertificate::attach(&config)==ESP_OK);
    auto good=cert();assert(check(session,config,good)==0);assert(session.complete());
    mbedtls_ssl_config second;assert(GitUpdateCertificate::attach(&second)==ESP_ERR_INVALID_STATE);
    assert(check(session,config,good)==0); // failed reuse cannot overwrite prior verifier/context
    GitUpdateCertificate nested(1700000000,clockNow);mbedtls_ssl_config nestedConfig;
    { GitUpdateCertificate::Scope inner(nested);assert(GitUpdateCertificate::attach(&nestedConfig)==ESP_OK); }
    assert(check(nested,nestedConfig,good)==0 && nested.complete()); // independent of factory slot
    assert(check(session,config,good)==0);
  }
  assert(GitUpdateCertificate::attach(&config)==ESP_ERR_INVALID_STATE);
  assert(check(session,config,cert())==0); // config still owns live session, no active scope needed
  for(int mode=0;mode<13;mode++) {
    GitUpdateCertificate s(1700000000,clockNow);mbedtls_ssl_config conf;auto c=cert();
    now=1709164800;originalFlags=0;originalResult=0;
    GitUpdateCertificate::Scope scope(s);assert(GitUpdateCertificate::attach(&conf)==ESP_OK);
    if(mode==0) c.valid_to={2024,2,28,23,59,59};
    if(mode==1) c.valid_from={2024,3,1,0,0,0};
    if(mode==2) c.valid_to={2023,2,29,0,0,0};
    if(mode==3) c.valid_from={2024,13,1,0,0,0};
    if(mode==4) now=0;
    if(mode==5) {originalFlags=0x4000;originalResult=-77;}
    if(mode==6) {c.valid_from={2024,2,29,0,0,0};c.valid_to=c.valid_from;}
    if(mode==7) {c.valid_from={2024,2,29,0,0,0};c.valid_to={2024,2,28,0,0,0};}
    if(mode==8) c.valid_to={2024,3,1,24,0,0};
    if(mode==9) c.valid_to={2100,2,29,0,0,0};
    if(mode==10) c.valid_from={2024,2,29,0,0,60};
    if(mode==12) originalFlags=0x4000;
    if(mode==11) {c.valid_from={2000,2,29,0,0,0};c.valid_to={2024,2,29,0,0,0};}
    const auto flags=check(s,conf,c,0,originalResult);
    if(mode==5 || mode==12) assert(flags==originalFlags && s.rejectedByBundle() && !s.complete());
    else if(mode==6 || mode==11) assert(flags==0 && s.complete()); // both inclusive boundaries
    else assert(flags!=0 && !s.complete());
  }
  now=1709164800;originalFlags=0;originalResult=0;
  GitUpdateCertificate chain(1700000000,clockNow);mbedtls_ssl_config chainConfig;
  {GitUpdateCertificate::Scope scope(chain);assert(GitUpdateCertificate::attach(&chainConfig)==ESP_OK);}
  auto intermediate=cert();intermediate.valid_to={2024,2,28,0,0,0};
  assert(check(chain,chainConfig,intermediate,1)!=0);
  assert(check(chain,chainConfig,cert(),0)==0 && !chain.complete()); // earlier chain failure persists
  GitUpdateCertificate clocked(1700000000,clockNow);mbedtls_ssl_config clockConfig;
  {GitUpdateCertificate::Scope scope(clocked);assert(GitUpdateCertificate::attach(&clockConfig)==ESP_OK);}
  assert(check(clocked,clockConfig,cert(),1)==0);--now;
  assert(check(clocked,clockConfig,cert(),0)!=0 && clocked.failure()==GitUpdateCertificate::Failure::clock);
  now=1709164800;
  GitUpdateCertificate zero(1700000000,clockNow);mbedtls_ssl_config zeroConfig;
  {GitUpdateCertificate::Scope scope(zero);assert(GitUpdateCertificate::attach(&zeroConfig)==ESP_OK);}
  assert(check(zero,zeroConfig,{})!=0); // never silently skip zero-date certificates
  GitUpdateCertificate jumped(1700000000,clockNow);mbedtls_ssl_config jumpConfig;
  {GitUpdateCertificate::Scope scope(jumped);assert(GitUpdateCertificate::attach(&jumpConfig)==ESP_OK);}
  assert(check(jumped,jumpConfig,cert(),1)==0);now+=181;
  assert(check(jumped,jumpConfig,cert(),0)!=0 && jumped.failure()==GitUpdateCertificate::Failure::clock);
  now=1709164800;
  leaveCallback=true;
  {GitUpdateCertificate s(1700000000,clockNow);GitUpdateCertificate::Scope scope(s);assert(GitUpdateCertificate::attach(&config)==ESP_FAIL);}
  leaveCallback=false;assert(check(session,config,cert())==0);
  GitUpdateCertificate noFloor(0,clockNow);mbedtls_ssl_config noFloorConfig;
  {GitUpdateCertificate::Scope scope(noFloor);assert(GitUpdateCertificate::attach(&noFloorConfig)==ESP_OK);}
  assert(check(noFloor,noFloorConfig,cert())!=0);
  missingCallback=true;
  {GitUpdateCertificate s(1700000000,clockNow);GitUpdateCertificate::Scope scope(s);mbedtls_ssl_config c;assert(GitUpdateCertificate::attach(&c)==ESP_FAIL);}
  missingCallback=false;failAttach=true;
  {GitUpdateCertificate s(1700000000,clockNow);GitUpdateCertificate::Scope scope(s);mbedtls_ssl_config c;assert(GitUpdateCertificate::attach(&c)==ESP_FAIL);}
  failAttach=false;
  nullOpaque=true;
  {GitUpdateCertificate s(1700000000,clockNow);mbedtls_ssl_config c;GitUpdateCertificate::Scope scope(s);
    assert(GitUpdateCertificate::attach(&c)==ESP_OK);assert(check(s,c,cert())==0 && s.complete());}
  nullOpaque=false;
  {GitUpdateCertificate outer(1700000000,clockNow);mbedtls_ssl_config out;GitUpdateCertificate::Scope scope(outer);
    {GitUpdateCertificate inner(1700000000,clockNow);mbedtls_ssl_config in;GitUpdateCertificate::Scope nested(inner);
      assert(GitUpdateCertificate::attach(&in)==ESP_OK);}
    assert(GitUpdateCertificate::attach(&out)==ESP_OK);assert(check(outer,out,cert())==0);}
  // A different client's original bundle attachment is never globally replaced.
  mbedtls_ssl_config other;assert(esp_crt_bundle_attach(&other)==ESP_OK);assert(other.MBEDTLS_PRIVATE(f_vrfy)==bundle);
  std::atomic<unsigned> completed{0},ready{0};std::vector<std::thread> threads;
  for(unsigned n=0;n<4;n++) threads.emplace_back([&] {
    GitUpdateCertificate s(1700000000,clockNow);mbedtls_ssl_config c;GitUpdateCertificate::Scope scope(s);
    ++ready;while(ready<4) std::this_thread::yield();
    assert(GitUpdateCertificate::attach(&c)==ESP_OK);++completed;
    while(completed<4) std::this_thread::yield();
  });
  for(auto &t:threads) t.join();
  assert(completed==4);
}
