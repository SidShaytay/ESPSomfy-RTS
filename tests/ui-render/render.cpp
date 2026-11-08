#include "LocalUiRenderer.h"
#include <vector>
#include <assert.h>
#include <stdio.h>
#include <string.h>
using namespace LocalUi;
static void save(const char *directory, const char *name, const std::vector<uint16_t> &buffer) {
  char path[512];
  snprintf(path, sizeof(path), "%s/%s.ppm", directory, name);
  FILE *f = fopen(path, "wb");
  assert(f);
  fprintf(f, "P6\n320 170\n255\n");
  for(size_t i = 1; i < buffer.size() - 1; i++) {
    const uint16_t c = buffer[i];
    const uint8_t rgb[] = {uint8_t((c >> 11) * 255 / 31), uint8_t(((c >> 5) & 63) * 255 / 63),
                           uint8_t((c & 31) * 255 / 31)};
    fwrite(rgb, 1, 3, f);
  }
  fclose(f);
}
int main(int argc, char **argv) {
  assert(argc == 2);
  std::vector<uint16_t> buffer(screenWidth * screenHeight + 2, 0xdead);
  Canvas c;
  c.attach(buffer.data() + 1);
  Model m;
  m.ready = true;
  strcpy(m.targets[0].name, "Master Bedroom");
  strcpy(m.targets[1].name, "Master Bathroom");
  strcpy(m.targets[2].name, "Both rooms");
  for(auto &t : m.targets) {
    t.valid = true;
    t.open[0] = 45;
    t.open[1] = 20;
  }
  m.targets[0].moving = true;
  StatusDisplay::Snapshot s;
  s.running = true;
  s.battery = {BatteryGauge::State::ready, 74, 3920, -100};
  s.link = StatusDisplay::Link::connected;
  s.web = StatusDisplay::Web::listening;
  s.radio = StatusDisplay::Radio::ready;
  s.uptimeSeconds = 4140;
  strcpy(s.address, "192.0.2.12");
  strcpy(s.build, "Plus UI #15 v4.0.1");
  strcpy(s.firmwareVersion, "v4.0.1");
  strcpy(s.applicationVersion, "4.0.2");
  View v;
  const Screen screens[] = {Screen::status, Screen::choose, Screen::control, Screen::details};
  const char *names[] = {"status", "choose", "control", "details"};
  for(int i = 0; i < 4; i++) {
    v.screen = screens[i];
    c.paint(m, v, s);
    save(argv[1], names[i], buffer);
    assert(buffer.front() == 0xdead && buffer.back() == 0xdead);
  }
  // Version values and scrolling must each affect the actual rendered pixels.
  auto initialDetails = buffer;
  strcpy(s.applicationVersion, "4.0.3");
  c.paint(m, v, s);
  assert(buffer != initialDetails);
  strcpy(s.applicationVersion, "4.0.2");
  strcpy(s.firmwareVersion, "v4.0.2");
  c.paint(m, v, s);
  assert(buffer != initialDetails);
  strcpy(s.firmwareVersion, "v4.0.1");
  v.detailsOffset = detailsRowCount - detailsVisibleRows;
  c.paint(m, v, s);
  assert(buffer != initialDetails);
  save(argv[1], "details-scrolled", buffer);
  v.detailsOffset = 0;
  v.screen = Screen::control;
  v.selected = 2;
  c.paint(m, v, s);
  save(argv[1], "group", buffer);
  v.screen = Screen::status;
  s.link = StatusDisplay::Link::disconnected;
  s.web = StatusDisplay::Web::error;
  s.radio = StatusDisplay::Radio::unavailable;
  c.paint(m, v, s);
  save(argv[1], "disconnected", buffer);
  v.screen = Screen::choose;
  v.selected = 3;
  strcpy(m.targets[1].name, "A very long roomname");
  c.paint(m, v, s);
  save(argv[1], "status-choice", buffer);
  v.screen = Screen::brightness;
  c.paint(m, v, s);
  save(argv[1], "brightness", buffer);
  v.dimSeconds=v.offSeconds=0;v.setting=2;c.paint(m,v,s);save(argv[1],"no-timeouts",buffer);
  v.screen=Screen::unlock; v.credentialMaximum=4; v.credentialLength=4;
  v.credentialChoice=CredentialEditor::doneChoice;
  c.paint(m,v,s); save(argv[1],"unlock-pin",buffer);
  auto maskedPin=buffer;
  v.credentialMaximum=32; v.credentialLength=32; v.credentialUser=true;
  c.paint(m,v,s); save(argv[1],"unlock-username",buffer);
  assert(buffer!=maskedPin);
  v.credentialUser=false; v.credentialByte=true; v.credentialChoice=0xc3;
  c.paint(m,v,s); save(argv[1],"unlock-byte",buffer);
  v.retryAfterMs=30000; v.notice=Notice::unlockWait;
  c.paint(m,v,s); save(argv[1],"unlock-throttled",buffer);
  assert(buffer.front() == 0xdead && buffer.back() == 0xdead);
  puts("Real GFX renderer produced eight 320x170 screens with buffer bounds intact.");
}
