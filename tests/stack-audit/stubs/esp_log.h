#pragma once
void testAuditLog(const char *, ...);
#if defined(CORE_DEBUG_LEVEL) && CORE_DEBUG_LEVEL >= 3
#define ESP_LOGI(tag, ...) testAuditLog(__VA_ARGS__)
#else
#define ESP_LOGI(tag, ...) ((void)0)
#endif
