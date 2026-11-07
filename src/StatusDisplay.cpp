#include "StatusDisplay.h"
#include <stdio.h>
#include <string.h>

namespace StatusDisplay {
void format(const Snapshot &s, Page &p) {
  snprintf(p.lines[0], sizeof(p.lines[0]), "%s up %luh %02um", s.running ? "RUN" : "BOOT",
    (unsigned long)(s.uptimeSeconds / 3600 % 1000000), (unsigned)(s.uptimeSeconds / 60 % 60));
  snprintf(p.lines[1], sizeof(p.lines[1]), "%s", s.graphicsFailure==1 ? "LCD: PSRAM not found" : s.graphicsFailure==2 ? "LCD: PSRAM alloc failed" : "Power: not monitored");
  const char *link = "disconnected";
  if(s.link == Link::connecting) link = "connecting";
  else if(s.link == Link::connected) link = "connected";
  else if(s.link == Link::accessPoint) link = "access point";
  snprintf(p.lines[2], sizeof(p.lines[2]), "WiFi: %s", link);
  const bool addressed = (s.link == Link::connected || s.link == Link::accessPoint) && s.address[0];
  snprintf(p.lines[3], sizeof(p.lines[3]), "IP: %s", addressed ? s.address : "unavailable");
  const char *web = s.web == Web::starting ? "starting" : s.web == Web::listening ? "listening" : "error";
  snprintf(p.lines[4], sizeof(p.lines[4]), "HTTP: %s", web);
  if(addressed && s.web == Web::listening) snprintf(p.lines[5], sizeof(p.lines[5]), "http://%.15s", s.address);
  else snprintf(p.lines[5], sizeof(p.lines[5]), "UI: unavailable");
  const char *radio = s.radio == Radio::initializing ? "initializing" : s.radio == Radio::ready ? "ready" : "unavailable";
  snprintf(p.lines[6], sizeof(p.lines[6]), "Radio: %s", radio);
  snprintf(p.lines[7], sizeof(p.lines[7]), "RF: %.2f MHz", s.frequency);
  snprintf(p.lines[8], sizeof(p.lines[8]), "%.26s", s.build);
}
void Session::tick(uint32_t now, const Snapshot &s, bool pressed) {
  if(failed) return;
  if(!initialized) {
    const auto result = output.initialize(now);
    if(result == Result::failed) { failed = true; output.brightness(0); return; }
    if(result != Result::ready) return;
    initialized = true; awakeSince = now;
  }
  if(pressed != input) { input = pressed; inputSince = now; }
  if(input != stableInput && uint32_t(now - inputSince) >= 40) {
    stableInput = input;
    if(stableInput) { awakeSince = now; sleepStage = 0; }
  }
  const auto age = uint32_t(now - awakeSince);
  if(offAfter && age >= offAfter) sleepStage = 2;
  else if(dimAfter && age >= dimAfter && sleepStage == 0) sleepStage = 1;
  output.brightness(sleepStage == 2 ? 0 : sleepStage == 1 ? (awakeLevel<4?1:4) : awakeLevel);
  if(sleepStage == 2 || (attempted && uint32_t(now - lastRow) < 20)) return;
  lastRow = now;
  attempted = true;
  Page desired;
  format(s, desired);
  for(uint8_t i = 0; i < 9; ++i) {
    if(!(paintedMask & (1u << i)) || strcmp(desired.lines[i], painted.lines[i]) != 0) {
      if(!output.row(i, desired.lines[i])) return; // busy bus: retry, never wait for TX
      strcpy(painted.lines[i], desired.lines[i]);
      paintedMask |= 1u << i;
      return; // at most one changed row per pass
    }
  }
}
}
