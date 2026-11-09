#include "CounterCommandGuard.h"
#include "Somfy.h"
#include "BitLength.h"
extern SomfyShadeController somfy;
namespace {
bool rf(radio_proto proto){return proto==radio_proto::RTS || proto==radio_proto::RTW || proto==radio_proto::RTV;}
uint8_t bits(SomfyRemote &remote){return remote.bitLength ? remote.bitLength : somfy.transceiver.config.type;}
bool ordinaryStop(SomfyShade &shade) {
  return uint8_t(shade.shadeType)<=uint8_t(shade_types::rgate1) &&
    uint8_t(shade.tiltType)<=uint8_t(tilt_types::euromode) &&
    !isToggleShade(shade.shadeType) && shade.shadeType!=shade_types::drycontact && shade.shadeType!=shade_types::drycontact2;
}
bool safeGroupStop(SomfyGroup &group) {
  if(!rf(group.proto) || !supportedBitLength(bits(group)))return false;
  bool present=false;
  for(auto id:group.linkedShades)if(id && id!=255) {
    auto *shade=somfy.getShadeById(id);
    if(!shade || !rf(shade->proto) || shade->proto!=group.proto || bits(*shade)!=bits(group) || !ordinaryStop(*shade))return false;
    if(!shade->state().direction && !shade->state().tiltDirection)return false;
    present=true;
  }
  return present;
}
}
bool counterCommandAllowed(uint32_t address,somfy_commands command) {
  const auto blocked=somfy.transceiver.blockedCounterAddress();
  if(!blocked)return true; // Normal algorithms/semantics remain unchanged.
  if(address==blocked)return false;
  SomfyGroup *affected=nullptr;
  unsigned matchingGroups=0;
  for(auto &group:somfy.groups)if(group.getGroupId()!=255 && group.getRemoteAddress()==address) {
    ++matchingGroups;
    for(auto id:group.linkedShades)if(id && id!=255) {
      auto *shade=somfy.getShadeById(id);
      if(shade && shade->getRemoteAddress()==blocked)affected=&group;
    }
  }
  if(!affected)return true;
  // A new group start could leave a quarantined primary without its ordinary
  // STOP. Only an independent, complete, all-moving ordinary RF My is allowed.
  if(command!=somfy_commands::My || matchingGroups!=1 || !safeGroupStop(*affected))return false;
  // Same-address primary/linked aliases receive My outside the configured
  // group membership too. An idle alias could start its favorite while all
  // configured members are moving. Refuse this ambiguous legacy context.
  for(auto &shade:somfy.shades)if(shade.getShadeId()!=255) {
    if(shade.getRemoteAddress()==address)return false;
    for(auto &remote:shade.linkedRemotes)if(remote.getRemoteAddress()==address)return false;
  }
  return true;
}
