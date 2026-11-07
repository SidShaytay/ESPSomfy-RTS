#pragma once
#include <ArduinoJson.h>

// Requests are thread-safe and return immediately. Only loop() invokes the
// preparation callback or schedules a reboot; the worker never owns shade data.
class GitUpdateClass {
public:
  void begin(bool (*prepareInstall)() = nullptr);
  void loop();
  bool requestCheck();
  bool requestInstall();
  bool requestCancel();
  bool busy() const;
  void status(JsonObject destination) const;
  bool beginManualUpdate();
  void endManualUpdate();
};
extern GitUpdateClass gitUpdate;
