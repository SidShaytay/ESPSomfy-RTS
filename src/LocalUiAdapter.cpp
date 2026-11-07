#ifdef ESPSOMFY_LOCAL_UI
#include "LocalUiAdapter.h"
#include "Somfy.h"
#include "ShadeRequests.h"
#include <Preferences.h>
#include <math.h>
#include <string.h>
extern SomfyShadeController somfy;
extern ShadeRequests shadeRequests;
namespace LocalUi {
namespace {
void name(char *out, const char *input) {
  for(int i = 0; i < 20 && input[i]; i++)
    out[i] = (input[i] >= 32 && input[i] <= 126) ? input[i] : '?';
}
void readShade(uint8_t id, Target &t) {
  auto *shade = somfy.getShadeById(id);
  if(!shade)
    return;
  name(t.name, shade->name);
  t.identity = shade->getRemoteAddress();
  // This initial controller UX is only for the existing RTS roller shades.
  t.valid = shade->paired && shade->proto == radio_proto::RTS && shade->shadeType == shade_types::roller &&
            t.identity != 0;
  t.moving = !shade->isIdle();
  const float pos = shade->state().currentPos;
  if(isfinite(pos) && pos >= 0 && pos <= 100)
    t.open[0] = 100 - shade->transformPosition(pos);
}
} // namespace
uint8_t loadBrightness() {
  Preferences prefs;
  if(!prefs.begin("LCDUI", true))
    return 32;
  const uint8_t value = prefs.getUChar("brightness", 32);
  prefs.end();
  return value;
}
bool saveBrightness(uint8_t value) {
  Preferences prefs;
  if(!prefs.begin("LCDUI"))
    return false;
  const bool ok = prefs.putUChar("brightness", value) == 1;
  prefs.end();
  return ok;
}
Model snapshotModel(bool running) {
  Model m;
  if(!running)
    return m;
#ifdef ESPSOMFY_LOCAL_COMMANDS
  m.controlsEnabled = true;
#endif
  m.ready = somfy.transceiver.available() && !somfy.transceiver.txSuspended();
  // Existing model IDs, not RF identities or names; model renames are reflected.
  readShade(1, m.targets[0]);
  readShade(2, m.targets[1]);
  auto *group = somfy.getGroupById(1);
  if(!group)
    return m;
  auto &t = m.targets[2];
  name(t.name, group->name);
  bool one = false, two = false, other = false;
  for(auto id : group->linkedShades) {
    if(id == 1)
      one = true;
    else if(id == 2)
      two = true;
    else if(id != 0 && id != 255)
      other = true;
  }
  t.identity = group->getRemoteAddress();
  t.members[0] = m.targets[0].identity;
  t.members[1] = m.targets[1].identity;
  t.valid = one && two && !other && m.targets[0].valid && m.targets[1].valid &&
            group->proto == radio_proto::RTS && group->getRemoteAddress() != 0;
  if(!t.valid)
    t.identity = 0;
  t.moving = m.targets[0].moving || m.targets[1].moving;
  t.open[0] = m.targets[0].open[0];
  t.open[1] = m.targets[1].open[0];
  return m;
}
bool dispatch(const Command &cmd) {
#ifndef ESPSOMFY_LOCAL_COMMANDS
  (void)cmd;
  return false; // Preview binary contains no local request submission.
#else
  if(cmd.selected >= 3 ||
     (cmd.action != Action::up && cmd.action != Action::down && cmd.action != Action::stop))
    return false;
  const Model current = snapshotModel(true);
  const auto &target = current.targets[cmd.selected];
  if(!current.ready || !target.valid || target.identity != cmd.identity ||
     target.members[0] != cmd.members[0] || target.members[1] != cmd.members[1])
    return false;
  shade_request_t request;
  if(cmd.action == Action::stop) {
    // Never broadcast MY to a mixed/idle group. Recheck individual movement
    // immediately on the model's owning loop, then reuse its command dispatcher.
    bool sent = false;
    for(uint8_t i = 0; i < 2; i++) {
      if(cmd.selected != 2 && cmd.selected != i)
        continue;
      auto *shade = somfy.getShadeById(i + 1);
      if(!shade || shade->isIdle())
        continue;
      request.id = i + 1;
      request.command = somfy_commands::My;
      if(shadeRequests.run(request) != shade_request_result_t::done)
        return false;
      sent = true;
    }
    return sent;
  }
  request.id = cmd.selected == 2 ? 1 : cmd.selected + 1;
  request.type =
      cmd.selected == 2 ? shade_request_type_t::group_command : shade_request_type_t::shade_command;
  request.command = cmd.action == Action::up ? somfy_commands::Up : somfy_commands::Down;
  return shadeRequests.run(request) == shade_request_result_t::done;
#endif
}
} // namespace LocalUi
#endif
