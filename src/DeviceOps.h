#ifndef DEVICE_OPS_H
#define DEVICE_OPS_H
#include <stdint.h>
#include <ArduinoJson.h>
#include "LoopQueue.h"
#include "ProtoSettings.h"
#include "SettingsApply.h"
#include "SettingsRevisions.h"

// Socket protocol 2's settings ops and the WiFi scan, run on loop()
// (docs/PROTOCOL.md).  loop() also reads the settings while it runs the
// network, MQTT and the radio, so it is the one that changes them.

// A saveSettings on its way to loop(), which frees it.
struct settings_save_t {
  settings_section_t section = settings_section_t::general;
  // The client's address, for the token a security save answers with.
  uint32_t ip = 0;
  // The route's request, from expandSettingsPayload().
  JsonDocument request;
};
// async_tcp: a save to fill in and hand to loop().  One runs at a time,
// whichever client sent it: nullptr and busy while another is waiting, and
// nullptr when there is no memory for it.
settings_save_t *newSettingsSave(bool &busy);
// Frees a save, run or refused, so another may follow.
void freeSettingsSave(settings_save_t *save);
// loop() only: applies a request already validated/expanded by
// expandSettingsPayload. Shared by socket and local administration callers;
// owns runtime effects, browser notification/session revocation and required
// restart scheduling. Zero origin means LCD; a socket origin queues its reply
// before returning to ProtoSocket::loop(), which drains it before closure.
settings_apply_result_t applyDeviceSettings(settings_section_t section, JsonObject request, uint32_t originSocketClient = 0);
// loop(): runs a proto_device item and replies.
void runDeviceOp(const loop_item_t &item);
#endif
