#pragma once
// Test adapter uses the host's independent SHA256 oracle. Production links
// the pinned SDK's mbedTLS. The adapter preserves every streamed input byte.
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <vector>
extern int hashFailure;
struct mbedtls_sha256_context { std::vector<unsigned char> bytes; };
inline void mbedtls_sha256_init(mbedtls_sha256_context *context) { context->bytes.clear(); }
inline void mbedtls_sha256_free(mbedtls_sha256_context *) {}
inline int mbedtls_sha256_starts(mbedtls_sha256_context *context,int) { context->bytes.clear();return hashFailure==1 ? -1 : 0; }
inline int mbedtls_sha256_update(mbedtls_sha256_context *context,const unsigned char *data,size_t size) { if(hashFailure==2) return -1;context->bytes.insert(context->bytes.end(),data,data+size);return 0; }
inline int mbedtls_sha256_finish(mbedtls_sha256_context *context,unsigned char out[32]) {
  if(hashFailure==3) return -1;
  char name[]="/tmp/espsomfy-assets-oracle-XXXXXX";
  int fd=mkstemp(name);assert(fd>=0);
  size_t offset=0;
  while(offset<context->bytes.size()) {
    const ssize_t written=write(fd,context->bytes.data()+offset,context->bytes.size()-offset);
    assert(written>0);offset+=size_t(written);
  }
  close(fd);
  const std::string command=std::string("sha256sum ")+name;
  FILE *oracle=popen(command.c_str(),"r");assert(oracle);
  char hex[65];assert(fread(hex,1,64,oracle)==64);hex[64]=0;
  assert(pclose(oracle)==0);unlink(name);
  for(size_t i=0;i<32;i++) { char pair[3]={hex[2*i],hex[2*i+1],0};out[i]=static_cast<unsigned char>(strtoul(pair,nullptr,16)); }
  return 0;
}
