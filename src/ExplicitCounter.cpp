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
static_assert(PrimaryCounterReview::aliasSlots==SOMFY_MAX_SHADES*(1+SOMFY_MAX_LINKED_REMOTES)+SOMFY_MAX_GROUPS,
              "Every physical primary/linked/group counter must be represented");
template<class Visit> void visitAliases(uint32_t address,Visit visit) {
  unsigned slot=0;
  for(auto &shade:somfy.shades) {
    const bool configured=shade.getShadeId()!=255;
    visit(slot++,configured && shade.getRemoteAddress()==address,shade.lastRollingCode);
    for(auto &remote:shade.linkedRemotes)
      visit(slot++,configured && remote.getRemoteAddress()==address,remote.lastRollingCode);
  }
  for(auto &group:somfy.groups)
    visit(slot++,group.getGroupId()!=255 && group.getRemoteAddress()==address,group.lastRollingCode);
}
bool reviewedAliasesMatch(SomfyShade &shade,const PrimaryCounterReview &expected) {
  if(!expected.address || shade.getShadeId()!=expected.shadeId || shade.getRemoteAddress()!=expected.address || shade.lastRollingCode!=expected.primary)return false;
  bool same=true;
  visitAliases(expected.address,[&](unsigned slot,bool matches,uint16_t code) {
    if(matches!=expected.includes(slot) || (matches && code!=expected.codes[slot]))same=false;
  });
  return same;
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
CounterDomainResult correct(SomfyShade &shade,uint32_t address,uint16_t code,SomfyLinkedRemote *slot,const PrimaryCounterReview *expected=nullptr,CounterCorrectionWitness *witness=nullptr) {
  if(!somfy.modelLoad.usable())return {CounterDomainState::busy};
  if(!address || address>0xffffff)return {};
  // A correction changes model history; preserve already accepted intents
  // rather than making an unrelated queued STOP stale behind this operation.
  if(loopQueue.pending() || !allShadesIdleForCounter())return {CounterDomainState::busy};
  Owner owner;if(!owner)return {CounterDomainState::busy};
  const auto context=affected(address,slot ? &shade : nullptr);
  if(loopQueue.pending() || !allShadesIdleForCounter())return {CounterDomainState::busy};
  const auto admitted=somfy.transceiver.reserveCounterEdit(address,context.idle,context.groups,restartPending());
  if(admitted!=CounterEditAdmission::admitted)return {admission(admitted)};
  if(expected) {
    StoredCounter stored;
    // The identity is reserved before any NVS read; no writable namespace is
    // opened if the exact review changed or cannot be read. Recheck aliases
    // after the read as well, before the first counter write.
    CounterDomainState refused=CounterDomainState::saved;
    if(!reviewedAliasesMatch(shade,*expected))refused=CounterDomainState::stale;
    else if(!readExplicitCounter(address,stored))refused=CounterDomainState::nvsFailed;
    else if(!stored.same(expected->stored) || !reviewedAliasesMatch(shade,*expected))refused=CounterDomainState::stale;
    if(refused==CounterDomainState::saved && (!witness || !witness->current()))refused=CounterDomainState::stale;
    if(refused!=CounterDomainState::saved) {
      somfy.transceiver.finishCounterEdit({CounterEditState::failed},{});
      return {refused};
    }
  }
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
  if(!somfy.modelLoad.usable())return {CounterDomainState::busy};
  if(!shadeId || shadeId>SOMFY_MAX_SHADES)return {CounterDomainState::missing};
  auto *shade=somfy.getShadeById(shadeId);
  return shade ? correct(*shade,shade->getRemoteAddress(),code,nullptr) : CounterDomainResult{CounterDomainState::missing};
}
CounterDomainState reviewPrimaryCounter(uint8_t shadeId,PrimaryCounterReview &out) {
  out={};
  if(!somfy.modelLoad.usable())return CounterDomainState::busy;
  if(!shadeId || shadeId>SOMFY_MAX_SHADES)return CounterDomainState::missing;
  auto *shade=somfy.getShadeById(shadeId);if(!shade)return CounterDomainState::missing;
  const uint32_t address=shade->getRemoteAddress();
  if(!address || address>0xffffff)return CounterDomainState::invalid;
  if(somfy.transceiver.counterQuarantine()!=CounterQuarantine::none)return CounterDomainState::quarantined;
  Owner owner;if(!owner)return CounterDomainState::busy;
  out.address=address;out.primary=shade->lastRollingCode;out.shadeId=shadeId;
  visitAliases(address,[&](unsigned slot,bool matches,uint16_t code) {
    if(matches) {out.matching[slot/8]|=uint8_t(1u<<(slot%8));out.codes[slot]=code;}
  });
  if(!readExplicitCounter(address,out.stored)) {out={};return CounterDomainState::nvsFailed;}
  if(!reviewedAliasesMatch(*shade,out)) {out={};return CounterDomainState::stale;}
  return CounterDomainState::saved;
}
bool primaryCounterReviewCurrent(uint8_t shadeId,const PrimaryCounterReview &expected) {
  if(!somfy.modelLoad.usable() || !shadeId || shadeId>SOMFY_MAX_SHADES || somfy.transceiver.counterQuarantine()!=CounterQuarantine::none)return false;
  auto *shade=somfy.getShadeById(shadeId);
  return shade && reviewedAliasesMatch(*shade,expected);
}
CounterDomainResult correctReviewedPrimaryCounter(uint8_t shadeId,uint16_t code,const PrimaryCounterReview &expected,CounterCorrectionWitness &witness) {
  if(!somfy.modelLoad.usable())return {CounterDomainState::busy};
  if(!shadeId || shadeId>SOMFY_MAX_SHADES)return {CounterDomainState::missing};
  auto *shade=somfy.getShadeById(shadeId);
  return shade ? correct(*shade,shade->getRemoteAddress(),code,nullptr,&expected,&witness) : CounterDomainResult{CounterDomainState::missing};
}
CounterDomainResult linkExplicitRemote(uint8_t shadeId,uint32_t address,uint16_t code) {
  if(!somfy.modelLoad.usable())return {CounterDomainState::busy};
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
  if(!somfy.modelLoad.usable())return {CounterDomainState::busy};
  Owner owner;if(!owner)return {CounterDomainState::busy};
  const auto admitted=somfy.transceiver.reserveCounterFileRecovery(restartPending());
  if(admitted!=CounterEditAdmission::admitted)return {admission(admitted)};
  const auto file=somfy.commit();somfy.transceiver.finishCounterFileRecovery(file);
  return {file.saved() ? CounterDomainState::saved : file.state==ModelCommitState::uncertain ? CounterDomainState::modelUncertain : CounterDomainState::modelFailed,false,file};
}
