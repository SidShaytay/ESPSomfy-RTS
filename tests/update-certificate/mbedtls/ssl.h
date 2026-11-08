#pragma once
#include <cstdint>
#include "build_info.h"
struct mbedtls_x509_time { int year=0,mon=0,day=0,hour=0,min=0,sec=0; };
struct mbedtls_x509_crt { mbedtls_x509_time valid_from,valid_to; };
#define MBEDTLS_X509_BADCERT_EXPIRED 1u
#define MBEDTLS_X509_BADCERT_FUTURE 512u
#define MBEDTLS_X509_BADCERT_OTHER 256u
#define MBEDTLS_X509_BADCERT_NOT_TRUSTED 8u
#define MBEDTLS_ERR_X509_FATAL_ERROR -12288
struct mbedtls_ssl_config {
  int (*MBEDTLS_PRIVATE(f_vrfy))(void *,mbedtls_x509_crt *,int,uint32_t *)=nullptr;
  void *MBEDTLS_PRIVATE(p_vrfy)=nullptr;
};
inline void mbedtls_ssl_conf_verify(mbedtls_ssl_config *conf,int (*callback)(void *,mbedtls_x509_crt *,int,uint32_t *),void *context) {
  conf->MBEDTLS_PRIVATE(f_vrfy)=callback;conf->MBEDTLS_PRIVATE(p_vrfy)=context;
}
