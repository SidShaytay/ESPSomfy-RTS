#include "ExplicitCounter.h"
#include "Somfy.h"
#include "GitUpdate.h"
#include "SafeRestart.h"
#include "Utils.h"
#include "LoopQueue.h"
#include "GroupReadiness.h"
extern SomfyShadeController somfy;
extern LoopQueue loopQueue;
namespace {
static_assert(SOMFY_MAX_SHADES<=32 && SOMFY_MAX_GROUPS<=16,"Keep affected-target masks bounded");
struct Owner {
  GitUpdateOwnerToken token;
  Owner(){gitUpdate.acquireExternal(GitUpdateExternalOwner::counter,token);}
  ~Owner(){if(token.generation)gitUpdate.releaseExternal(token);}
  explicit operator bool() const{return token.generation!=0;}
};
CounterDomainState admission(CounterEditAdmission value) {
  return value==CounterEditAdmission::blocked ? CounterDomainState::quarantined :
         value==CounterEditAdmission::invalid ? CounterDomainState::invalid : CounterDomainState::busy;
}
bool matches(SomfyShade &shade,uint32_t address) {
  if(shade.getRemoteAddress()==address)return true;
  for(auto &remote:shade.linkedRemotes)if(remote.getRemoteAddress()==address)return true;
  return false;
}
struct Affected {
  uint32_t shades=0;
  uint16_t groups=0;
  bool idle=true;
};
Affected affected(uint32_t address,SomfyShade *newLink=nullptr) {
  Affected out;
  for(unsigned i=0;i<SOMFY_MAX_SHADES;++i) {
    auto &shade=somfy.shades[i];
    if(shade.getShadeId()!=255 && (&shade==newLink || matches(shade,address)))out.shades|=uint32_t(1)<<i;
  }
  // A group identity can be an alias too: include every motor it addresses.
  for(unsigned i=0;i<SOMFY_MAX_GROUPS;++i) {
    auto &group=somfy.groups[i];
    if(group.getGroupId()==255 || group.getRemoteAddress()!=address)continue;
    out.groups|=uint16_t(1)<<i;
    const auto readiness=readGroupReadiness(somfy,group);
    // Explicit correction also refuses missing members, even absent motion
    // evidence. Cached direction alone never overrides fully tracked idle.
    if(readiness.activeMember || readiness.missingMembers || readiness.unknownMovement())out.idle=false;
    for(auto id:group.linkedShades)if(id && id!=255) {
      for(unsigned j=0;j<SOMFY_MAX_SHADES;++j)if(somfy.shades[j].getShadeId()!=255 && somfy.shades[j].getShadeId()==id) {
        out.shades|=uint32_t(1)<<j;break;
      }
    }
  }
  for(unsigned i=0;i<SOMFY_MAX_SHADES;++i)if(out.shades&(uint32_t(1)<<i)) {
    auto &shade=somfy.shades[i];
    if(!shade.idleForAdministration())out.idle=false;
    // Queued group work may address an affected motor under another identity.
    for(unsigned g=0;g<SOMFY_MAX_GROUPS;++g)if(somfy.groups[g].getGroupId()!=255)
      for(auto id:somfy.groups[g].linkedShades)if(id && id==shade.getShadeId())out.groups|=uint16_t(1)<<g;
  }
  return out;
}
uint16_t aliasHighwater(uint32_t address) {
  uint16_t high=0;
  auto include=[&](SomfyRemote &remote) {
    if(remote.getRemoteAddress()==address && remote.lastRollingCode>high)high=remote.lastRollingCode;
  };
  for(auto &shade:somfy.shades)if(shade.getShadeId()!=255) {
    include(shade);for(auto &remote:shade.linkedRemotes)include(remote);
  }
  for(auto &group:somfy.groups)if(group.getGroupId()!=255)include(group);
  return high;
}
void adoptAll(uint32_t address,uint16_t code) {
  for(auto &shade:somfy.shades)if(shade.getShadeId()!=255) {
    bool changed=false;
    if(shade.getRemoteAddress()==address){shade.adoptExplicitCounter(code);changed=true;}
    for(auto &remote:shade.linkedRemotes)if(remote.getRemoteAddress()==address){remote.adoptExplicitCounter(code);changed=true;}
    if(changed) {
      // A shade also remembers frames received through a linked identity.
      // Its own counter may be unrelated, but that old repeat is still stale.
      shade.lastFrame.valid=false;shade.lastFrame.await=0;shade.lastFrame.processed=true;
      shade.configChanged();
    }
  }
  for(auto &group:somfy.groups)if(group.getGroupId()!=255 && group.getRemoteAddress()==address) {
    group.adoptExplicitCounter(code);group.configChanged();
  }
  ++somfy.configurationRevision;
}
bool allShadesIdleForCounter() {
  for(auto &shade:somfy.shades)
    if(shade.getShadeId()!=255 && !shade.idleForAdministration())return false;
  return true;
}
CounterDomainResult correct(SomfyShade &shade,uint32_t address,uint16_t code,SomfyLinkedRemote *slot) {
  if(!address || address>0xffffff)return {};
  // A correction changes model history; preserve already accepted intents
  // rather than making an unrelated queued STOP stale behind this operation.
  if(loopQueue.pending() || !allShadesIdleForCounter())return {CounterDomainState::busy};
  Owner owner;if(!owner)return {CounterDomainState::busy};
  const auto context=affected(address,slot ? &shade : nullptr);
  if(loopQueue.pending() || !allShadesIdleForCounter())return {CounterDomainState::busy};
  const auto admitted=somfy.transceiver.reserveCounterEdit(address,context.idle,context.groups,restartPending());
  if(admitted!=CounterEditAdmission::admitted)return {admission(admitted)};
  const auto persisted=slot ? persistObservedCounter(address,code,aliasHighwater(address)) : persistExplicitCounter(address,code);
  if(!persisted.saved()) {
    somfy.transceiver.finishCounterEdit(persisted,{});
    return {persisted.state==CounterEditState::stale ? CounterDomainState::stale : persisted.state==CounterEditState::uncertain ? CounterDomainState::nvsUncertain : CounterDomainState::nvsFailed};
  }
  if(slot)slot->setRemoteAddress(address);
  adoptAll(address,code);
  const auto file=somfy.commit();
  somfy.transceiver.finishCounterEdit(persisted,file);
  return {file.saved() ? CounterDomainState::saved : file.state==ModelCommitState::uncertain ? CounterDomainState::modelUncertain : CounterDomainState::modelFailed,true,file};
}
}
void SomfyRemote::adoptExplicitCounter(uint16_t code) {
  this->p_lastRollingCode(code);this->m_rollingCodeSynced=true;
  this->lastFrame.valid=false;this->lastFrame.await=0;this->lastFrame.processed=true;
}
CounterDomainResult correctExplicitCounter(uint8_t shadeId,uint16_t code) {
  if(!shadeId || shadeId>SOMFY_MAX_SHADES)return {CounterDomainState::missing};
  auto *shade=somfy.getShadeById(shadeId);
  return shade ? correct(*shade,shade->getRemoteAddress(),code,nullptr) : CounterDomainResult{CounterDomainState::missing};
}
CounterDomainResult linkExplicitRemote(uint8_t shadeId,uint32_t address,uint16_t code) {
  if(!shadeId || shadeId>SOMFY_MAX_SHADES)return {CounterDomainState::missing};
  auto *shade=somfy.getShadeById(shadeId);if(!shade)return {CounterDomainState::missing};
  if(!address || address>0xffffff)return {};
  SomfyLinkedRemote *empty=nullptr;
  for(auto &remote:shade->linkedRemotes) {
    if(remote.getRemoteAddress()==address)return correct(*shade,address,code,&remote);
    if(!empty && !remote.getRemoteAddress())empty=&remote;
  }
  return empty ? correct(*shade,address,code,empty) : CounterDomainResult{CounterDomainState::full};
}
CounterDomainResult recoverExplicitCounterFile() {
  Owner owner;if(!owner)return {CounterDomainState::busy};
  const auto admitted=somfy.transceiver.reserveCounterFileRecovery(restartPending());
  if(admitted!=CounterEditAdmission::admitted)return {admission(admitted)};
  const auto file=somfy.commit();somfy.transceiver.finishCounterFileRecovery(file);
  return {file.saved() ? CounterDomainState::saved : file.state==ModelCommitState::uncertain ? CounterDomainState::modelUncertain : CounterDomainState::modelFailed,false,file};
}
