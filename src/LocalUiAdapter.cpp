#ifdef ESPSOMFY_LOCAL_UI
#include "LocalUiAdapter.h"
#include "LocalUiAuthorizationAdapter.h"
#include "LocalStopLease.h"
#include "Somfy.h"
#include "ShadeRequests.h"
#include <Preferences.h>
#include <math.h>
#include <string.h>
extern SomfyShadeController somfy;
extern ShadeRequests shadeRequests;
namespace LocalUi {
namespace {
StopLease stopLease;
uint8_t controlFocus=255, leaseTarget=255;
uint32_t stopMasks[3] = {};
uint64_t mix(uint64_t hash, uint32_t value) { return (hash ^ value) * UINT64_C(1099511628211); }
uint64_t textHash(uint64_t hash, const char *value) {
  for(size_t i=0;i<21 && value[i];i++) hash=mix(hash,uint8_t(value[i]));
  return hash;
}
uint64_t radioConfiguration() {
  const auto &radio=somfy.transceiver.config;
  uint64_t hash=UINT64_C(14695981039346656037);
  const uint32_t values[]={radio.enabled, radio.type, uint32_t(radio.proto), radio.SCKPin,
    radio.TXPin, radio.RXPin, radio.MOSIPin, radio.MISOPin, radio.CSNPin,
    uint32_t(int32_t(radio.txPower)), radio.noiseDetection};
  for(auto value:values) hash=mix(hash,value);
  const float floats[]={radio.frequency,radio.deviation,radio.rxBandwidth};
  for(float value:floats) {
    uint32_t bits; memcpy(&bits,&value,sizeof(bits)); hash=mix(hash,bits);
  }
  return hash;
}
uint64_t shadeConfiguration(uint8_t id) {
  auto *shade = somfy.getShadeById(id);
  if(!shade) return 0;
  uint64_t hash=UINT64_C(14695981039346656037);
  const uint32_t values[] = {uint32_t(shade->proto), uint32_t(shade->shadeType), uint32_t(shade->tiltType),
    shade->paired, shade->bitLength, shade->repeats, shade->flipCommands, shade->flipPosition,
    shade->upTime, shade->downTime, shade->tiltTime, shade->stepSize, shade->simMy, shade->sunSensor,
    shade->light, shade->gpioUp, shade->gpioDown, shade->gpioMy, shade->gpioFlags, shade->roomId};
  for(auto value:values) hash=mix(hash,value);
  for(auto &remote:shade->linkedRemotes) hash=mix(hash,remote.getRemoteAddress());
  return textHash(hash,shade->name);
}
void authorizeModel(Model &m) {
  const auto security=pollAuthorization();
  m.authMode=security.mode; m.authorized=security.allowed;
  m.authGeneration=security.generation; m.authConfigurationGeneration=security.configurationGeneration;
  m.retryAfterMs=security.retryAfterMs;
  // Leaving controls for an ordinary menu does not select a different RF target.
  if(security.allowed && controlFocus<3) leaseTarget=controlFocus;
  else if(controlFocus<3 && controlFocus!=leaseTarget) leaseTarget=255;
  for(uint8_t selected=0;selected<3;selected++) {
    StopContext context;
    uint32_t moving=0;
    const auto &target=m.targets[selected];
    if(target.valid) {
      context.id=selected==2?1:selected+1; context.group=selected==2;
      context.identity=target.identity; context.securityGeneration=security.configurationGeneration;
      context.members=selected==2?2:1;
      context.configuration=radioConfiguration();
      for(uint8_t member=0;member<context.members;member++) {
        const uint8_t index=selected==2?member:selected;
        context.memberIds[member]=index+1;
        context.memberIdentities[member]=m.targets[index].identity;
        const uint64_t configuration=shadeConfiguration(index+1);
        context.configuration=mix(context.configuration,uint32_t(configuration));
        context.configuration=mix(context.configuration,uint32_t(configuration>>32));
        if(m.targets[index].moving) moving|=uint32_t(1)<<member;
      }
      if(selected==2) {
        auto *group=somfy.getGroupById(1);
        context.configuration=mix(context.configuration,uint32_t(group->proto));
        context.configuration=mix(context.configuration,group->bitLength);
        context.configuration=mix(context.configuration,group->repeats);
        context.configuration=mix(context.configuration,group->flipCommands);
        context.configuration=textHash(context.configuration,group->name);
      }
    }
    m.targets[selected].configuration=context.configuration;
    if(selected==leaseTarget) {
      stopLease.observe(context,moving,security.allowed,security.stopGrace);
      stopMasks[selected]=stopLease.allowed(context);
    } else stopMasks[selected]=0;
    m.targets[selected].stopLease=stopMasks[selected]!=0;
  }
}
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
  // Device-category behavior belongs to the shared command dispatcher.
  t.valid = shade->paired && shade->proto == radio_proto::RTS && t.identity != 0;
  t.moving = !shade->isIdle();
  const float pos = shade->state().currentPos;
  if(isfinite(pos) && pos >= 0 && pos <= 100)
    t.open[0] = 100 - shade->transformPosition(pos);
}
} // namespace
void focusAuthorizationTarget(uint8_t selected) { controlFocus=selected<3?selected:255; }
DisplayPreferences loadPreferences() {
  DisplayPreferences out;
  Preferences prefs;
  if(!prefs.begin("LCDUI", true))
    return out;
  out.brightness = prefs.getUChar("brightness", 32);
  out.dimSeconds = prefs.getUShort("dimSeconds", 10);
  out.offSeconds = prefs.getUShort("offSeconds", 20);
  prefs.end();
  return out;
}
bool savePreferences(const DisplayPreferences &value) {
  Preferences prefs;
  if(!prefs.begin("LCDUI"))
    return false;
  const bool ok = prefs.putUChar("brightness", value.brightness) == 1 &&
                  prefs.putUShort("dimSeconds", value.dimSeconds) == 2 &&
                  prefs.putUShort("offSeconds", value.offSeconds) == 2;
  prefs.end();
  return ok;
}
Model snapshotModel(bool running) {
  Model m;
  if(!running) {
    m.authorized=false;
    m.authMode=AuthMode::unavailable;
    return m;
  }
#ifdef ESPSOMFY_LOCAL_COMMANDS
  m.controlsEnabled = true;
#endif
  m.ready = somfy.transceiver.available() && !somfy.transceiver.txSuspended();
  // Existing model IDs, not RF identities or names; model renames are reflected.
  readShade(1, m.targets[0]);
  readShade(2, m.targets[1]);
  auto *group = somfy.getGroupById(1);
  if(!group) { authorizeModel(m); return m; }
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
  authorizeModel(m);
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
  if(!current.ready || !target.valid || target.identity != cmd.identity || target.configuration != cmd.configuration ||
     target.members[0] != cmd.members[0] || target.members[1] != cmd.members[1])
    return false;
  const bool session=current.authorized && current.authGeneration==cmd.authGeneration;
  if(!session && !(cmd.action==Action::stop && target.stopLease)) return false;
  const uint32_t leased=stopMasks[cmd.selected];
  shade_request_t request;
  if(cmd.action == Action::stop) {
    // A moving pair can stop on its shared identity in one transmission,
    // just like its Up/Down press. Never send a shared My to an idle member.
    if(cmd.selected == 2 && current.targets[0].moving && current.targets[1].moving &&
       (session || leased==3)) {
      request.type = shade_request_type_t::group_command;
      request.id = 1;
      request.command = somfy_commands::My;
      return shadeRequests.run(request) == shade_request_result_t::done;
    }
    // Mixed groups and individual selections stop only the moving members,
    // rechecked immediately on the model's owning loop.
    bool sent = false;
    for(uint8_t i = 0; i < 2; i++) {
      if(cmd.selected != 2 && cmd.selected != i)
        continue;
      auto *shade = somfy.getShadeById(i + 1);
      const uint8_t bit=cmd.selected==2?i:0;
      if(!shade || shade->isIdle() || (!session && !(leased & (uint32_t(1)<<bit))))
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
