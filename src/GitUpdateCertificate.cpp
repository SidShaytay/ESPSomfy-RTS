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
  const int result=session->original(session->opaque,certificate,depth,flags);
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
