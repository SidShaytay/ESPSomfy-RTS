#include "LocalAdvanced.h"
#include "LocalJsonAllocator.h"
#include "LocalPrimaryCounter.h"
#include "SomfyFrame.h"
#include <new>
#include <stdio.h>
#include <string.h>
namespace LocalUi {
namespace Advanced {
using namespace EntityAdmin;
struct Form::Session final : LearnPending {
  Form &owner;
  struct LearnState {
    LearnCandidates candidates;
    LearnSelection selection;
    explicit LearnState(ReceiveHub &hub):candidates(hub){}
  };
  // Only one interactive flow lives in the wiping slab. Keeping both would
  // crowd out Learn's existing bounded validation pools at execution time.
  union { LearnState learned; PrimaryCounterDraft primary; MembershipDraft membership; };
  bool primaryMode=false, membershipMode=false;
  bool programMode=false;
  RemoteProgram program;
  bool programSent=false;
  uint64_t ticket = 0, queuedAt = 0;
  bool listening = false, fileOnly = false, canRetryFile = false,
       linkApplied = false, fileVerified = false;
  CounterDomainState counterOutcome = CounterDomainState::invalid;
  Session(Form &form, ReceiveHub &hub) : owner(form), learned(hub) {}
  ~Session() override {
    if(membershipMode)membership.~MembershipDraft();else if(primaryMode)primary.~PrimaryCounterDraft();else learned.~LearnState();
  }
  void openPrimary(const PrimaryCounterReview &review) {
    if(!primaryMode){learned.~LearnState();new(&primary)PrimaryCounterDraft();primaryMode=true;}
    primary.open(review);
  }
  LearnQueueReason current(uint64_t now) override {
    return owner.learnCurrent(now);
  }
  bool usesRadio() const override { return programMode; }
  void execute(uint64_t now) override { owner.runLearn(now); }
  void cancelled(LearnQueueReason reason) override {
    owner.learnCancelled(reason);
  }
  enum class Page : uint8_t {
    picker,
    menu,
    list,
    confirm,
    result,
    refused,
    learnStart,
    learnList,
    learnReview,
    learnPending,
    learnFault,
    learnResult,
    primaryCounter,
    membership,
    groupMenu,
    details,
    remoteMenu,remoteRepeat,remoteConfirm,remotePending,remoteResult
  };
  Context context;
  OrderIds ids{}, slots{};
  Authorization auth;
  uint64_t revision = 0, observedAt = 0, activityAt = 0;
  Target target;
  Action action;
  Page page = Page::picker, returnPage = Page::menu;
  char refusal[64]{};
  uint8_t count = 0, cursor = 0, selectedSlot = 0;
  bool paired = false;
};
uint64_t Form::epoch() const { return session ? session->revision : 0; }
void Form::endLearning() {
  if (!session || session->primaryMode || session->membershipMode)
    return;
  if (session->listening || session->learned.selection.session ||
      session->learned.candidates.count())
    privateProjectionDiscarded = true;
  session->learned.candidates.end();
  session->listening = false;
  session->learned.selection = {};
}
void Form::returnFromLearning(){
  endLearning();auto &s=*session;
  s.page=s.target.scope==Scope::repeaters?Session::Page::list:Session::Page::menu;
  s.cursor=s.target.scope==Scope::repeaters?backend.itemCount(s.target):2;
}
void Form::close(Notice n) {
  if (session) {
    if (session->ticket)
      backend.cancelLearn(session->ticket, *session);
    session->ticket = 0;
    endLearning();
    if(session->primaryMode){session->primary.clear();privateProjectionDiscarded=true;}
    if(session->membershipMode){session->membership.clear();privateProjectionDiscarded=true;}
    session->~Session();
    allocator.deallocate(session);
    session = nullptr;
  }
  status = n;
}
void Form::touch(uint64_t now) {
  session->activityAt = now;
  backend.activity();
}
bool Form::guard(uint64_t now) {
  if (!session)
    return false;
  auto &s = *session;
  if (now < s.observedAt || now - s.activityAt >= timeoutMs) {
    close(Notice::expired);
    return false;
  }
  s.observedAt = now;
  if(s.primaryMode)s.primary.tick(now);
  if(s.primaryMode && s.primary.expired()){close(Notice::expired);return false;}
  const auto auth = backend.authorization();
  if (!auth.allowed) {
    close(Notice::locked);
    return false;
  }
  if (auth.generation != s.auth.generation ||
      auth.configurationGeneration != s.auth.configurationGeneration ||
      backend.revision() != s.revision) {
    close(Notice::changed);
    return false;
  }
  if (s.page != Session::Page::picker) {
    Context current;
    if (!backend.capture(s.target, current) || !s.context.same(current)) {
      close(Notice::changed);
      return false;
    }
  }
  if(s.membershipMode && s.membership.hasSelection() && !s.fileOnly){
    const auto current=backend.membershipCurrent(s.target,s.context,s.auth,s.revision,s.membership.selection());
    if(current==Admission::locked){close(Notice::locked);return false;}
    if(current!=Admission::accepted){close(Notice::changed);return false;}
  }
  if(s.primaryMode && s.primary.review().address && !backend.primaryCurrent(s.target,s.primary.review())) {
    close(Notice::changed);return false;
  }
  if (s.listening && s.page != Session::Page::learnPending) {
    const auto check = s.learned.candidates.tick(now);
    if (check != LearnCheck::ready) {
      close(check == LearnCheck::expired || check == LearnCheck::clock
                ? Notice::expired
                : Notice::changed);
      return false;
    }
    if (s.page == Session::Page::learnReview &&
        s.learned.candidates.validate(s.learned.selection, now) != LearnCheck::ready) {
      s.learned.selection = {};
      learnFault("Received frame changed. Choose and review a frame again.",
                 true);
    }
  }
  return true;
}
bool Form::open(Scope scope, uint64_t now) {
  discard();
  const auto auth = backend.authorization();
  const auto revision = backend.revision();
  if (!auth.allowed || !auth.generation || !auth.configurationGeneration) {
    close(Notice::locked);
    return false;
  }
  if (!revision || (scope != Scope::devices && scope != Scope::groups &&
                    scope != Scope::repeaters)) {
    close(Notice::changed);
    return false;
  }
  static_assert(
      sizeof(Session) + JsonAllocator::allocationOverhead <= 2304,
      "Bounded lazy Advanced slab, including wiping allocator header");
  void *memory = allocator.allocate(sizeof(Session));
  if (!memory) {
    close(Notice::noMemory);
    return false;
  }
  session = new (memory) Session(*this, backend.receivedFrames());
  auto &s = *session;
  s.auth = auth;
  s.revision = revision;
  s.target.scope = scope;
  s.observedAt = s.activityAt = now;
  if (scope == Scope::repeaters) {
    s.page = Session::Page::list;
    if (!backend.capture(s.target, s.context)) {
      close(Notice::changed);
      return false;
    }
  } else if (!backend.targets(scope, s.ids, s.slots, s.count) || s.count > 32) {
    close(Notice::changed);
    return false;
  }
  for (uint8_t i = 0; i < s.count; i++) {
    if (!s.ids[i] || s.ids[i] == 255 || s.slots[i] >= 32) {
      close(Notice::changed);
      return false;
    }
    for (uint8_t j = 0; j < i; j++)
      if (s.ids[i] == s.ids[j] || s.slots[i] == s.slots[j]) {
        close(Notice::changed);
        return false;
      }
  }
  return guard(now);
}
bool Form::select() {
  auto &s = *session;
  if (s.cursor >= s.count)
    return false;
  s.target.id = s.ids[s.cursor];
  s.selectedSlot = s.slots[s.cursor];
  if (!backend.capture(s.target, s.context))
    return false;
  if (s.target.scope == Scope::devices) {
    if (!backend.paired(s.target, s.paired))
      return false;
    s.page = Session::Page::menu;
  } else if(s.target.scope==Scope::groups) {
    s.page=Session::Page::groupMenu;
  } else
    s.page = backend.supportsProgramming(s.target)?Session::Page::menu:Session::Page::list;
  s.cursor = 0;
  return true;
}
void Form::turn(int direction, uint64_t now) {
  if (!guard(now) || !direction)
    return;
  touch(now);
  auto &s = *session;
  uint8_t count = 0;
  switch (s.page) {
  case Session::Page::picker:
    count = s.count;
    break;
  case Session::Page::primaryCounter:
    s.primary.turn(direction,now);return;
  case Session::Page::membership:
    s.membership.turn(direction);return;
  case Session::Page::groupMenu:count=3;break;
  case Session::Page::details:count=entityStatusRows;break;
  case Session::Page::menu:
    count = 6;
    break;
  case Session::Page::remoteMenu:
    count=6;
    break;
  case Session::Page::remoteRepeat:
    if(direction>0&&s.program.repeats<127)++s.program.repeats;
    if(direction<0&&s.program.repeats>1)--s.program.repeats;
    return;
  case Session::Page::remoteConfirm:
    count=2;
    break;
  case Session::Page::remotePending:
  case Session::Page::remoteResult:
    count=1;
    break;
  case Session::Page::learnList:
    count = s.learned.candidates.count();
    break;
  case Session::Page::learnPending:
    count = 1;
    break;
  case Session::Page::learnResult:
    count = s.canRetryFile ? 2 : 1;
    break;
  case Session::Page::learnStart:
  case Session::Page::learnReview:
  case Session::Page::learnFault:
  case Session::Page::confirm:
  case Session::Page::result:
  case Session::Page::refused:
    count = 2;
    break;
  case Session::Page::list:
    count = backend.itemCount(s.target)+(s.target.scope==Scope::repeaters?1:0);
    break;
  }
  if (count)
    s.cursor = (s.cursor + (direction > 0 ? 1 : count - 1)) % count;
}
void Form::refused(Admission a, const char *reason) {
  if (a == Admission::noMemory) {
    close(Notice::noMemory);
    return;
  }
  if (a == Admission::locked) {
    close(Notice::locked);
    return;
  }
  if (a == Admission::stale) {
    close(Notice::changed);
    return;
  }
  status = a == Admission::busy ? Notice::busy : Notice::failed;
  if (session->page != Session::Page::result) {
    session->page = Session::Page::refused;
    snprintf(session->refusal, sizeof(session->refusal), "%.63s",
             reason && *reason ? reason
             : a == Admission::busy
                 ? "Busy. Wait for queued or radio work to finish."
                 : "Configuration refused. Review current state.");
  }
  session->cursor = 0;
}
void Form::apply() {
  auto &s = *session;
  finishApply(backend.apply(s.target,s.action,s.context,s.auth,s.revision,allocator));
}
void Form::finishApply(const ApplyResult &result){
  auto &s=*session;
  if (result.admission != Admission::accepted) {
    refused(result.admission, result.reason);
    return;
  }
  if (!result.edit.applied) {
    refused(Admission::invalid);
    return;
  }
  if (result.edit.persistence.saved()) {
    close(Notice::saved);
    return;
  }
  status = result.edit.persistence.state == ModelCommitState::uncertain
               ? Notice::uncertain
               : Notice::appliedNotSaved;
  // The mutation has happened. Erase every executable action and picker ID;
  // only fresh current context/history and a COMMIT-only result remain.
  s.action = {};
  memset(s.ids, 0, sizeof(s.ids));
  memset(s.slots, 0, sizeof(s.slots));
  s.count = 0;
  s.revision = backend.revision();
  if (!backend.capture(s.target, s.context)) {
    close(Notice::changed);
    return;
  }
  s.page = Session::Page::result;
  s.cursor = 1;
}
LearnQueueReason Form::learnCurrent(uint64_t now) {
  if (!session)
    return LearnQueueReason::stale;
  const auto &s = *session;
  if ((s.page == Session::Page::learnPending || s.page==Session::Page::remotePending || (s.primaryMode && s.primary.pending()) || (s.membershipMode && s.membership.pending())) &&
      (now < s.queuedAt || now - s.queuedAt >= LearnQueue::maxAgeMs))
    return LearnQueueReason::expired;
  if (now < s.observedAt || now < s.activityAt ||
      now - s.activityAt >= timeoutMs)
    return LearnQueueReason::expired;
  const auto admission = s.programMode
      ? backend.checkProgrammingContext(s.target,s.context,s.auth,s.revision)
      : backend.checkLearnContext(s.target, s.context, s.auth, s.revision);
  if (admission == Admission::locked)
    return LearnQueueReason::locked;
  if (admission != Admission::accepted)
    return LearnQueueReason::stale;
  if(s.programMode)return LearnQueueReason::ready;
  if (s.fileOnly)
    return LearnQueueReason::ready;
  if(s.membershipMode) {
    const auto a=backend.membershipCurrent(s.target,s.context,s.auth,s.revision,s.membership.selection());
    return a==Admission::accepted?LearnQueueReason::ready:a==Admission::locked?LearnQueueReason::locked:a==Admission::stale?LearnQueueReason::stale:LearnQueueReason::candidate;
  }
  if(s.primaryMode)return backend.primaryCurrent(s.target,s.primary.review()) ? LearnQueueReason::ready : LearnQueueReason::stale;
  const auto check = session->learned.candidates.validate(s.learned.selection, now);
  return check == LearnCheck::ready     ? LearnQueueReason::ready
         : check == LearnCheck::expired ? LearnQueueReason::expired
         : check == LearnCheck::clock   ? LearnQueueReason::clock
                                        : LearnQueueReason::candidate;
}
void Form::learnFault(const char *reason, bool chooseFrame) {
  auto &s = *session;
  s.page = Session::Page::learnFault;
  s.returnPage =
      chooseFrame ? Session::Page::learnList : Session::Page::learnReview;
  s.cursor = 0;
  snprintf(s.refusal, sizeof(s.refusal), "%s", reason);
  status = Notice::none;
}
void Form::learnCancelled(LearnQueueReason reason) {
  if (!session)
    return;
  session->ticket = 0;
  if (reason == LearnQueueReason::locked) {
    close(Notice::locked);
    return;
  }
  if (reason == LearnQueueReason::expired ||
      reason == LearnQueueReason::clock) {
    close(Notice::expired);
    return;
  }
  if (reason == LearnQueueReason::stale) {
    close(Notice::changed);
    return;
  }
  if(session->membershipMode){session->membership.busy();return;}
  if(session->primaryMode){session->primary.busy();return;}
  if(session->programMode){session->programSent=false;session->page=Session::Page::remoteResult;return;}
  if (session->fileOnly) {
    session->page = Session::Page::learnResult;
    session->cursor = 1;
    return;
  }
  session->learned.selection = {};
  learnFault("Received frame changed. Choose and review a frame again.", true);
}
void Form::queueLearn(uint64_t now, bool fileOnly) {
  auto &s = *session;
  s.fileOnly = fileOnly;
  s.queuedAt = now;
  s.page = Session::Page::learnPending;
  s.cursor = 0;
  const auto queued = backend.queueLearn(s, now);
  if (queued.reason == LearnQueueReason::ready) {
    s.ticket = queued.token;
    return;
  }
  if (queued.reason == LearnQueueReason::busy ||
      queued.reason == LearnQueueReason::queueFull ||
      queued.reason == LearnQueueReason::exhausted) {
    if (fileOnly) {
      s.page = Session::Page::learnResult;
      s.cursor = 1;
      return;
    }
    learnFault("Queue unavailable/full. Wait, then review explicitly.");
    return;
  }
  learnCancelled(queued.reason);
}
void Form::runLearn(uint64_t) {
  if (!session)
    return;
  if(session->membershipMode){runMembership();return;}
  if(session->primaryMode){runPrimary();return;}
  auto &s = *session;
  s.ticket = 0;
  if(s.programMode) {
    const auto result=backend.sendProgramming(s.target,s.context,s.auth,s.revision,s.program,s);
    if(result.admission==Admission::locked){close(Notice::locked);return;}
    if(result.admission==Admission::stale){close(Notice::changed);return;}
    s.programSent=result.admission==Admission::accepted&&result.result==shade_request_result_t::done;
    s.page=Session::Page::remoteResult;s.cursor=0;
    return;
  }
  if (s.fileOnly) {
    const auto result =
        backend.retryLearnFile(s.target, s.context, s.auth, s.revision, s);
    if (result.freshness != LearnQueueReason::ready) {
      learnCancelled(result.freshness);
      return;
    }
    if (result.admission == Admission::locked) {
      close(Notice::locked);
      return;
    }
    if (result.admission == Admission::stale) {
      close(Notice::changed);
      return;
    }
    if (result.report.refusal == RestartReason::counterUncertain) {
      s.canRetryFile = false;
      s.counterOutcome = CounterDomainState::quarantined;
      s.page = Session::Page::learnResult;
      s.cursor = 0;
      return;
    }
    if (result.admission != Admission::accepted ||
        result.report.refusal != RestartReason::none) {
      snprintf(s.refusal, sizeof(s.refusal),
               "Busy. Link applied; retry file only, never Link again.");
      s.page = Session::Page::learnResult;
      s.cursor = 1;
      return;
    }
    s.counterOutcome = result.report.file.saved() ? CounterDomainState::saved
                       : result.report.file.state == ModelCommitState::uncertain
                           ? CounterDomainState::modelUncertain
                           : CounterDomainState::modelFailed;
    s.fileVerified = result.report.file.saved();
    s.refusal[0] = 0;
    s.canRetryFile = !result.report.file.saved();
    s.page = Session::Page::learnResult;
    s.cursor = s.canRetryFile ? 1 : 0;
    status = Notice::none;
    return;
  }
  const auto result = backend.linkLearn(s.target, s.context, s.auth, s.revision,
                                        s.learned.selection, s, allocator);
  if (result.freshness != LearnQueueReason::ready) {
    learnCancelled(result.freshness);
    return;
  }
  if (result.admission == Admission::locked) {
    close(Notice::locked);
    return;
  }
  if (result.admission == Admission::stale) {
    close(Notice::changed);
    return;
  }
  if (result.admission == Admission::noMemory) {
    close(Notice::noMemory);
    return;
  }
  if (result.admission != Admission::accepted) {
    learnFault(s.target.scope==Scope::repeaters&&result.edit.desc&&*result.edit.desc?result.edit.desc:
      "Link not applied. Wait, then review explicitly.");
    return;
  }
  if(s.target.scope==Scope::repeaters){
    if(!result.edit.applied){learnFault(result.edit.desc&&*result.edit.desc?result.edit.desc:"Repeater not added. Review current list.");return;}
    endLearning();finishApply({result.edit});return;
  }
  if (!result.edit.hasCounterResult) {
    endLearning();
    s.counterOutcome = CounterDomainState::invalid;
    s.canRetryFile = false;
    s.page = Session::Page::learnResult;
    s.cursor = 0;
    return;
  }
  s.counterOutcome = result.edit.counterState;
  if (result.edit.applied) {
    s.linkApplied = true;
    s.fileVerified = result.edit.persistence.saved();
    s.refusal[0] = 0;
    endLearning();
    s.action = {};
    memset(s.ids, 0, sizeof(s.ids));
    memset(s.slots, 0, sizeof(s.slots));
    s.count = 0;
    s.revision = backend.revision();
    if (!backend.capture(s.target, s.context)) {
      close(Notice::changed);
      return;
    }
    s.canRetryFile = !result.edit.persistence.saved();
    s.page = Session::Page::learnResult;
    s.cursor = s.canRetryFile ? 1 : 0;
    status = Notice::none;
    return;
  }
  if (s.counterOutcome == CounterDomainState::nvsUncertain ||
      s.counterOutcome == CounterDomainState::quarantined) {
    endLearning();
    s.canRetryFile = false;
    s.page = Session::Page::learnResult;
    s.cursor = 0;
    return;
  }
  learnFault(
      s.counterOutcome == CounterDomainState::stale
          ? "Stored/live code newer. Cancel, then Begin observation again."
      : s.counterOutcome == CounterDomainState::nvsFailed
          ? "Counter storage read/open failed. Nothing was linked."
      : s.counterOutcome == CounterDomainState::full
          ? "No linked remote slot. Nothing was linked."
          : "Link not applied. Wait, then review explicitly.");
}
void Form::primaryEffect(PrimaryCounterEffect effect,uint64_t now) {
  if(!session || effect==PrimaryCounterEffect::none)return;
  auto &s=*session;
  if(effect==PrimaryCounterEffect::leave){close(Notice::none);return;}
  if(effect==PrimaryCounterEffect::checkUnchanged ||
     (effect==PrimaryCounterEffect::queueCorrection && s.primary.review().reconciled(s.primary.requested()))) {
    PrimaryCounterReview current;
    const auto read=backend.readPrimary(s.target,s.context,s.auth,s.revision,current);
    if(read.admission==Admission::locked){close(Notice::locked);return;}
    if(read.admission==Admission::stale){close(Notice::changed);return;}
    if(read.admission!=Admission::accepted){s.primary.busy();return;}
    if(read.state==CounterDomainState::saved && current.same(s.primary.review()) && current.reconciled(s.primary.requested())) {
      close(Notice::unchanged);return;
    }
    s.primary.complete({read.state==CounterDomainState::saved?CounterDomainState::stale:read.state});
    privateProjectionDiscarded=true;return;
  }
  s.fileOnly=effect==PrimaryCounterEffect::queueFile;s.queuedAt=now;s.primary.queued();
  const auto queued=backend.queueLearn(s,now);
  if(queued.reason==LearnQueueReason::ready){s.ticket=queued.token;return;}
  if(queued.reason==LearnQueueReason::busy || queued.reason==LearnQueueReason::queueFull || queued.reason==LearnQueueReason::exhausted) {s.primary.busy();return;}
  learnCancelled(queued.reason);
}
void Form::runPrimary() {
  auto &s=*session;s.ticket=0;
  if(s.fileOnly) {
    const auto result=backend.retryLearnFile(s.target,s.context,s.auth,s.revision,s);
    if(result.freshness!=LearnQueueReason::ready){learnCancelled(result.freshness);return;}
    if(result.admission==Admission::locked){close(Notice::locked);return;}
    if(result.admission==Admission::stale){close(Notice::changed);return;}
    if(result.report.refusal==RestartReason::counterUncertain){s.primary.blockedFile();return;}
    if(result.admission!=Admission::accepted || result.report.refusal!=RestartReason::none){s.primary.busy();return;}
    s.primary.completeFile(result.report.file);return;
  }
  const auto result=backend.correctPrimary(s.target,s.context,s.auth,s.revision,s.primary.review(),s.primary.requested(),s);
  if(result.freshness!=LearnQueueReason::ready){learnCancelled(result.freshness);return;}
  if(result.admission==Admission::locked){close(Notice::locked);return;}
  if(result.admission==Admission::stale){close(Notice::changed);return;}
  if(result.admission!=Admission::accepted){s.primary.busy();return;}
  privateProjectionDiscarded=true;
  s.primary.complete(result.counter);
  if(result.counter.applied) {
    s.action={};memset(s.ids,0,sizeof(s.ids));memset(s.slots,0,sizeof(s.slots));s.count=0;
    s.revision=backend.revision();
    if(!backend.capture(s.target,s.context)){close(Notice::changed);return;}
  }
}
bool Form::openMembership(bool adding) {
  auto &s=*session;MembershipRows rows;
  if(!backend.membershipRows(s.target,adding,rows))return false;
  if(!s.membershipMode){endLearning();s.learned.~LearnState();new(&s.membership)MembershipDraft();s.membershipMode=true;}
  privateProjectionDiscarded=true;s.page=Session::Page::membership;s.refusal[0]=0;
  return s.membership.open(adding,rows);
}
void Form::membershipEffect(MembershipEffect effect,uint64_t now) {
  if(!session || effect==MembershipEffect::none)return;
  auto &s=*session;
  if(effect==MembershipEffect::leave){close(Notice::none);return;}
  if(effect==MembershipEffect::readMembers || effect==MembershipEffect::readCandidates){if(!openMembership(effect==MembershipEffect::readCandidates))close(Notice::changed);return;}
  if(effect==MembershipEffect::discardReview){privateProjectionDiscarded=true;return;}
  if(effect==MembershipEffect::select){
    MembershipSelection selected;
    if(!backend.selectMembership(s.target,s.context,s.membership.adding(),s.membership.rows(),s.membership.index(),selected) || !s.membership.select(selected))close(Notice::changed);
    return;
  }
  const bool file=effect==MembershipEffect::queueFile;
  const auto admission=file?backend.checkLearnContext(s.target,s.context,s.auth,s.revision):backend.membershipCurrent(s.target,s.context,s.auth,s.revision,s.membership.selection());
  if(admission==Admission::locked){close(Notice::locked);return;}
  if(admission==Admission::stale){close(Notice::changed);return;}
  if(admission!=Admission::accepted){s.membership.busy();return;}
  s.fileOnly=file;s.queuedAt=now;
  const auto queued=backend.queueLearn(s,now);
  if(queued.reason==LearnQueueReason::ready){s.ticket=queued.token;s.membership.queued();return;}
  learnCancelled(queued.reason);
}
void Form::runMembership() {
  auto &s=*session;s.ticket=0;
  if(s.fileOnly){
    const auto result=backend.retryMembershipFile(s.target,s.context,s.auth,s.revision,s);
    if(result.freshness!=LearnQueueReason::ready){learnCancelled(result.freshness);return;}
    if(result.admission==Admission::locked){close(Notice::locked);return;}
    if(result.admission==Admission::stale){close(Notice::changed);return;}
    if(result.admission!=Admission::accepted || result.report.refusal!=RestartReason::none){s.membership.busy();return;}
    s.membership.fileComplete(result.report.file);return;
  }
  const auto result=backend.applyMembership(s.target,s.context,s.auth,s.revision,s.membership.selection(),s,allocator);
  if(result.freshness!=LearnQueueReason::ready){learnCancelled(result.freshness);return;}
  if(result.result.admission==Admission::locked){close(Notice::locked);return;}
  if(result.result.admission==Admission::stale){close(Notice::changed);return;}
  if(result.result.admission!=Admission::accepted){snprintf(s.refusal,sizeof(s.refusal),"%s",result.result.reason);s.membership.busy();return;}
  const auto completed=s.membership.complete(result.result.edit);
  if(completed==MembershipCompletion::unchanged){close(Notice::unchanged);return;}
  if(completed==MembershipCompletion::applied){privateProjectionDiscarded=true;s.revision=backend.revision();if(!backend.capture(s.target,s.context))close(Notice::changed);}
}
void Form::activate(uint64_t now) {
  if (!guard(now))
    return;
  touch(now);
  auto &s = *session;
  switch (s.page) {
  case Session::Page::picker:
    if (!s.count) {
      close(Notice::none);
      return;
    }
    if (!select())
      close(Notice::changed);
    break;
  case Session::Page::primaryCounter:
    primaryEffect(s.primary.activate(now),now);return;
  case Session::Page::membership:
    membershipEffect(s.membership.activate(),now);return;
  case Session::Page::groupMenu:
    if(!s.cursor){if(!openMembership(false))close(Notice::changed);}
    else if(s.cursor==1){s.page=Session::Page::details;s.cursor=0;}
    else{if(!backend.supportsProgramming(s.target)){status=Notice::invalid;return;}endLearning();s.programMode=true;s.program={};s.page=Session::Page::remoteMenu;s.cursor=0;}return;
  case Session::Page::details:
    if(s.cursor==entityStatusRows-1){s.page=s.target.scope==Scope::groups?Session::Page::groupMenu:Session::Page::menu;s.cursor=s.target.scope==Scope::groups?1:4;}return;
  case Session::Page::menu:
    if(s.cursor==4){endLearning();s.page=Session::Page::details;s.cursor=0;return;}
    if((s.target.scope==Scope::devices&&s.cursor==5) || (s.target.scope==Scope::groups&&s.cursor==2)) {
      if(!backend.supportsProgramming(s.target)){status=Notice::invalid;return;}
      endLearning();s.programMode=true;s.program={};s.page=Session::Page::remoteMenu;s.cursor=0;return;
    }
    if(s.cursor==3) {
      PrimaryCounterReview reviewed;
      const auto result=backend.readPrimary(s.target,s.context,s.auth,s.revision,reviewed);
      if(result.admission==Admission::locked){close(Notice::locked);return;}
      if(result.admission==Admission::stale){close(Notice::changed);return;}
      if(result.admission!=Admission::accepted){status=Notice::busy;return;}
      endLearning();s.openPrimary(reviewed);s.page=Session::Page::primaryCounter;
      if(result.state!=CounterDomainState::saved)s.primary.complete({result.state});
      status=Notice::none;return;
    }
    if (s.cursor == 2) {
      s.page = Session::Page::learnStart;
      s.cursor = 0;
      break;
    }
    if (s.cursor) {
      s.page = Session::Page::list;
      s.cursor = 0;
      break;
    }
    s.action = {};
    s.action.kind = ActionKind::setPaired;
    s.action.paired = !s.paired;
    s.returnPage = Session::Page::menu;
    s.page = Session::Page::confirm;
    s.cursor = 0;
    break;
  case Session::Page::remoteMenu: {
    if(s.cursor==5){s.page=Session::Page::remoteRepeat;s.cursor=0;return;}
    static constexpr somfy_commands commands[]={somfy_commands::UpDown,somfy_commands::MyUpDown,somfy_commands::MyUp,somfy_commands::MyDown,somfy_commands::Prog};
    if(s.cursor>=5)return;
    s.program.command=commands[s.cursor];s.page=Session::Page::remoteConfirm;s.cursor=0;return;
  }
  case Session::Page::remoteRepeat:
    s.page=Session::Page::remoteMenu;s.cursor=5;return;
  case Session::Page::remoteConfirm:
    if(!s.cursor){s.page=Session::Page::remoteMenu;s.cursor=0;return;}
    s.queuedAt=now;s.page=Session::Page::remotePending;s.cursor=0;
    {
      const auto queued=backend.queueLearn(s,now);
      if(queued.reason==LearnQueueReason::ready)s.ticket=queued.token;
      else {s.programSent=false;s.page=Session::Page::remoteResult;}
    }
    return;
  case Session::Page::remotePending:
  case Session::Page::remoteResult:
    close(Notice::none);return;
  case Session::Page::list: {
    if(s.target.scope==Scope::repeaters&&s.cursor==backend.itemCount(s.target)){
      if(backend.itemCount(s.target)>=7){status=Notice::capacity;return;}
      s.page=Session::Page::learnStart;s.cursor=0;status=Notice::none;return;
    }
    if (!backend.itemCount(s.target)) {
      close(Notice::none);
      return;
    }
    Item item;
    if (!backend.itemAt(s.target, s.cursor, item)) {
      close(Notice::changed);
      return;
    }
    if (s.target.scope == Scope::groups && !item.missing) {
      status = Notice::invalid;
      break;
    }
    s.action = {};
    s.action.key = item.key;
    s.action.slot = item.slot;
    s.action.kind =
        s.target.scope == Scope::repeaters ? ActionKind::unlinkRepeater
        : s.target.scope == Scope::groups  ? ActionKind::removeMissingMember
                                           : ActionKind::unlinkRemote;
    s.returnPage = Session::Page::list;
    s.page = Session::Page::confirm;
    s.cursor = 0;
    break;
  }
  case Session::Page::confirm:
    if (!s.cursor) {
      s.action = {};
      s.page = s.returnPage;
      s.cursor = 0;
      status = Notice::none;
    } else
      apply();
    break;
  case Session::Page::refused:
    if (s.cursor) {
      close(Notice::none);
      return;
    }
    s.page = Session::Page::confirm;
    s.cursor = 0;
    status = Notice::none;
    break;
  case Session::Page::learnStart:
    if (!s.cursor) {
      returnFromLearning();
      break;
    }
    if (!s.learned.candidates.begin(now)) {
      returnFromLearning();
      status = Notice::busy;
      break;
    }
    s.listening = true;
    s.learned.selection = {};
    s.page = Session::Page::learnList;
    s.cursor = 0;
    break;
  case Session::Page::learnList:
    if (!s.learned.candidates.count())
      break;
    if (s.learned.candidates.select(s.cursor, now, s.learned.selection) != LearnCheck::ready) {
      endLearning();
      s.page = Session::Page::learnStart;
      s.cursor = 0;
      status = Notice::changed;
      break;
    }
    s.page = Session::Page::learnReview;
    s.cursor = 0;
    break;
  case Session::Page::learnReview:
    if (!s.cursor) {
      s.page = Session::Page::learnList;
      s.cursor = 0;
    } else
      queueLearn(now, false);
    break;
  case Session::Page::learnPending:
    close(Notice::none);
    return;
  case Session::Page::learnFault:
    if (!s.cursor) {
      returnFromLearning();
    } else {
      s.page = s.returnPage;
      s.cursor = 0;
    }
    status = Notice::none;
    break;
  case Session::Page::learnResult:
    if (s.canRetryFile && !s.cursor) {
      queueLearn(now, true);
      break;
    }
    close(Notice::none);
    return;
  case Session::Page::result: {
    if (s.cursor) {
      close(Notice::none);
      return;
    }
    const auto result =
        backend.retryFile(s.target, s.context, s.auth, s.revision);
    if (result.admission != Admission::accepted) {
      refused(result.admission);
      if (session)
        session->cursor = 1;
      return;
    }
    if (result.file.saved()) {
      close(Notice::saved);
      return;
    }
    status = result.file.state == ModelCommitState::uncertain
                 ? Notice::uncertain
                 : Notice::appliedNotSaved;
    s.cursor = 1;
    break;
  }
  }
}
void Form::back(uint64_t now) {
  if (!guard(now))
    return;
  touch(now);
  auto &s = *session;
  if(s.membershipMode){membershipEffect(s.membership.back(),now);return;}
  if(s.page==Session::Page::remotePending||s.page==Session::Page::remoteResult){close(Notice::none);return;}
  if(s.page==Session::Page::remoteConfirm||s.page==Session::Page::remoteRepeat){s.page=Session::Page::remoteMenu;s.cursor=0;return;}
  if(s.page==Session::Page::remoteMenu){s.programMode=false;s.page=s.target.scope==Scope::groups?Session::Page::groupMenu:Session::Page::menu;s.cursor=s.target.scope==Scope::groups?2:5;return;}
  if(s.page==Session::Page::primaryCounter){primaryEffect(s.primary.back(now),now);return;}
  if(s.page==Session::Page::details){s.page=s.target.scope==Scope::groups?Session::Page::groupMenu:Session::Page::menu;s.cursor=s.target.scope==Scope::groups?1:4;return;}
  if (s.page == Session::Page::learnPending ||
      s.page == Session::Page::learnResult) {
    close(Notice::none);
    return;
  }
  if (s.page == Session::Page::learnReview) {
    s.page = Session::Page::learnList;
    s.cursor = 0;
    return;
  }
  if (s.page == Session::Page::learnStart ||
      s.page == Session::Page::learnList ||
      s.page == Session::Page::learnFault) {
    returnFromLearning();
    status = Notice::none;
    return;
  }
  if (s.page == Session::Page::refused) {
    s.page = Session::Page::confirm;
    s.cursor = 0;
    status = Notice::none;
  } else if (s.page == Session::Page::confirm) {
    s.action = {};
    s.page = s.returnPage;
    s.cursor = 0;
    status = Notice::none;
  } else if (s.page == Session::Page::list &&
             (s.target.scope == Scope::devices||backend.supportsProgramming(s.target))) {
    s.page = Session::Page::menu;
    s.cursor = 1;
    status = Notice::none;
  } else
    close(Notice::none);
}
void Form::render(FormView &out) {
  out = {};
  out.notice = status;
  if (!session)
    return;
  auto &s = *session;
  out.id = s.target.id;
  out.kind = s.target.scope == Scope::groups ? Kind::group : Kind::shade;
  out.mode = Mode::fields;
  out.selected = s.cursor;
  if (s.target.scope == Scope::repeaters)
    strcpy(out.title, "Repeaters");
  else if (s.page == Session::Page::picker)
    strcpy(out.title, s.target.scope == Scope::devices ? "Advanced devices"
                                                       : "Advanced groups");
  else {
    Choice label;
    if (!backend.targetLabel(s.target, s.selectedSlot, label) ||
        label.value != s.target.id ||
        !memchr(label.label, 0, sizeof(label.label))) {
      close(Notice::changed);
      out = {};
      out.notice = status;
      return;
    }
    snprintf(out.title, sizeof(out.title), "%s", label.label);
  }
  if(s.membershipMode){
    s.membership.render(out,s.target.id,s.refusal);
    for(uint8_t i=0;i<out.rowCount;++i){const auto index=uint8_t(out.offset+i);
      if(!s.membership.needsLabel(index))continue;
      Choice choice;char detail[48]={};
      if(!backend.membershipLabel(s.target,s.membership.adding(),s.membership.rows(),index,choice,detail) ||
         choice.value!=s.membership.rows().ids[index] || !memchr(choice.label,0,sizeof(choice.label)) || !memchr(detail,0,sizeof(detail))){close(Notice::changed);out={};out.notice=status;return;}
      snprintf(out.rows[i].label,sizeof(out.rows[i].label),"%s",choice.label);
      snprintf(out.rows[i].value,sizeof(out.rows[i].value),"%s",detail);
    }
    return;
  }
  if(s.page==Session::Page::primaryCounter){s.primary.render(out,s.target.id);return;}
  if(s.programMode) {
    static const char *const labels[]={"Up + Down","MY + Up + Down","MY + Up","MY + Down","Prog","Repeats"};
    strcpy(out.hint,"May move motor or change limits/pairing.");
    if(s.page==Session::Page::remoteMenu) {
      out.total=6;out.offset=s.cursor/3*3;
      for(uint8_t i=out.offset;i<6&&out.rowCount<3;++i){auto &row=out.rows[out.rowCount++];strcpy(row.label,labels[i]);if(i==5)snprintf(row.value,sizeof(row.value),"%u",unsigned(s.program.repeats));}
    } else if(s.page==Session::Page::remoteRepeat) {
      out.total=out.rowCount=1;strcpy(out.rows[0].label,"Repeats");snprintf(out.rows[0].value,sizeof(out.rows[0].value),"%u",unsigned(s.program.repeats));strcpy(out.hint,"Turn: 1..127 repeats. Press: keep. Back: review.");
    } else if(s.page==Session::Page::remoteConfirm) {
      out.mode=Mode::confirm;out.total=out.rowCount=2;strcpy(out.rows[0].label,"Cancel");strcpy(out.rows[1].label,"Send remote command");
      const char *command=s.program.command==somfy_commands::UpDown?labels[0]:s.program.command==somfy_commands::MyUpDown?labels[1]:s.program.command==somfy_commands::MyUp?labels[2]:s.program.command==somfy_commands::MyDown?labels[3]:labels[4];
      snprintf(out.hint,sizeof(out.hint),"%s x%u; may move/change limits/pairing.",command,unsigned(s.program.repeats));
    } else {
      out.mode=Mode::result;out.total=out.rowCount=1;
      strcpy(out.rows[0].label,s.page==Session::Page::remotePending?"Cancel queued request":s.programSent?"Request accepted":"Request refused");
      strcpy(out.hint,s.page==Session::Page::remotePending?"Waiting for radio. Back cancels before execution.":s.programSent?"Accepted by controller; observe motor. No acknowledgement.":"Not sent. Close and review before a new request.");
    }
    for(uint8_t i=0;i<out.rowCount;++i)out.rows[i].selected=out.offset+i==out.selected;
    return;
  }
  if(s.page==Session::Page::groupMenu){
    out.total=out.rowCount=3;strcpy(out.rows[0].label,"Member links");strcpy(out.rows[1].label,"Status details");strcpy(out.rows[2].label,"Advanced remote");
    strcpy(out.hint,"Read status or edit controller list.");for(uint8_t i=0;i<3;++i)out.rows[i].selected=i==s.cursor;return;
  }
  if(s.page==Session::Page::details){
    out.mode=Mode::stackedRows;out.total=entityStatusRows;out.offset=s.cursor/3*3;
    strcpy(out.hint,"Read only; positions and movement are estimates.");
    for(uint8_t i=out.offset;i<out.total&&out.rowCount<3;++i){auto &row=out.rows[out.rowCount++];
      if(!backend.statusRow(s.target,i,row)){strcpy(row.label,"Status unavailable");row.value[0]=0;}
      row.selected=i==s.cursor;
    }return;
  }

  strcpy(out.hint, "Press: choose   Back: cancel");
  if (s.page == Session::Page::learnStart ||
      s.page == Session::Page::learnReview ||
      s.page == Session::Page::learnPending ||
      s.page == Session::Page::learnFault ||
      s.page == Session::Page::learnResult) {
    out.mode = Mode::validation;
    out.total = out.rowCount = 2;
    strcpy(out.rows[0].label, "Cancel");
    if (s.page == Session::Page::learnStart) {
      strcpy(out.rows[1].label, "Begin observation");
      strcpy(out.hint,
             s.target.scope==Scope::repeaters?"Future frames may retransmit. Use only one repeater.":"Future received commands may affect device; Link sends no RF.");
    } else if (s.page == Session::Page::learnReview) {
      out.mode = Mode::learnReview;
      strcpy(out.rows[1].label,s.target.scope==Scope::repeaters?"Add repeater":"Link received remote");
      snprintf(out.rows[2].label, sizeof(out.rows[2].label), "#%lu Code %u",
               static_cast<unsigned long>(s.learned.selection.address),
               unsigned(s.learned.selection.rollingCode));
      LearnCandidate candidate;
      for (uint8_t i = 0; i < s.learned.candidates.count(); i++)
        if (s.learned.candidates.candidate(i, candidate) &&
            candidate.address == s.learned.selection.address &&
            candidate.protocol == s.learned.selection.protocol &&
            candidate.bits == s.learned.selection.bits)
          break;
      char rawCommand[8];
      snprintf(rawCommand, sizeof(rawCommand), "0x%02X",
               unsigned(candidate.command));
      const char *command =
          somfyCommandLabel(static_cast<somfy_commands>(candidate.command));
      char signal[20];
      if (candidate.rssi)
        snprintf(signal, sizeof(signal), "%ddBm", int(candidate.rssi));
      else
        snprintf(signal, sizeof(signal), "RSSI unavailable");
      snprintf(out.rows[2].value, sizeof(out.rows[2].value), "%s %u-bit %s %s",
               s.learned.selection.protocol == 0   ? "RTS"
               : s.learned.selection.protocol == 1 ? "RTW"
                                           : "RTV",
               unsigned(s.learned.selection.bits), command ? command : rawCommand,
               signal);
      strcpy(out.hint,s.target.scope==Scope::repeaters?
        "Future frames may retransmit. One repeater controller only.":
        "Links address/counter; observed format is not a filter.");
    } else if (s.page == Session::Page::learnPending) {
      strcpy(out.title, s.fileOnly ? "File save queued" : s.target.scope==Scope::repeaters?"Repeater add queued":"Link queued");
      out.total = out.rowCount = 1;
      strcpy(out.rows[0].label, "Cancel queued operation");
      strcpy(out.hint,
             "FIFO; expires after 5s. Back cancels. No automatic retry.");
    } else if (s.page == Session::Page::learnFault) {
      strcpy(out.title,s.target.scope==Scope::repeaters?"Repeater not added":"Link not applied");
      strcpy(out.rows[0].label, "Cancel learning");
      strcpy(out.rows[1].label, s.returnPage == Session::Page::learnList
                                    ? "Choose received frame"
                                    : "Back to review");
      snprintf(out.hint, sizeof(out.hint), "%s", s.refusal);
    } else {
      strcpy(out.rows[0].label, "Close");
      out.total = out.rowCount = 1;
      if (s.canRetryFile) {
        strcpy(out.title, s.counterOutcome == CounterDomainState::modelUncertain
                              ? "Linked; file uncertain"
                              : "Linked; file not saved");
        out.total = out.rowCount = 2;
        strcpy(out.rows[0].label, "Retry model file save");
        strcpy(out.rows[1].label, "Close");
        snprintf(
            out.hint, sizeof(out.hint), "%s",
            s.refusal[0]
                ? s.refusal
                : "Counter/link applied. Commit file only; never Link again.");
      } else if (s.linkApplied && s.fileVerified &&
                 s.counterOutcome != CounterDomainState::nvsUncertain &&
                 s.counterOutcome != CounterDomainState::quarantined) {
        strcpy(out.title, "Remote linked");
        strcpy(out.hint,
               s.counterOutcome == CounterDomainState::saved
                   ? "Counter and model file verified. Link sent no RF."
                   : "Link applied; file saved. Review counter outcome.");
      } else if (s.counterOutcome == CounterDomainState::nvsUncertain) {
        strcpy(out.title, "Counter uncertain");
        strcpy(out.hint,
               "Link not applied; stored code unknown. Do not replay/reset.");
      } else if (s.counterOutcome == CounterDomainState::quarantined) {
        strcpy(out.title, s.linkApplied ? "Link recovery blocked"
                                        : "Counter blocked");
        strcpy(
            out.hint,
            s.linkApplied
                ? "Earlier link kept. Counter state unknown. Do not replay."
                : "Earlier counter state needs recovery. No link was applied.");
      } else {
        strcpy(out.title, "Link not applied");
        strcpy(out.hint,
               "Outcome unavailable. No success reported; review state.");
      }
    }
  } else if (s.page == Session::Page::learnList) {
    out.total = s.learned.candidates.count();
    out.offset = s.cursor / 3 * 3;
    strcpy(out.hint, s.learned.candidates.overflowed()
                         ? "8 max; choose frame to review"
                         : "30s window. Choose frame to review.");
    if (!out.total) {
      out.total = out.rowCount = 1;
      strcpy(out.rows[0].label, "Waiting for remote frame");
    }
    for (uint8_t i = out.offset; i < s.learned.candidates.count() && out.rowCount < 3;
         i++) {
      LearnCandidate candidate;
      if (!s.learned.candidates.candidate(i, candidate)) {
        close(Notice::changed);
        out = {};
        out.notice = status;
        return;
      }
      auto &row = out.rows[out.rowCount++];
      snprintf(row.label, sizeof(row.label), "#%lu",
               static_cast<unsigned long>(candidate.address));
      snprintf(row.value, sizeof(row.value), "Code %u",
               unsigned(candidate.rollingCode));
    }
  } else if (s.page == Session::Page::refused) {
    out.mode = Mode::validation;
    out.total = out.rowCount = 2;
    strcpy(out.rows[0].label, "Back to review");
    strcpy(out.rows[1].label, "Close");
    snprintf(out.hint, sizeof(out.hint), "%s", s.refusal);
  } else if (s.page == Session::Page::confirm) {
    out.mode = Mode::confirm;
    out.total = out.rowCount = 2;
    strcpy(out.rows[0].label, "Cancel");
    if (s.action.kind == ActionKind::setPaired) {
      strcpy(out.rows[1].label,
             s.action.paired ? "Mark paired" : "Mark unpaired");
      strcpy(out.hint, "Marker only. Motor pairing is unchanged.");
    } else {
      snprintf(out.rows[1].label, sizeof(out.rows[1].label), "Remove #%lu",
               static_cast<unsigned long>(s.action.key));
      strcpy(out.hint, s.action.kind == ActionKind::removeMissingMember
                           ? "Remove all stale links. No physical unpair."
                       : s.action.kind == ActionKind::unlinkRepeater
                           ? "Stop future repeats. No physical unpair."
                           : "Remove association. No physical unpair.");
    }
  } else if (s.page == Session::Page::result) {
    out.mode = Mode::result;
    out.total = out.rowCount = 2;
    strcpy(out.rows[0].label, "Retry model save");
    strcpy(out.rows[1].label, "Close");
    strcpy(out.hint, "Applied already. Retry saves the model file only.");
  } else if (s.page == Session::Page::menu) {
    out.total=6;out.offset=s.cursor/3*3;
    for(uint8_t i=out.offset;i<6 && out.rowCount<3;++i) {
      auto &row=out.rows[out.rowCount++];
      if(i==5)strcpy(row.label,"Advanced remote");
      else if(i==4)strcpy(row.label,"Status details");
      else if(i==0){strcpy(row.label,"Paired marker");strcpy(row.value,s.paired?"Paired":"Unpaired");}
      else strcpy(row.label,i==1?"Linked remotes":i==2?"Learn received remote":"Primary rolling code");
    }
  } else {
    out.total =
        s.page == Session::Page::picker ? s.count : backend.itemCount(s.target)+(s.target.scope==Scope::repeaters?1:0);
    out.offset = s.cursor / 3 * 3;
    if (!out.total) {
      out.total = out.rowCount = 1;
      strcpy(out.rows[0].label, "Close (empty list)");
    } else
      for (uint8_t i = out.offset; i < out.total && out.rowCount < 3; i++) {
        auto &row = out.rows[out.rowCount++];
        if(s.page==Session::Page::list&&s.target.scope==Scope::repeaters&&i==backend.itemCount(s.target)){
          strcpy(row.label,"Learn/add repeater");strcpy(row.value,backend.itemCount(s.target)>=7?"Full":"Receive then review");continue;
        }
        if (s.page == Session::Page::picker) {
          Choice label;
          if (!backend.targetLabel({s.target.scope, s.ids[i]}, s.slots[i],
                                   label) ||
              label.value != s.ids[i] ||
              !memchr(label.label, 0, sizeof(label.label))) {
            close(Notice::changed);
            out = {};
            out.notice = status;
            return;
          }
          snprintf(row.label, sizeof(row.label), "%s", label.label);
        } else {
          Item item;
          if (!backend.itemAt(s.target, i, item) ||
              !memchr(item.label, 0, sizeof(item.label))) {
            close(Notice::changed);
            out = {};
            out.notice = status;
            return;
          }
          snprintf(row.label, sizeof(row.label), "%s", item.label);
          if (s.target.scope == Scope::devices)
            snprintf(row.value, sizeof(row.value), "Code %u",
                     unsigned(item.rollingCode));
          if (s.target.scope == Scope::groups)
            strcpy(row.value, item.missing ? "Missing: repair" : "View only");
        }
      }
  }
  for (uint8_t i = 0; i < out.rowCount; i++)
    out.rows[i].selected = out.offset + i == out.selected;
}
} // namespace Advanced
} // namespace LocalUi
