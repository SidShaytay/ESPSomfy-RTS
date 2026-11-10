#pragma once
#include <stdint.h>
constexpr int ESP_OK=0;
struct esp_partition_t { const char *label; };
struct esp_app_desc_t { uint8_t app_elf_sha256[32]; };
inline const esp_partition_t *esp_ota_get_running_partition() { return nullptr; }
inline int esp_ota_get_partition_description(const esp_partition_t *,esp_app_desc_t *) { return -1; }
