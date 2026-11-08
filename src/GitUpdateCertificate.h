#pragma once
#if defined(ESPSOMFY_LILYGO_T_EMBED_CC1101) || defined(GIT_UPDATE_CERTIFICATE_HOST_TEST)
#include <cstdint>
#include <ctime>
#include <esp_err.h>
#include <mbedtls/ssl.h>

// Updater-client-only certificate time checking for the exact pinned SDK.
// The original bundle verifier/context are delegated to without replacement
// outside this one SSL configuration. Session must outlive client cleanup.
class GitUpdateCertificate {
public:
  enum class Failure : uint8_t { none,clock,calendar,expired,future,bundle };
  using Clock=time_t (*)();
  using Verify=int (*)(void *,mbedtls_x509_crt *,int,uint32_t *);
  explicit GitUpdateCertificate(int64_t floor,Clock clock=nullptr);
  GitUpdateCertificate(const GitUpdateCertificate &)=delete;
  GitUpdateCertificate &operator=(const GitUpdateCertificate &)=delete;
  Failure failure() const { return timeFailure; }
  bool complete() const { return attached && leafSeen && !bundleFailed && timeFailure==Failure::none; }
  bool rejectedByBundle() const { return bundleFailed; }
  // Dynamic factory scope is task-local and nestable. Verify callbacks retain
  // the per-config pointer; they never consult the factory after attachment.
  class Scope {
    GitUpdateCertificate *previous;
  public:
    explicit Scope(GitUpdateCertificate &session);
    ~Scope();
    Scope(const Scope &)=delete;
    Scope &operator=(const Scope &)=delete;
  };
  static esp_err_t attach(void *configuration);
private:
  int64_t floor,anchor=0;
  Clock clock;
  Verify original=nullptr;
  void *opaque=nullptr;
  bool attached=false,anchored=false,leafSeen=false,bundleFailed=false;
  Failure timeFailure=Failure::none;
  static int verify(void *,mbedtls_x509_crt *,int,uint32_t *);
};

#endif
