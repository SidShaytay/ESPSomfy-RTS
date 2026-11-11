#pragma once
void testAuditLog(const char *, ...);
#define ESP_LOGI(tag, ...) testAuditLog(__VA_ARGS__)
