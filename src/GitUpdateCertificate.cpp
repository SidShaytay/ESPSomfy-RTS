#if defined(ARDUINO) && (defined(GIT_UPDATE_HOST_TEST) || defined(GIT_UPDATE_CERTIFICATE_HOST_TEST))
#error "Updater host test hooks are forbidden in firmware"
#endif
#include "GitUpdateCertificate.h"
#if defined(ESPSOMFY_LILYGO_T_EMBED_CC1101) || defined(GIT_UPDATE_CERTIFICATE_HOST_TEST)
#include <esp_crt_bundle.h>
#include <esp_idf_version.h>
#include <mbedtls/build_info.h>
#include <type_traits>
#include <utility>
#include <limits>
#include <cstddef>
#include <cstring>

// Private field READS below are tied to this exact reviewed prebuilt SDK. A
// compiler -D cannot enable date checks in its already-built X.509 verifier.
#if ESP_IDF_VERSION != ESP_IDF_VERSION_VAL(5,5,5) || MBEDTLS_VERSION_NUMBER != 0x03060600
#error "Updater certificate adapter requires reviewed IDF 5.5.5 / Mbed TLS 3.6.6"
#endif
#ifndef MBEDTLS_CONFIG_FILE
#error "Updater certificate adapter requires the pinned esp_config.h selection"
#endif
#if defined(MBEDTLS_USER_CONFIG_FILE) || defined(MBEDTLS_PSA_CRYPTO_USER_CONFIG_FILE)
#error "Updater certificate adapter does not permit unreviewed Mbed TLS configuration overrides"
#endif
namespace {
constexpr bool pinnedConfig(const char *a,const char *b) {
  while(*a && *a==*b) { ++a;++b; }
  return *a==*b;
}
}
static_assert(pinnedConfig(MBEDTLS_CONFIG_FILE,"mbedtls/esp_config.h"),"Updater certificate adapter configuration differs from SDK");
static_assert(std::is_same<decltype(std::declval<mbedtls_ssl_config>().MBEDTLS_PRIVATE(f_vrfy)),GitUpdateCertificate::Verify>::value,
  "Pinned bundle verification callback type changed");
static_assert(std::is_same<decltype(std::declval<mbedtls_ssl_config>().MBEDTLS_PRIVATE(p_vrfy)),void *>::value,
  "Pinned bundle verification context type changed");
// Values below fingerprint the actual reviewed prebuilt SDK ABI. Access still
// uses typed member names; no numeric offset is used to read/write memory.
#if defined(GIT_UPDATE_HOST_TEST) || defined(GIT_UPDATE_CERTIFICATE_HOST_TEST)
static_assert(sizeof(mbedtls_ssl_config)==2*sizeof(void *) && alignof(mbedtls_ssl_config)==alignof(void *) &&
  offsetof(mbedtls_ssl_config,MBEDTLS_PRIVATE(f_vrfy))==0 &&
  offsetof(mbedtls_ssl_config,MBEDTLS_PRIVATE(p_vrfy))==sizeof(void *),"Host certificate fixture layout changed");
#else
static_assert(sizeof(mbedtls_ssl_config)==196 && alignof(mbedtls_ssl_config)==4 &&
  offsetof(mbedtls_ssl_config,MBEDTLS_PRIVATE(f_vrfy))==64 &&
  offsetof(mbedtls_ssl_config,MBEDTLS_PRIVATE(p_vrfy))==68,
  "Updater certificate adapter ABI differs from prebuilt SDK; feature overrides are unsupported");
#endif
static_assert(sizeof(time_t)>=8 && std::numeric_limits<time_t>::is_signed,
  "Updater requires the pinned SDK's signed 64-bit UTC clock");
namespace {
thread_local GitUpdateCertificate *factory=nullptr;
time_t currentClock() { return std::time(nullptr); }
bool valid(const mbedtls_x509_time &value) {
  if(value.year<1 || value.year>9999 || value.mon<1 || value.mon>12 || value.day<1 ||
      value.hour<0 || value.hour>23 || value.min<0 || value.min>59 || value.sec<0 || value.sec>59) return false;
  const int days[]={31,28,31,30,31,30,31,31,30,31,30,31};
  const bool leap=value.year%4==0 && (value.year%100!=0 || value.year%400==0);
  return value.day<=days[value.mon-1]+(value.mon==2 && leap ? 1 : 0);
}
int compare(const mbedtls_x509_time &a,const mbedtls_x509_time &b) {
  const int left[]={a.year,a.mon,a.day,a.hour,a.min,a.sec};
  const int right[]={b.year,b.mon,b.day,b.hour,b.min,b.sec};
  for(size_t i=0;i<6;i++) if(left[i]!=right[i]) return left[i]<right[i] ? -1 : 1;
  return 0;
}
}
namespace {
uint32_t little(const uint8_t *p,unsigned bytes) {
  uint32_t value=0;
  for(unsigned i=0;i<bytes;i++) value|=uint32_t(p[i])<<(8*i);
  return value;
}
bool equal(const mbedtls_x509_buf &raw,const uint8_t *data,size_t length) {
  return raw.p && raw.len==length && std::memcmp(raw.p,data,length)==0;
}
}
// ESP-IDF 5.5 bundle layout (gen_crt_bundle.py): little-endian u32 offset per
// root, then per root u16 subject length, u16 key length, subject DER and
// SubjectPublicKeyInfo DER. Any inconsistency means no match.
bool GitUpdateCertificate::bundledAnchor(const mbedtls_x509_crt &certificate,const uint8_t *bundle,size_t size) {
  if(!bundle || size<4 || !certificate.subject_raw.len || !certificate.pk_raw.len) return false;
  const uint32_t table=little(bundle,4);
  if(!table || table%4 || table>size) return false;
  bool found=false;
  for(uint32_t i=0;i<table/4;i++) {
    const uint32_t offset=little(bundle+4*i,4);
    if(offset<table || offset>size || size-offset<4) return false;
    const size_t nameLength=little(bundle+offset,2),keyLength=little(bundle+offset+2,2);
    if(size-offset-4<nameLength+keyLength) return false;
    const uint8_t *name=bundle+offset+4;
    found|=equal(certificate.subject_raw,name,nameLength) && equal(certificate.pk_raw,name+nameLength,keyLength);
  }
  return found;
}
#if defined(GIT_UPDATE_HOST_TEST) && !defined(GIT_UPDATE_CERTIFICATE_HOST_TEST)
// The worker host test's fake TLS chain never presents a bundled root.
bool GitUpdateCertificate::linkedBundle(const uint8_t *&,size_t &) { return false; }
#elif !defined(GIT_UPDATE_CERTIFICATE_HOST_TEST)
extern const uint8_t sdkBundleStart[] asm("_binary_x509_crt_bundle_start");
extern const uint8_t sdkBundleEnd[] asm("_binary_x509_crt_bundle_end");
bool GitUpdateCertificate::linkedBundle(const uint8_t *&data,size_t &size) {
  if(sdkBundleEnd<=sdkBundleStart) return false;
  data=sdkBundleStart;size=size_t(sdkBundleEnd-sdkBundleStart);return true;
}
#endif
GitUpdateCertificate::GitUpdateCertificate(int64_t floor,Clock clock):floor(floor),clock(clock ? clock : currentClock) {}
GitUpdateCertificate::Scope::Scope(GitUpdateCertificate &session):previous(factory) { factory=&session; }
GitUpdateCertificate::Scope::~Scope() { factory=previous; }
esp_err_t GitUpdateCertificate::attach(void *configuration) {
  if(!factory || factory->attached) return ESP_ERR_INVALID_STATE;
  if(!configuration) return ESP_ERR_INVALID_ARG;
  auto &session=*factory;
  const esp_err_t attached=esp_crt_bundle_attach(configuration);
  if(attached!=ESP_OK) { session.bundleFailed=true;return attached; }
  auto *config=static_cast<mbedtls_ssl_config *>(configuration);
  // Preserve both values EXACTLY, including a null opaque pointer. Only this
  // one configuration receives our public setter; other clients are untouched.
  const auto original=config->MBEDTLS_PRIVATE(f_vrfy);
  if(!original || original==verify) { session.bundleFailed=true;return ESP_FAIL; }
  session.original=original;session.opaque=config->MBEDTLS_PRIVATE(p_vrfy);
  session.attached=true;
  mbedtls_ssl_conf_verify(config,verify,&session);
  return ESP_OK;
}
int GitUpdateCertificate::verify(void *context,mbedtls_x509_crt *certificate,int depth,uint32_t *flags) {
  auto *session=static_cast<GitUpdateCertificate *>(context);
  if(!session || !session->original || !certificate || !flags || depth<0) return MBEDTLS_ERR_X509_FATAL_ERROR;
  // The bundle may legitimately clear NOT_TRUSTED after checking its root key.
  // Preserve its resulting flags and exact return; add failures only afterwards.
  const int64_t started=session->monotonic ? session->monotonic() : 0;
  if(session->monotonic && session->firstVerify<0) session->firstVerify=started;
  // mbedTLS has already checked every signature below the untrusted top of the
  // chain. When that top certificate is itself a bundled root (same subject and
  // key), it is the trust anchor, so skip checking its cross-signature. GitHub
  // sends Sectigo Root E46 cross-signed by USERTrust ECC; verifying that P-384
  // signature took ~3 s and pushed the handshake past GitHub's ~10 s limit.
  const uint8_t *bundle=nullptr;size_t bundleSize=0;
  int result=0;
  if((*flags&~uint32_t(MBEDTLS_X509_BADCERT_BAD_MD))==MBEDTLS_X509_BADCERT_NOT_TRUSTED &&
      linkedBundle(bundle,bundleSize) && bundledAnchor(*certificate,bundle,bundleSize)) {
    *flags=0;session->presentedAnchor=true;
  } else {
    result=session->original(session->opaque,certificate,depth,flags);
  }
  if(session->monotonic) session->bundleVerify+=session->monotonic()-started;
  session->bundleFailed|=result!=0 || *flags!=0;
  if(depth==0) session->leafSeen=true;
  auto reject=[&](Failure failure,uint32_t flag) {
    if(session->timeFailure==Failure::none) session->timeFailure=failure;
    *flags|=flag;
  };
  const int64_t now=static_cast<int64_t>(session->clock());
  if(session->floor<1577836800 || now<session->floor || now<0 ||
      (session->anchored && (now<session->anchor || now-session->anchor>180))) {
    reject(Failure::clock,MBEDTLS_X509_BADCERT_OTHER);return result;
  }
  if(!session->anchored) { session->anchor=now;session->anchored=true; }
  const time_t timestamp=static_cast<time_t>(session->anchor);
  tm utc;
  if(!gmtime_r(&timestamp,&utc) || utc.tm_year<70 || utc.tm_year>8099) {
    reject(Failure::clock,MBEDTLS_X509_BADCERT_OTHER);return result;
  }
  const mbedtls_x509_time current={utc.tm_year+1900,utc.tm_mon+1,utc.tm_mday,utc.tm_hour,utc.tm_min,utc.tm_sec};
  if(!valid(current) || !valid(certificate->valid_from) || !valid(certificate->valid_to) ||
      compare(certificate->valid_from,certificate->valid_to)>0) {
    reject(Failure::calendar,MBEDTLS_X509_BADCERT_OTHER);return result;
  }
  if(compare(current,certificate->valid_from)<0) reject(Failure::future,MBEDTLS_X509_BADCERT_FUTURE);
  if(compare(current,certificate->valid_to)>0) reject(Failure::expired,MBEDTLS_X509_BADCERT_EXPIRED);
  return result;
}
#endif
