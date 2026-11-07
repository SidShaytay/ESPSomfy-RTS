#include "GitUpdateBootloader.h"
#include <esp_flash.h>
#include <mbedtls/sha256.h>
#include <cassert>
#include <cstring>
#include <cstdint>

static uint8_t flash[1025];
static unsigned reads = 0, freed = 0;
static const char *mode;
int esp_flash_read(void *, void *out, uint32_t offset, uint32_t count) {
  assert(reinterpret_cast<uintptr_t>(out) % 4 == 0);
  assert(offset == 0x2000 + reads * 512);
  offset -= 0x2000;
  assert(offset + count <= sizeof(flash));
  assert(count == (offset < 1024 ? 512 : 1));
  ++reads;
  if(!strcmp(mode, "read-error") && offset == 512) return -1;
  memcpy(out, flash + offset, count);
  return ESP_OK;
}
void mbedtls_sha256_init(mbedtls_sha256_context *ctx) { ctx->sum = 0; }
int mbedtls_sha256_starts(mbedtls_sha256_context *, int is224) {
  assert(!is224);
  return !strcmp(mode, "start-error") ? -1 : 0;
}
int mbedtls_sha256_update(mbedtls_sha256_context *ctx, const unsigned char *data, size_t size) {
  if(!strcmp(mode, "hash-error")) return -1;
  for(size_t i = 0; i < size; ++i) ctx->sum += data[i];
  return 0;
}
int mbedtls_sha256_finish(mbedtls_sha256_context *ctx, unsigned char *out) {
  if(!strcmp(mode, "finish-error")) return -1;
  memset(out, 0, 32);
  out[0] = ctx->sum & 255;
  out[1] = (ctx->sum >> 8) & 255;
  return 0;
}
void mbedtls_sha256_free(mbedtls_sha256_context *) { ++freed; }
int main(int argc, char **argv) {
  assert(argc == 2);
  mode = argv[1];
  assert(!strcmp(gitUpdateBootloaderDigest(), "0104000000000000000000000000000000000000000000000000000000000000"));
  memset(flash, 1, sizeof(flash));
  if(!strcmp(mode, "tampered")) flash[512] = 2;
  const bool valid = !strcmp(mode, "valid");
  assert(gitUpdateBootloaderMatches() == valid);
  assert(freed == 1);
  if(valid) assert(reads == 3);
  const unsigned initialReads = reads;
  memset(flash, 9, sizeof(flash));
  assert(gitUpdateBootloaderMatches() == valid);
  assert(reads == initialReads && freed == 1);
}
