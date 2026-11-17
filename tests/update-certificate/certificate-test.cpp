#include "GitUpdateCertificate.h"
#include <esp_crt_bundle.h>
#include <cassert>
#include <thread>
#include <vector>
#include <atomic>
#include <cstring>
#include <string>
// Bundle entries use ESP-IDF's layout: little-endian u32 offsets, then
// u16 subject length, u16 key length, subject DER, SubjectPublicKeyInfo DER.
static std::vector<uint8_t> linked;
bool GitUpdateCertificate::linkedBundle(const uint8_t *&data,size_t &size) {
  data=linked.empty() ? nullptr : linked.data();size=linked.size();return !linked.empty();
}
static std::vector<uint8_t> makeBundle(const std::vector<std::pair<std::string,std::string>> &roots) {
  std::vector<uint8_t> out(4*roots.size());
  for(size_t i=0;i<roots.size();i++) {
    const uint32_t offset=uint32_t(out.size());
    for(unsigned b=0;b<4;b++) out[4*i+b]=uint8_t(offset>>(8*b));
    const auto &[name,key]=roots[i];
    for(size_t length:{name.size(),key.size()}) { out.push_back(uint8_t(length));out.push_back(uint8_t(length>>8)); }
    out.insert(out.end(),name.begin(),name.end());out.insert(out.end(),key.begin(),key.end());
  }
  return out;
}
static std::string anchorName="CN=Root E46",anchorKey="P-384 key E46",otherName="CN=Other Root";
static mbedtls_x509_buf view(std::string &text) { mbedtls_x509_buf b;b.len=text.size();b.p=reinterpret_cast<unsigned char *>(text.data());return b; }
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
static mbedtls_x509_crt cert() { mbedtls_x509_crt c;c.valid_from={2024,2,28,0,0,0};c.valid_to={2024,3,1,0,0,0};return c; }
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
  // A presented certificate whose subject and key equal a bundled root is that
  // trust anchor: no extra signature check by its cross-signer (GitHub sends
  // Sectigo Root E46 cross-signed by USERTrust ECC; that P-384 check took ~3 s).
  linked=makeBundle({{otherName,"other key"},{anchorName,anchorKey}});
  std::string wrongKey="P-384 key E46 rotated",shortName="CN=Root E4";
  auto presented=[&](std::string &name,std::string &key) { auto c=cert();c.subject_raw=view(name);c.pk_raw=view(key);return c; };
  auto anchorCase=[&](mbedtls_x509_crt c,uint32_t flags,int depth,unsigned expectCalls,uint32_t expectFlags,bool expectAnchor) {
    GitUpdateCertificate s(1700000000,clockNow);mbedtls_ssl_config conf;
    {GitUpdateCertificate::Scope scope(s);assert(GitUpdateCertificate::attach(&conf)==ESP_OK);}
    originalFlags=0;originalResult=0;const unsigned before=called;
    const auto result=check(s,conf,c,depth,0,flags);
    assert(called-before==expectCalls && result==expectFlags);
    assert(bool(s.diagnosticState()&8)==expectAnchor);
    return s.complete();
  };
  // Untrusted top of chain matching a bundled root: accepted without the bundle.
  assert(!anchorCase(presented(anchorName,anchorKey),MBEDTLS_X509_BADCERT_NOT_TRUSTED,2,0,0,true));
  // The anchor's own weak signature hash is irrelevant, as in the bundle.
  anchorCase(presented(anchorName,anchorKey),MBEDTLS_X509_BADCERT_NOT_TRUSTED|MBEDTLS_X509_BADCERT_BAD_MD,2,0,0,true);
  // Same name with another key, a prefix name, or other failures still go to the bundle.
  anchorCase(presented(anchorName,wrongKey),MBEDTLS_X509_BADCERT_NOT_TRUSTED,2,1,0,false);
  anchorCase(presented(shortName,anchorKey),MBEDTLS_X509_BADCERT_NOT_TRUSTED,2,1,0,false);
  anchorCase(presented(anchorName,anchorKey),MBEDTLS_X509_BADCERT_NOT_TRUSTED|MBEDTLS_X509_BADCERT_EXPIRED,2,1,MBEDTLS_X509_BADCERT_EXPIRED,false);
  // Already trusted certificates are not re-examined.
  anchorCase(presented(anchorName,anchorKey),0,1,1,0,false);
  // Date checks still apply to an anchor.
  { auto c=presented(anchorName,anchorKey);c.valid_to={2024,2,28,0,0,0};
    anchorCase(c,MBEDTLS_X509_BADCERT_NOT_TRUSTED,2,0,MBEDTLS_X509_BADCERT_EXPIRED,true); }
  // A whole chain anchored this way completes.
  { GitUpdateCertificate s(1700000000,clockNow);mbedtls_ssl_config conf;
    {GitUpdateCertificate::Scope scope(s);assert(GitUpdateCertificate::attach(&conf)==ESP_OK);}
    assert(check(s,conf,presented(anchorName,anchorKey),2,0,MBEDTLS_X509_BADCERT_NOT_TRUSTED)==0);
    assert(check(s,conf,cert(),1,0,0)==0 && check(s,conf,cert(),0,0,0)==0 && s.complete()); }
  // Malformed bundles never match: truncated entry, offset past the end, empty.
  for(int damage=0;damage<4;damage++) {
    linked=makeBundle({{otherName,"other key"},{anchorName,anchorKey}});
    if(damage==0) linked.pop_back();
    if(damage==1) linked[4]=0xff;
    if(damage==2) linked.clear();
    if(damage==3) { linked[0]=3; }
    anchorCase(presented(anchorName,anchorKey),MBEDTLS_X509_BADCERT_NOT_TRUSTED,2,1,0,false);
  }
  linked.clear();
}
