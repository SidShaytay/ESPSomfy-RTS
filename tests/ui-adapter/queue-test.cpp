#include "Model.h"
#include "LocalUiAdapter.h"
#include "LocalControlQueue.h"
#include "LocalUiAuthorizationAdapter.h"
#include <assert.h>
#include <vector>
#include <stdio.h>
SomfyShadeController somfy;
uint64_t authTestNow=1000;
static LocalUi::Authorization authorization;
static bool securityOverride=false;
static LocalUi::SecuritySnapshot suppliedSecurity;
LocalUi::SecuritySnapshot LocalUi::pollAuthorization() {
  if(securityOverride)return suppliedSecurity;
  const bool allowed=authorization.allowed(authTestNow);
  return {authorization.configuredMode(),allowed,authorization.stopGraceAllowed(),authorization.generation(),authorization.configurationGeneration(),authorization.retryAfter(authTestNow),authorization.expiredSessionGeneration()};
}
void LocalUi::lockAuthorization(){authorization.lock();}
static std::vector<shade_request_t> sent;
static uint64_t advanceOnSend=0;
static unsigned refuseAt=0;
static void (*duringSend)()=nullptr;
struct RecordingRequests {
  shade_request_result_t run(const shade_request_t &request) {
    sent.push_back(request);authTestNow+=advanceOnSend;
    if(duringSend) {auto callback=duringSend;duringSend=nullptr;callback();}
    return refuseAt && sent.size()==refuseAt?shade_request_result_t::radio_refused:shade_request_result_t::done;
  }
} shadeRequests;
#include "LoopHarness.h"
#define ShadeRequests RecordingRequests
#include "../../src/LocalUiAdapter.cpp"
#undef ShadeRequests
using namespace LocalUi;
static Model open(View &v,EntityRef ref) {
  v.entity=ref;v.openedToken=0;v.screen=Screen::control;
  auto model=snapshotModel(true,v);v.openedToken=model.target.token;
  v.authorizedSelection=model.authorized;v.selectionAuthGeneration=model.authGeneration;
  markControlPresented(ref,v.openedToken,v.authorizedSelection,model.authGeneration);
  return snapshotModel(true,v);
}
static Command command(const View &v,Action action) {
  const auto auth=pollAuthorization();return {v.entity,v.openedToken,action,-1,0,auth.allowed?auth.generation:auth.expiredSessionGeneration};
}
static DispatchResult result() {DispatchResult r;assert(takeLocalControlResult(r));return r;}
static View reset() {
  securityOverride=false;advanceOnSend=0;refuseAt=0;modelEditHook=nullptr;duringSend=nullptr;
  setLocalControlsFrozen(true);somfy.transceiver.room=somfy.transceiver.ready=true;somfy.transceiver.suspended=false;pendingRestart=false;
  loopQueue.drain();DispatchResult ignored;while(takeLocalControlResult(ignored)) {}
  setLocalControlsFrozen(false);authTestNow+=1000000;authorization.configure(AuthMode::none,"","");
  sent.clear();loopReads=0;++somfy.configurationRevision;
  for(auto &shade:somfy.shades)shade=SomfyShade{};
  for(auto &group:somfy.groups)group=SomfyGroup{};
  auto &one=somfy.shades[13];one.id=7;one.address=700;one.paired=true;one.shadeType=shade_types::shutter;one.bitLength=56;one.idle=false;
  auto &two=somfy.shades[31];two.id=22;two.address=2200;two.paired=true;two.shadeType=shade_types::shutter;two.bitLength=56;two.idle=false;
  View view;open(view,{EntityKind::device,7});return view;
}
static void pinSession(View &v) {
  authorization.configure(AuthMode::pin,"","1234");Credentials input;strcpy(input.secret,"1234");
  assert(authorization.unlock(input,authTestNow)==AuthResult::unlocked);open(v,v.entity);
}
static void mixedGroup(View &v) {
  auto &third=somfy.shades[0];third.id=31;third.address=3100;third.paired=true;third.shadeType=shade_types::shutter;third.bitLength=56;third.idle=true;
  auto &group=somfy.groups[9];group.id=11;group.bitLength=56;group.linkedShades[0]=31;group.linkedShades[1]=22;group.linkedShades[2]=7;
  ++somfy.configurationRevision;open(v,{EntityKind::group,11});
}
static void deferred_fifo_backpressure_and_cancel() {
  auto view=reset();const auto up=command(view,Action::up);
  assert(dispatch(up)==DispatchResult::queued && sent.empty() && localControlPending());
  assert(dispatch(command(view,Action::down))==DispatchResult::busy);
  loopQueue.drain();assert(sent.size()==1 && sent.back().id==7 && result()==DispatchResult::done && !localControlPending());
  view=reset();assert(dispatch(command(view,Action::up))==DispatchResult::queued);
  const auto duplicate=lastLocalToken;duringSend=[] {runLocalControl(lastLocalToken);};loopQueue.drain();
  assert(sent.size()==1 && !localControlTokenActive(duplicate) && result()==DispatchResult::done); // callback cannot reenter the same in-flight token
  view=reset();loop_item_t other;other.request.id=22;other.request.command=somfy_commands::Down;
  assert(loopQueue.submit(other));assert(dispatch(command(view,Action::up))==DispatchResult::queued);
  other.request.id=31;assert(loopQueue.submit(other));loopQueue.drain();
  assert(sent.size()==3 && sent[0].id==22 && sent[1].id==7 && sent[2].id==31);assert(result()==DispatchResult::done);
  view=reset();other.type=loop_item_type_t::proto_read;
  for(unsigned i=0;i<LOOP_QUEUE_SIZE;i++)assert(loopQueue.submit(other));
  assert(dispatch(command(view,Action::up))==DispatchResult::busy && !localControlPending() && sent.empty());loopQueue.drain();assert(loopReads==32);
  view=reset();somfy.transceiver.room=false;
  assert(dispatch(command(view,Action::up))==DispatchResult::queued);assert(loopQueue.submit(other));
  assert(cancelUnsubmittedLocalControl() && !localControlPending());loopQueue.drain();
  assert(sent.empty() && loopReads==1 && result()==DispatchResult::cancelled); // cancelled RF marker never holds FIFO
}
static void age_clock_context_and_freeze_guards() {
  for(unsigned age:{4999u,5000u}) {
    auto view=reset();assert(dispatch(command(view,Action::up))==DispatchResult::queued);authTestNow+=age;loopQueue.drain();
    assert(result()==(age==4999?DispatchResult::done:DispatchResult::expired));assert(sent.size()==(age==4999?1:0));
    loopQueue.drain();assert(sent.size()==(age==4999?1:0)); // never retries on its own
  }
  auto view=reset();somfy.transceiver.room=false;assert(dispatch(command(view,Action::up))==DispatchResult::queued);
  authTestNow+=100;loopQueue.drain();authTestNow-=50;loopQueue.drain();assert(sent.empty() && result()==DispatchResult::expired);
  for(unsigned changed=0;changed<4;changed++) {
    view=reset();assert(dispatch(command(view,Action::up))==DispatchResult::queued);
    if(changed==0)++somfy.shades[13].configurationRevision;
    if(changed==1)++somfy.restoreRevision;
    if(changed==2)++somfy.transceiver.configurationRevision;
    if(changed==3)pendingRestart=true;
    loopQueue.drain();assert(sent.empty());assert(result()==(changed==3?DispatchResult::refused:DispatchResult::stale));
  }
  view=reset();somfy.transceiver.room=false;loop_item_t edit;edit.type=loop_item_type_t::model_edit;
  modelEditHook=[] {++somfy.shades[13].configurationRevision;};assert(loopQueue.submit(edit));
  assert(dispatch(command(view,Action::up))==DispatchResult::queued);
  loop_item_t read;read.type=loop_item_type_t::proto_read;assert(loopQueue.submit(read));loopQueue.drain();
  assert(!localControlPending() && sent.empty() && loopReads==1 && result()==DispatchResult::stale); // edit earlier in SAME drain, no stale head hold
  view=reset();const auto old=command(view,Action::up);assert(dispatch(old)==DispatchResult::queued);
  setLocalControlsFrozen(true);assert(!localControlPending() && result()==DispatchResult::cancelled);setLocalControlsFrozen(false);
  loopQueue.drain();assert(sent.empty());assert(dispatch(old)==DispatchResult::denied); // None also revokes generation
  view=reset();somfy.shades[13].proto=radio_proto::GP_Remote;++somfy.configurationRevision;open(view,view.entity);
  somfy.transceiver.room=false;assert(dispatch(command(view,Action::up))==DispatchResult::queued);loopQueue.drain();
  assert(sent.size()==1 && result()==DispatchResult::done); // GPIO marker never waits for RF room
}
static void invalid_or_active_replacement_sources_cannot_authorize_old_intents() {
  auto view=reset();pinSession(view);assert(dispatch(command(view,Action::stop))==DispatchResult::queued);
  suppliedSecurity=pollAuthorization();suppliedSecurity.expiredSessionGeneration=suppliedSecurity.generation;
  suppliedSecurity.generation+=2;suppliedSecurity.allowed=true;suppliedSecurity.stopGrace=true;securityOverride=true;
  loopQueue.drain();assert(sent.empty() && result()==DispatchResult::denied); // active replacement never borrows old grace
  view=reset();suppliedSecurity=pollAuthorization();suppliedSecurity.generation=0;securityOverride=true;
  auto missing=command(view,Action::up);missing.authGeneration=0;
  assert(dispatch(missing)==DispatchResult::denied && !localControlPending() && sent.empty());
}
static void stop_supersedes_only_same_authorized_context_at_fifo_tail() {
  auto view=reset();somfy.transceiver.room=false;assert(dispatch(command(view,Action::up))==DispatchResult::queued);
  loop_item_t other;other.request.id=22;other.request.command=somfy_commands::Down;assert(loopQueue.submit(other));
  assert(dispatch(command(view,Action::stop))==DispatchResult::queued && sent.empty());
  assert(dispatch(command(view,Action::stop))==DispatchResult::busy);somfy.transceiver.room=true;loopQueue.drain();
  assert(sent.size()==2 && sent[0].id==22 && sent[1].id==7 && sent[1].command==somfy_commands::My);assert(result()==DispatchResult::done);
  view=reset();somfy.shades[13].idle=true;open(view,view.entity);assert(dispatch(command(view,Action::up))==DispatchResult::queued);
  assert(dispatch(command(view,Action::stop))==DispatchResult::cancelled && !localControlPending());loopQueue.drain();assert(sent.empty());
  assert(dispatch(command(view,Action::stop))==DispatchResult::idle && sent.empty());
  view=reset();somfy.transceiver.room=false;assert(dispatch(command(view,Action::up))==DispatchResult::queued);
  other.type=loop_item_type_t::proto_read;for(unsigned i=1;i<LOOP_QUEUE_SIZE;i++)assert(loopQueue.submit(other));
  assert(dispatch(command(view,Action::stop))==DispatchResult::cancelledBusy && !localControlPending());
  loopQueue.drain();assert(sent.empty() && loopReads==31); // old UP not restored after tail failure
  view=reset();assert(dispatch(command(view,Action::up))==DispatchResult::queued);
  auto bad=command(view,Action::stop);bad.authGeneration=0;assert(dispatch(bad)==DispatchResult::denied && localControlPending());
  bad=command(view,Action::stop);bad.token=0;assert(dispatch(bad)==DispatchResult::stale && localControlPending());
  open(view,{EntityKind::device,22});assert(dispatch(command(view,Action::stop))==DispatchResult::busy && localControlPending());
  assert(cancelUnsubmittedLocalControl());loopQueue.drain();assert(sent.empty());
}
static void pending_stop_observes_idle_behind_a_held_head_and_never_gains_motion() {
  auto view=reset();mixedGroup(view);somfy.transceiver.room=false;
  loop_item_t head;head.request.id=31;head.request.command=somfy_commands::Up;assert(loopQueue.submit(head));
  assert(dispatch(command(view,Action::stop))==DispatchResult::queued);
  somfy.shades[13].idle=true;loopQueue.drain(); // head held, no UI snapshot, local still observes idle
  somfy.shades[13].idle=false;somfy.transceiver.room=true;loopQueue.drain();
  assert(sent.size()==2 && sent[0].id==31 && sent[1].id==22 && sent[1].command==somfy_commands::My);assert(result()==DispatchResult::done);
  view=reset();mixedGroup(view);assert(dispatch(command(view,Action::stop))==DispatchResult::queued);
  somfy.shades[13].idle=somfy.shades[31].idle=true;refreshLocalControl();
  somfy.shades[13].idle=somfy.shades[31].idle=false;loopQueue.drain();assert(sent.empty() && result()==DispatchResult::idle);
}
static void queue_idle_observations_revoke_only_the_matching_global_lease() {
  auto view=reset();mixedGroup(view);pinSession(view);authTestNow+=120000;snapshotModel(true,view);
  somfy.transceiver.room=false;loop_item_t head;head.request.id=31;head.request.command=somfy_commands::Up;assert(loopQueue.submit(head));
  assert(dispatch(command(view,Action::stop))==DispatchResult::queued);
  somfy.shades[13].idle=true;loopQueue.drain();somfy.shades[13].idle=false;somfy.transceiver.room=true;loopQueue.drain();
  assert(result()==DispatchResult::done);sent.clear();snapshotModel(true,view);
  assert(dispatch(command(view,Action::stop))==DispatchResult::queued);loopQueue.drain();
  assert(sent.size()==1 && sent[0].id==22 && result()==DispatchResult::done); // restarted member7 cannot return through GLOBAL grace
  view=reset();pinSession(view);authTestNow+=119999;snapshotModel(true,view);somfy.transceiver.room=false;
  head.request.id=22;assert(loopQueue.submit(head));assert(dispatch(command(view,Action::up))==DispatchResult::queued);
  somfy.shades[13].idle=true;loopQueue.drain(); // ANY matching pending target read must revoke observed idle
  ++authTestNow;somfy.shades[13].idle=false;loopQueue.drain();
  assert(sent.empty() && result()==DispatchResult::denied && !snapshotModel(true,view).target.stopLease);
  view=reset();auto &other=somfy.shades[1];other.id=42;other.address=4200;other.paired=true;other.bitLength=56;other.shadeType=shade_types::shutter;other.idle=false;
  mixedGroup(view);pinSession(view);authTestNow+=119999;snapshotModel(true,view);somfy.transceiver.room=false;assert(dispatch(command(view,Action::stop))==DispatchResult::queued);
  View navigation;open(navigation,{EntityKind::device,42});++authTestNow;snapshotModel(true,navigation);
  somfy.shades[13].idle=somfy.shades[31].idle=true;loopQueue.drain();
  assert(result()==DispatchResult::idle && sent.empty());
  ++authTestNow;somfy.shades[13].idle=false;assert(snapshotModel(true,navigation).target.stopLease); // unrelated focus42 was not revoked
}
static void stop_checks_session_source_age_and_grace_before_every_member() {
  auto view=reset();mixedGroup(view);somfy.shades[31].idle=true;
  assert(dispatch(command(view,Action::stop))==DispatchResult::queued);authTestNow+=4999;advanceOnSend=1;loopQueue.drain();
  assert(sent.size()==1 && result()==DispatchResult::done); // expiry after the LAST required send cannot turn success into partial
  view=reset();mixedGroup(view);assert(dispatch(command(view,Action::stop))==DispatchResult::queued);
  authTestNow+=4999;advanceOnSend=1;loopQueue.drain();assert(sent.size()==1 && result()==DispatchResult::partial);
  view=reset();mixedGroup(view);pinSession(view);authTestNow+=120000+29999;snapshotModel(true,view);
  assert(dispatch(command(view,Action::stop))==DispatchResult::queued);advanceOnSend=1;loopQueue.drain();
  assert(sent.size()==1 && result()==DispatchResult::partial); // exact original 30 s boundary crossed mid-batch
  view=reset();pinSession(view);assert(dispatch(command(view,Action::stop))==DispatchResult::queued);
  suppliedSecurity=pollAuthorization();suppliedSecurity.allowed=false;suppliedSecurity.stopGrace=true;
  suppliedSecurity.expiredSessionGeneration=suppliedSecurity.generation+1;suppliedSecurity.generation+=2;securityOverride=true;
  loopQueue.drain();assert(sent.empty() && result()==DispatchResult::denied); // provider source changes at fixed queue time to isolate binding
}
static void all_32_members_and_partial_refusal_stay_bounded() {
  auto view=reset();for(uint8_t i=0;i<32;i++) {
    auto &s=somfy.shades[i];s.id=i+1;s.address=100+i;s.paired=true;s.proto=radio_proto::RTS;s.bitLength=56;s.shadeType=shade_types::shutter;s.idle=false;
  }
  auto &group=somfy.groups[0];group.id=11;group.bitLength=56;for(uint8_t i=0;i<32;i++)group.linkedShades[i]=32-i;
  ++somfy.configurationRevision;open(view,{EntityKind::group,11});
  assert(dispatch(command(view,Action::stop))==DispatchResult::queued);loopQueue.drain();
  assert(sent.size()==1 && sent[0].type==shade_request_type_t::group_command && result()==DispatchResult::done);
  sent.clear();refuseAt=1;assert(dispatch(command(view,Action::stop))==DispatchResult::queued);loopQueue.drain();
  assert(sent.size()==1 && result()==DispatchResult::refused); // no individual fallback
  sent.clear();refuseAt=16;somfy.shades[31].idle=true;assert(dispatch(command(view,Action::stop))==DispatchResult::queued);loopQueue.drain();
  assert(sent.size()==16 && result()==DispatchResult::partial); // one refused member, no invented retry
}
static void restart_seal_preserves_every_accepted_item_and_refuses_late_submission() {
  reset();
  loop_item_t read;read.type=loop_item_type_t::proto_read;
  assert(loopQueue.submit(read));
  assert(!loopQueue.sealForRestart()); // accepted work cannot be erased
  loopQueue.drain();assert(loopReads==1);
  beforeQueueSend=[] {assert(!loopQueue.sealForRestart());};
  assert(loopQueue.submit(read)); // producer in-flight across seal attempt
  beforeQueueSend=nullptr;
  loopQueue.drain();assert(loopReads==2);
  assert(loopQueue.sealForRestart());
  assert(!loopQueue.submit(read) && !loopQueue.submit(read,true));
  assert(loopQueue.pending()==0 && !loopQueue.sealForRestart());
  loopQueue.reopenAfterRefusedRestart();
  assert(loopQueue.submit(read));loopQueue.drain();assert(loopReads==3);
}
static void accepted_mutation_history_follows_actual_queue_result() {
  reset();auto notes=restartMutationNotes;
  loop_item_t read;read.type=loop_item_type_t::proto_read;
  assert(loopQueue.submit(read));loopQueue.drain();assert(restartMutationNotes==notes);
  loop_item_t device;device.type=loop_item_type_t::proto_device;
  for(auto op:{proto_type_t::getSettings,proto_type_t::reboot}) {
    device.op=uint8_t(op);assert(loopQueue.submit(device));loopQueue.drain();assert(restartMutationNotes==notes);
  }
  for(auto op:{proto_type_t::saveSettings,proto_type_t::scanWifi,proto_type_t::beginFrequencyScan,proto_type_t::endFrequencyScan}) {
    device.op=uint8_t(op);assert(loopQueue.submit(device));loopQueue.drain();assert(restartMutationNotes==++notes);
  }
  loop_item_t write;write.type=loop_item_type_t::model_commit;
  assert(loopQueue.submit(write));loopQueue.drain();assert(restartMutationNotes==++notes);
  for(unsigned n=0;n<LOOP_QUEUE_SIZE;++n)assert(loopQueue.submit(read));
  assert(!loopQueue.submit(write));assert(restartMutationNotes==notes);loopQueue.drain();
  assert(loopQueue.sealForRestart());assert(!loopQueue.submit(write));assert(restartMutationNotes==notes);
  loopQueue.reopenAfterRefusedRestart();
}
struct LearnedIntent final:LearnPending {
  LearnQueueReason validity=LearnQueueReason::ready,last=LearnQueueReason::ready;
  uint64_t token=0;unsigned effects=0,cancellations=0,readsBefore=0;
  LearnQueueReason current(uint64_t)override{return validity;}
  void execute(uint64_t)override{assert(!localLearnTokenActive(token));++effects;readsBefore=loopReads;}
  void cancelled(LearnQueueReason reason)override{assert(!localLearnTokenActive(token));++cancellations;last=reason;}
};
static void learned_intents_share_real_fifo_age_and_exact_cancellation() {
  reset();LearnedIntent source;loop_item_t read;read.type=loop_item_type_t::proto_read;
  assert(loopQueue.submit(read));auto admission=submitLocalLearn(source,authTestNow);source.token=admission.token;
  assert(admission.reason==LearnQueueReason::ready && loopQueue.submit(read));loopQueue.drain();
  assert(source.effects==1 && source.readsBefore==1 && loopReads==2 && sent.empty());runLocalLearn(source.token);assert(source.effects==1);
  for(unsigned age:{4999u,5000u}) {
    reset();LearnedIntent delayed;admission=submitLocalLearn(delayed,authTestNow);delayed.token=admission.token;
    authTestNow+=age;loopQueue.drain();assert(delayed.effects==(age==4999?1u:0u));
    assert(delayed.cancellations==(age==5000?1u:0u));if(age==5000)assert(delayed.last==LearnQueueReason::expired);
  }
  reset();somfy.transceiver.suspended=true;loop_item_t held;held.request.type=shade_request_type_t::unpair;held.request.id=7;
  assert(loopQueue.submit(held));LearnedIntent behind;admission=submitLocalLearn(behind,authTestNow);behind.token=admission.token;
  loopQueue.drain();assert(!behind.effects && localLearnTokenActive(behind.token));authTestNow+=5000;loopQueue.drain();
  assert(!behind.effects && behind.last==LearnQueueReason::expired && loopQueue.pending()==2 && sent.empty());
  somfy.transceiver.suspended=false;loopQueue.drain();assert(loopQueue.pending()==0 && sent.size()==1 && !behind.effects);
  reset();auto*gone=new LearnedIntent;admission=submitLocalLearn(*gone,authTestNow);gone->token=admission.token;
  assert(cancelLocalLearn(admission.token,*gone));delete gone;
  LearnedIntent replacement;auto next=submitLocalLearn(replacement,authTestNow);replacement.token=next.token;assert(next.token>admission.token);
  loopQueue.drain();assert(replacement.effects==1 && !localLearnTokenActive(admission.token) && sent.empty());
  reset();for(unsigned i=0;i<LOOP_QUEUE_SIZE;i++)assert(loopQueue.submit(read));LearnedIntent full;
  admission=submitLocalLearn(full,authTestNow);assert(admission.reason==LearnQueueReason::queueFull && !admission.token);
  loopQueue.drain();assert(!full.effects && !full.cancellations);
  admission=submitLocalLearn(full,authTestNow);full.token=admission.token;full.validity=LearnQueueReason::candidate;loopQueue.drain();
  assert(!full.effects && full.last==LearnQueueReason::candidate && !localLearnTokenActive(full.token));
}
static void favourite_programming_requires_unchanged_observation_at_admission_and_execution() {
  auto view=reset();auto &shade=somfy.shades[13];shade.idle=true;shade.observationRevision=31;somfy.shades[31].idle=true;
  auto model=open(view,view.entity);assert(model.target.favouriteReady && model.target.favouriteRevision==31);
  auto set=command(view,Action::setFavouriteHere);
  assert(dispatch(set)==DispatchResult::stale && sent.empty());
  set.favouriteRevision=31;somfy.shades[31].idle=false;assert(dispatch(set)==DispatchResult::stale);
  somfy.shades[31].idle=true;somfy.transceiver.quiescent=false;assert(dispatch(set)==DispatchResult::stale);
  somfy.transceiver.quiescent=true;assert(dispatch(set)==DispatchResult::queued);
  ++shade.observationRevision;loopQueue.drain();assert(result()==DispatchResult::stale && sent.empty());
  set.favouriteRevision=shade.observationRevision;assert(dispatch(set)==DispatchResult::queued);loopQueue.drain();
  assert(result()==DispatchResult::done && sent.size()==1 && sent[0].type==shade_request_type_t::set_favorite_here);
  sent.clear();auto clear=command(view,Action::clearFavouriteHere);clear.favouriteRevision=shade.observationRevision;
  assert(dispatch(clear)==DispatchResult::stale && sent.empty()); // Not at old favourite.
  shade.value.myPos=shade.value.currentPos;assert(dispatch(clear)==DispatchResult::queued);
  shade.idle=false;loopQueue.drain();assert(result()==DispatchResult::stale && sent.empty());
  shade.idle=true;assert(dispatch(clear)==DispatchResult::queued);loopQueue.drain();
  assert(result()==DispatchResult::done && sent.size()==1 && sent[0].type==shade_request_type_t::clear_favorite_here);
}
int main() {
  static_assert(sizeof(loop_item_t)<=80,"Queue markers must not copy target contexts into 32 slots");
  queueAllocationOK=false;loopQueue.begin();auto noQueue=reset();
  assert(dispatch(command(noQueue,Action::up))==DispatchResult::busy && !localControlPending() && sent.empty());
  queueAllocationOK=true;loopQueue.begin();learned_intents_share_real_fifo_age_and_exact_cancellation();accepted_mutation_history_follows_actual_queue_result();restart_seal_preserves_every_accepted_item_and_refuses_late_submission();deferred_fifo_backpressure_and_cancel();age_clock_context_and_freeze_guards();
  invalid_or_active_replacement_sources_cannot_authorize_old_intents();
  favourite_programming_requires_unchanged_observation_at_admission_and_execution();
  stop_supersedes_only_same_authorized_context_at_fifo_tail();
  pending_stop_observes_idle_behind_a_held_head_and_never_gains_motion();
  queue_idle_observations_revoke_only_the_matching_global_lease();
  stop_checks_session_source_age_and_grace_before_every_member();all_32_members_and_partial_refusal_stay_bounded();
  printf("Host budgets: queue item %zu bytes, pending slot %zu bytes, snapshot %zu bytes.\n",sizeof(loop_item_t),sizeof(PendingControl),sizeof(Model));
  puts("Production LoopQueue/local adapter: FIFO, full queue, cancellation/STOP tail replacement, 5 s age, clock/context/freeze, session/grace, behind-head idle, GPIO and 32-member partial behavior passed.");
}
