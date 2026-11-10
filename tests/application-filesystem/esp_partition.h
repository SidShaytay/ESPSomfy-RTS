#pragma once
#include <cstdint>
#define ESP_PARTITION_TYPE_DATA 1
#define ESP_PARTITION_SUBTYPE_ANY 0xff
#define ESP_PARTITION_SUBTYPE_DATA_SPIFFS 0x82
#define ESP_PARTITION_SUBTYPE_DATA_LITTLEFS 0x83
struct esp_partition_t {uint8_t type,subtype;uint32_t address,size;char label[17];};
extern esp_partition_t mountedPartition,updatePartition;
const esp_partition_t *esp_partition_find_first(int,int,const char *);
