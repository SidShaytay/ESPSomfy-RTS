#pragma once
#include <stddef.h>
constexpr int ESP_OK=0;
bool esp_littlefs_mounted(const char *partitionLabel);
int esp_littlefs_info(const char *partitionLabel,size_t *total,size_t *used);
