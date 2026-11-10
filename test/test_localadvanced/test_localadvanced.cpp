#include "CounterResults.h"
#include "LocalAdvanced.h"
#include "LocalJsonAllocator.h"
#include <map>
#include <stdlib.h>
#include <string.h>
#include <unity.h>
using namespace LocalUi;
using namespace EntityAdmin;
using namespace Advanced;
static unsigned releases;
static size_t peak;
static std::map<void *, size_t> blocks;
static void *acquire(size_t n) {
  if (n > peak)
    peak = n;
  void *p = malloc(n);
  if (p)
    blocks[p] = n;
  return p;
}
static void release(void *p) {
  for (size_t i = 0; i < blocks[p]; i++)
    TEST_ASSERT_EQUAL_UINT8(0, static_cast<unsigned char *>(p)[i]);
  blocks.erase(p);
  ++releases;
  free(p);
}
struct Model : Advanced::Backend {
  Authorization auth{true, 1, 1};
  uint64_t history = 1;
  unsigned saves = 0, retries = 0, targetReads = 0;
  bool marked = false;
  bool stale = false, badLabel = false;
  uint8_t total = 1,repeaterTotal=1;
  uint32_t repeater = 1234;
  Admission refusal = Admission::accepted, retryRefusal = Admission::accepted;
  ModelCommitState file = ModelCommitState::saved;
  ReceiveHub received;
  LearnQueue learnedQueue;
  unsigned links = 0, counterWrites = 0, repeaterAdds=0;
  uint64_t learnedToken = 0;
  CounterDomainState counterState = CounterDomainState::saved;
  LearnQueueReason lateFreshness = LearnQueueReason::ready;
  RestartReason fileRetryRefusal = RestartReason::none;
  uint16_t primaryCode=1000,storedPrimary=1000,aliasCode=1000;
  bool storedPresent=true,primaryFresh=true;
  unsigned corrections=0;
  PrimaryReadResult readPrimary(Target target,const Context &,Authorization,uint64_t,PrimaryCounterReview &out) override {
    out={};if(refusal!=Admission::accepted)return {CounterDomainState::invalid,refusal};
    out.shadeId=target.id;out.address=100;out.primary=primaryCode;out.stored={storedPrimary,storedPresent};out.matching[0]=3;out.codes[0]=primaryCode;out.codes[1]=aliasCode;
    return {CounterDomainState::saved};
  }
  bool primaryCurrent(Target,const PrimaryCounterReview &review) override {return primaryFresh && review.primary==primaryCode && review.codes[1]==aliasCode;}
  PrimaryApplyResult correctPrimary(Target target,const Context &,Authorization,uint64_t,const PrimaryCounterReview &review,uint16_t code,LearnPending &witness) override {
    const auto fresh=witness.current(40);if(fresh!=LearnQueueReason::ready)return {{},Admission::accepted,fresh};
    ++corrections;const bool applied=counterState==CounterDomainState::saved || counterState==CounterDomainState::modelFailed || counterState==CounterDomainState::modelUncertain;
    if(applied || counterState==CounterDomainState::nvsUncertain)++counterWrites;
    if(applied){++history;primaryCode=storedPrimary=aliasCode=code;storedPresent=true;}
    return {{counterState,applied,{file,ModelCommitStage::write}}};
  }
  ReceiveHub &receivedFrames() override { return received; }
  LearnQueued queueLearn(LearnPending &pending, uint64_t now) override {
    auto result = learnedQueue.reserve(pending, now);
    learnedToken = result.token;
    return result;
  }
  bool cancelLearn(uint64_t token, LearnPending &pending) override {
    return learnedQueue.detach(token, pending);
  }
  Admission checkLearnContext(Target target, const Context &expected,
                              Authorization admitted, uint64_t rev) override {
    if (!auth.allowed)
      return Admission::locked;
    Context actual;
    capture(target, actual);
    return admitted.generation == auth.generation &&
                   admitted.configurationGeneration ==
                       auth.configurationGeneration &&
                   rev == history && expected.same(actual)
               ? Admission::accepted
               : Admission::stale;
  }
  LearnApplyResult linkLearn(Target target, const Context &context,
                             Authorization admitted, uint64_t rev,
                             const LearnSelection &candidate,
                             LearnPending &witness,
                             ArduinoJson::Allocator &) override {
    if (witness.current(20) != LearnQueueReason::ready)
      return {{}, Admission::stale};
    if (lateFreshness != LearnQueueReason::ready)
      return {{}, Admission::accepted, lateFreshness};
    if(target.scope==Scope::repeaters){
      ++repeaterAdds;repeater=candidate.address;++history;
      return {edit_result_t::completed(0,{file,ModelCommitStage::write})};
    }
    ++links;
    const bool applied = counterState == CounterDomainState::saved ||
                         counterState == CounterDomainState::modelFailed ||
                         counterState == CounterDomainState::modelUncertain;
    if (applied || counterState == CounterDomainState::nvsUncertain)
      ++counterWrites;
    if (applied)
      ++history;
    return {counterLinkResult(
        target.id, {counterState, applied, {file, ModelCommitStage::write}})};
  }
  LearnFileResult retryLearnFile(Target, const Context &, Authorization,
                                 uint64_t, LearnPending &) override {
    ++retries;
    LearnFileResult result;
    result.report.file = {ModelCommitState::saved};
    result.report.refusal = fileRetryRefusal;
    return result;
  }
  Authorization authorization() override { return auth; }
  uint64_t revision() override { return history; }
  void activity() override {}
  bool targets(Scope, OrderIds &ids, OrderIds &slots, uint8_t &count) override {
    ++targetReads;
    for (uint8_t i = 0; i < total; i++) {
      ids[i] = i ? (i >= 27 ? i + 1 : i) : 27;
      slots[i] = 31 - i;
    }
    count = total;
    return true;
  }
  bool targetLabel(Target target, uint8_t slot, Choice &out) override {
    if (slot == 31) {
      out.value = 27;
      strcpy(out.label, "#27 Office");
    } else {
      out.value = target.id;
      snprintf(out.label, sizeof(out.label), "#%u Device", 31 - slot);
    }
    if (badLabel)
      ++out.value;
    return true;
  }
  bool capture(Target target, Context &out) override {
    out = {};
    out.scope = target.scope;
    out.history = history;
    if (target.scope != Scope::repeaters) {
      auto &r = out.raw;
      r.ref = {target.scope == Scope::devices ? EntityKind::device
                                              : EntityKind::group,
               target.id};
      r.targetExists = true;
      r.identity = 100;
      r.revision = r.modelRevision = r.restoreRevision = r.radioRevision = 1;
      if (stale)
        ++r.identity;
    } else
      out.repeaters[0] = repeater;
    return true;
  }
  bool paired(Target, bool &out) override {
    out = marked;
    return true;
  }
  uint8_t itemCount(Target target) override { return target.scope==Scope::repeaters?repeaterTotal:1; }
  bool itemAt(Target target, uint8_t index, Item &out) override {
    if(target.scope==Scope::repeaters){if(index>=repeaterTotal)return false;out={};out.key=repeater+index;out.slot=6-index;snprintf(out.label,sizeof(out.label),"%lu",static_cast<unsigned long>(out.key));return true;}
    if (index)
      return false;
    out = {};
    out.key = 1234;
    out.rollingCode = 65535;
    out.slot = 6;
    out.missing = true;
    strcpy(out.label, "1234");
    return true;
  }
  ApplyResult apply(Target, const Action &action, const Context &,
                    Authorization, uint64_t,
                    ArduinoJson::Allocator &) override {
    if (refusal != Admission::accepted)
      return {{}, refusal};
    ++saves;
    marked = action.paired;
    ++history;
    return {edit_result_t::completed(27, {file, ModelCommitStage::write})};
  }
  CommitResult retryFile(Target, const Context &, Authorization,
                         uint64_t) override {
    if (retryRefusal != Admission::accepted)
      return {{}, retryRefusal};
    ++retries;
    return {{ModelCommitState::saved}};
  }
};
void setUp() {
  releases = 0;
  peak = 0;
  TEST_ASSERT_TRUE(blocks.empty());
}
void tearDown() { TEST_ASSERT_TRUE(blocks.empty()); }
static void cancel_is_default_and_rotation_never_applies() {
  Model model;
  JsonAllocator memory(4096, acquire, release);
  Advanced::Form form(model, memory);
  TEST_ASSERT_TRUE(form.open(Scope::devices, 10));
  form.activate(11); // choose #27
  FormView view;
  form.render(view);
  TEST_ASSERT_EQUAL_STRING("#27 Office", view.title);
  form.activate(12); // paired marker -> confirm
  for (int i = 0; i < 30; i++)
    form.render(view);
  TEST_ASSERT_EQUAL(1, model.targetReads);
  TEST_ASSERT_LESS_OR_EQUAL(2304, peak);
  TEST_ASSERT_EQUAL(0, model.saves);
  TEST_ASSERT_EQUAL(Mode::confirm, view.mode);
  TEST_ASSERT_EQUAL_STRING("Marker only. Motor pairing is unchanged.",
                           view.hint);
  TEST_ASSERT_EQUAL(0, view.selected);
  form.activate(13); // default Cancel
  TEST_ASSERT_EQUAL(0, model.saves);
  form.discard();
  TEST_ASSERT_EQUAL(0, memory.used());
}
static void applied_failure_retries_file_only() {
  Model model;
  model.file = ModelCommitState::failed;
  JsonAllocator memory(4096, acquire, release);
  Advanced::Form form(model, memory);
  form.open(Scope::devices, 10);
  form.activate(11);
  form.activate(12);
  form.turn(1, 13);
  form.activate(14);
  FormView view;
  form.render(view);
  TEST_ASSERT_EQUAL(Mode::result, view.mode);
  TEST_ASSERT_EQUAL(Notice::appliedNotSaved, view.notice);
  TEST_ASSERT_EQUAL(1, model.saves);
  form.turn(-1, 15);
  form.activate(16);
  TEST_ASSERT_EQUAL(1, model.retries);
  TEST_ASSERT_EQUAL(1, model.saves);
  TEST_ASSERT_TRUE(form.closed());
  TEST_ASSERT_EQUAL(0, memory.used());
}
static void context_and_authorization_changes_close_and_free() {
  for (unsigned which = 0; which < 3; which++) {
    Model model;
    JsonAllocator memory(4096, acquire, release);
    Advanced::Form form(model, memory);
    form.open(Scope::devices, 10);
    form.activate(11);
    form.activate(12);
    if (which == 0)
      ++model.auth.generation;
    else if (which == 1)
      model.stale = true;
    else
      ++model.history;
    form.tick(13);
    TEST_ASSERT_TRUE(form.closed());
    TEST_ASSERT_EQUAL(0, memory.used());
    TEST_ASSERT_EQUAL(0, model.saves);
  }
}
static void allocation_timeout_and_repeater_context() {
  Model model;
  JsonAllocator none(0, acquire, release);
  Advanced::Form denied(model, none);
  TEST_ASSERT_FALSE(denied.open(Scope::devices, 1));
  TEST_ASSERT_EQUAL(Notice::noMemory, denied.notice());
  JsonAllocator memory(4096, acquire, release);
  Advanced::Form form(model, memory);
  form.open(Scope::repeaters, 10);
  FormView view;
  form.render(view);
  TEST_ASSERT_EQUAL_STRING("Repeaters", view.title);
  form.activate(11);
  form.render(view);
  TEST_ASSERT_EQUAL(Mode::confirm, view.mode);
  TEST_ASSERT_TRUE(view.protectedFrame);
  form.tick(120011);
  TEST_ASSERT_TRUE(form.closed());
  TEST_ASSERT_EQUAL(0, memory.used());
  TEST_ASSERT_EQUAL(0, model.saves);
}
static void full_paging_repeater_change_and_admission_failure() {
  Model model;
  model.total = 32;
  JsonAllocator memory(4096, acquire, release);
  Advanced::Form form(model, memory);
  TEST_ASSERT_TRUE(form.open(Scope::devices, 10));
  for (unsigned i = 0; i < 31; i++)
    form.turn(1, 11 + i);
  FormView view;
  form.render(view);
  TEST_ASSERT_EQUAL(32, view.total);
  TEST_ASSERT_EQUAL(2, view.rowCount);
  TEST_ASSERT_EQUAL(30, view.offset);
  form.discard();
  form.open(Scope::repeaters, 100);
  model.repeater = 2345;
  form.tick(101);
  TEST_ASSERT_TRUE(form.closed());
  TEST_ASSERT_EQUAL(Notice::changed, form.notice());
  model.total = 1;
  form.open(Scope::devices, 200);
  form.activate(201);
  form.activate(202);
  form.turn(1, 203);
  model.refusal = Admission::noMemory;
  form.activate(204);
  TEST_ASSERT_TRUE(form.closed());
  TEST_ASSERT_EQUAL(Notice::noMemory, form.notice());
  TEST_ASSERT_EQUAL(0, model.saves);
  TEST_ASSERT_EQUAL(0, memory.used());
}
static void file_retry_busy_keeps_commit_only_result() {
  Model model;
  model.file = ModelCommitState::failed;
  JsonAllocator memory(4096, acquire, release);
  Advanced::Form form(model, memory);
  form.open(Scope::devices, 10);
  form.activate(11);
  form.activate(12);
  form.turn(1, 13);
  form.activate(14);
  model.retryRefusal = Admission::busy;
  form.turn(-1, 15);
  form.activate(16);
  FormView view;
  form.render(view);
  TEST_ASSERT_EQUAL(Mode::result, view.mode);
  TEST_ASSERT_EQUAL(1, view.selected);
  TEST_ASSERT_EQUAL(1, model.saves);
  TEST_ASSERT_EQUAL(0, model.retries);
  model.retryRefusal = Admission::accepted;
  form.turn(-1, 17);
  form.activate(18);
  TEST_ASSERT_EQUAL(1, model.retries);
  TEST_ASSERT_EQUAL(1, model.saves);
}
static void stable_label_identity_is_required() {
  for (unsigned picker = 0; picker < 2; picker++) {
    Model model;
    JsonAllocator memory(4096, acquire, release);
    Advanced::Form form(model, memory);
    form.open(Scope::devices, 10);
    if (!picker)
      form.activate(11);
    model.badLabel = true;
    FormView view;
    form.render(view);
    TEST_ASSERT_TRUE(form.closed());
    TEST_ASSERT_EQUAL(Notice::changed, view.notice);
    TEST_ASSERT_EQUAL(0, memory.used());
  }
}
static void linked_remote_list_keeps_readonly_counter_visible() {
  Model model;
  JsonAllocator memory(4096, acquire, release);
  Advanced::Form form(model, memory);
  form.open(Scope::devices, 10);
  form.activate(11);
  form.turn(1, 12);
  form.activate(13);
  FormView view;
  form.render(view);
  TEST_ASSERT_EQUAL_STRING("Code 65535", view.rows[0].value);
  TEST_ASSERT_EQUAL(0, model.saves);
}
static void confirmation_names_exact_association_and_configuration_effect() {
  Model model;
  JsonAllocator memory(4096, acquire, release);
  Advanced::Form form(model, memory);
  for (auto scope : {Scope::devices, Scope::groups, Scope::repeaters}) {
    form.open(scope, 10);
    if (scope != Scope::repeaters)
      form.activate(11);
    if (scope == Scope::devices) {
      form.turn(1, 12);
      form.activate(13);
    }
    form.activate(14);
    FormView view;
    form.render(view);
    TEST_ASSERT_EQUAL(Mode::confirm, view.mode);
    TEST_ASSERT_NOT_NULL(strstr(view.rows[1].label, "1234"));
    TEST_ASSERT_EQUAL_STRING(
        scope == Scope::groups ? "Remove all stale links. No physical unpair."
        : scope == Scope::repeaters ? "Stop future repeats. No physical unpair."
                                    : "Remove association. No physical unpair.",
        view.hint);
    TEST_ASSERT_EQUAL(0, view.selected);
    form.discard();
  }
  TEST_ASSERT_EQUAL(0, model.saves);
}
static void busy_mutation_has_visible_refusal_and_explicit_review_again() {
  Model model;
  JsonAllocator memory(4096, acquire, release);
  Advanced::Form form(model, memory);
  form.open(Scope::devices, 10);
  form.activate(11);
  form.activate(12);
  form.turn(1, 13);
  model.refusal = Admission::busy;
  form.activate(14);
  FormView view;
  form.render(view);
  TEST_ASSERT_EQUAL(Mode::validation, view.mode);
  TEST_ASSERT_NOT_NULL(strstr(view.hint, "Busy"));
  TEST_ASSERT_EQUAL(0, model.saves);
  model.refusal = Admission::accepted;
  form.activate(15);
  form.render(view);
  TEST_ASSERT_EQUAL(Mode::confirm, view.mode);
  TEST_ASSERT_EQUAL(0, view.selected);
  form.activate(16);
  TEST_ASSERT_EQUAL(0, model.saves);
  form.activate(17);
  form.turn(1, 18);
  form.activate(19);
  TEST_ASSERT_EQUAL(1, model.saves);
}
static void received(Model &model, uint64_t at, uint32_t address, uint16_t code,
                     uint8_t bits = 56, uint8_t protocol = 0) {
  ReceiveObservation v;
  v.observedAt = at;
  v.address = address;
  v.rollingCode = code;
  v.bits = bits;
  v.protocol = protocol;
  v.flags = ReceiveObservation::validFlag;
  TEST_ASSERT_TRUE(model.received.publish(v));
}
static void openLearn(Advanced::Form &form) {
  TEST_ASSERT_TRUE(form.open(Scope::devices, 0));
  form.activate(1);
  form.turn(1, 2);
  form.turn(1, 3);
  form.activate(4);
}
static void
learning_requires_explicit_start_frozen_review_and_queue_execution() {
  Model model;
  JsonAllocator memory(4096, acquire, release);
  Advanced::Form form(model, memory);
  FormView view;
  openLearn(form);
  char budget[96];
  snprintf(budget, sizeof(budget),
           "Learn slab/header=%zu bytes; outer handle=%zu", peak,
           sizeof(Advanced::Form));
  TEST_MESSAGE(budget);
  form.render(view);
  TEST_ASSERT_EQUAL_STRING("Begin observation", view.rows[1].label);
  TEST_ASSERT_EQUAL(0, view.selected);
  TEST_ASSERT_FALSE(model.received.active());
  form.activate(5);
  TEST_ASSERT_FALSE(model.received.active()); // default Cancel returns menu
  form.activate(6);
  form.turn(1, 7);
  form.activate(8);
  TEST_ASSERT_TRUE(model.received.active());
  received(model, 9, 0xabc, 20);
  form.activate(10);
  form.render(view);
  TEST_ASSERT_EQUAL(0, view.selected);
  TEST_ASSERT_EQUAL(0, model.links);
  form.activate(11);
  TEST_ASSERT_EQUAL(0, model.links); // default Cancel returns list
  form.activate(12);
  form.turn(1, 13);
  form.activate(14);
  TEST_ASSERT_EQUAL(0, model.links);
  TEST_ASSERT_TRUE(model.learnedQueue.active(model.learnedToken));
  model.learnedQueue.run(model.learnedToken, 20);
  form.render(view);
  TEST_ASSERT_EQUAL(1, model.links);
  TEST_ASSERT_EQUAL(1, model.counterWrites);
  TEST_ASSERT_FALSE(model.received.active());
  TEST_ASSERT_NOT_NULL(strstr(view.title, "linked"));
  form.activate(21);
  TEST_ASSERT_TRUE(form.closed());
  TEST_ASSERT_EQUAL(0, memory.used());
}
static void pending_candidate_or_auth_changes_and_back_cannot_link() {
  for (unsigned reason = 0; reason < 3; ++reason) {
    Model model;
    JsonAllocator memory(4096, acquire, release);
    Advanced::Form form(model, memory);
    openLearn(form);
    form.turn(1, 5);
    form.activate(6);
    received(model, 7, 123, 20);
    form.activate(8);
    form.turn(1, 9);
    form.activate(10);
    const auto token = model.learnedToken;
    if (reason == 0)
      received(model, 11, 123, 21);
    else if (reason == 1)
      ++model.auth.generation;
    else
      form.back(11);
    model.learnedQueue.refresh(12);
    model.learnedQueue.run(token, 13);
    TEST_ASSERT_EQUAL(0, model.links);
    form.discard();
    TEST_ASSERT_EQUAL(0, memory.used());
  }
}
static void
applied_link_file_failure_retries_current_file_never_counter_or_link() {
  Model model;
  model.counterState = CounterDomainState::modelFailed;
  model.file = ModelCommitState::failed;
  JsonAllocator memory(4096, acquire, release);
  Advanced::Form form(model, memory);
  openLearn(form);
  form.turn(1, 5);
  form.activate(6);
  received(model, 7, 123, 20);
  form.activate(8);
  form.turn(1, 9);
  form.activate(10);
  model.learnedQueue.run(model.learnedToken, 20);
  FormView view;
  form.render(view);
  TEST_ASSERT_NOT_NULL(strstr(view.rows[0].label, "file"));
  TEST_ASSERT_EQUAL(1, view.selected);
  TEST_ASSERT_FALSE(model.received.active());
  form.turn(-1, 21);
  form.activate(22);
  TEST_ASSERT_EQUAL(0, model.retries);
  model.learnedQueue.run(model.learnedToken, 23);
  TEST_ASSERT_EQUAL(1, model.retries);
  TEST_ASSERT_EQUAL(1, model.links);
  TEST_ASSERT_EQUAL(1, model.counterWrites);
  form.discard();
}
static void final_expired_witness_closes_and_wipes_without_linking() {
  Model model;
  model.lateFreshness = LearnQueueReason::expired;
  JsonAllocator memory(4096, acquire, release);
  Advanced::Form form(model, memory);
  openLearn(form);
  form.turn(1, 5);
  form.activate(6);
  received(model, 7, 123, 20);
  form.activate(8);
  form.turn(1, 9);
  form.activate(10);
  model.learnedQueue.run(model.learnedToken, 20);
  TEST_ASSERT_TRUE(form.closed());
  TEST_ASSERT_EQUAL(Notice::expired, form.notice());
  TEST_ASSERT_EQUAL(0, model.links);
  TEST_ASSERT_FALSE(model.received.active());
  TEST_ASSERT_EQUAL(0, memory.used());
}
static void uncertain_nvs_has_no_retry_and_observation_or_queue_expiry_wipes() {
  for (unsigned scenario = 0; scenario < 3; scenario++) {
    Model model;
    model.counterState = CounterDomainState::nvsUncertain;
    JsonAllocator memory(4096, acquire, release);
    Advanced::Form form(model, memory);
    openLearn(form);
    form.turn(1, 5);
    form.activate(6);
    received(model, 7, 123, 20);
    if (scenario == 0) {
      form.activate(8);
      form.turn(1, 9);
      form.activate(10);
      model.learnedQueue.run(model.learnedToken, 20);
      FormView view;
      form.render(view);
      TEST_ASSERT_EQUAL_STRING("Counter uncertain", view.title);
      TEST_ASSERT_EQUAL(1, view.rowCount);
      TEST_ASSERT_EQUAL_STRING("Close", view.rows[0].label);
      TEST_ASSERT_EQUAL(1, model.links);
      TEST_ASSERT_EQUAL(1, model.counterWrites);
      TEST_ASSERT_FALSE(model.received.active());
      form.activate(21);
      TEST_ASSERT_EQUAL(0, model.retries);
    } else if (scenario == 1) {
      form.tick(30006);
      TEST_ASSERT_TRUE(form.closed());
      TEST_ASSERT_EQUAL(Notice::expired, form.notice());
    } else {
      form.activate(8);
      form.turn(1, 9);
      form.activate(10);
      model.learnedQueue.refresh(5010);
      TEST_ASSERT_TRUE(form.closed());
      TEST_ASSERT_EQUAL(Notice::expired, form.notice());
    }
    TEST_ASSERT_EQUAL(0, memory.used());
    TEST_ASSERT_FALSE(model.received.active());
  }
}
static void
failed_prewrite_stays_unapplied_and_file_uncertainty_preserves_prior_link() {
  for (unsigned scenario = 0; scenario < 2; scenario++) {
    Model model;
    model.counterState = scenario ? CounterDomainState::modelUncertain
                                  : CounterDomainState::nvsFailed;
    model.file = ModelCommitState::uncertain;
    JsonAllocator memory(4096, acquire, release);
    Advanced::Form form(model, memory);
    openLearn(form);
    form.turn(1, 5);
    form.activate(6);
    received(model, 7, 123, 20);
    form.activate(8);
    form.turn(1, 9);
    form.activate(10);
    model.learnedQueue.run(model.learnedToken, 20);
    FormView view;
    form.render(view);
    TEST_ASSERT_EQUAL(1, model.links);
    if (!scenario) {
      TEST_ASSERT_EQUAL(0, model.counterWrites);
      TEST_ASSERT_EQUAL_STRING("Link not applied", view.title);
      form.activate(21);
      TEST_ASSERT_FALSE(model.received.active());
    } else {
      TEST_ASSERT_EQUAL(1, model.counterWrites);
      TEST_ASSERT_NOT_NULL(strstr(view.title, "uncertain"));
      TEST_ASSERT_EQUAL(1, view.selected);
      model.fileRetryRefusal = RestartReason::counterUncertain;
      form.turn(-1, 21);
      form.activate(22);
      model.learnedQueue.run(model.learnedToken, 23);
      form.render(view);
      TEST_ASSERT_EQUAL_STRING("Link recovery blocked", view.title);
      TEST_ASSERT_EQUAL(1, view.rowCount);
      TEST_ASSERT_NOT_NULL(strstr(view.hint, "Earlier link kept"));
      TEST_ASSERT_EQUAL(1, model.counterWrites);
      TEST_ASSERT_EQUAL(1, model.links);
    }
    form.discard();
    TEST_ASSERT_EQUAL(0, memory.used());
  }
}
static void projection_discard_event_survives_cache_and_session_erasure_once() {
  Model model;
  JsonAllocator memory(4096, acquire, release);
  Advanced::Form form(model, memory);
  TEST_ASSERT_EQUAL(32, sizeof(form));
  openLearn(form);
  form.turn(1, 5);
  form.activate(6);
  received(model, 7, 123, 20);
  form.activate(8);
  TEST_ASSERT_FALSE(form.takePrivateProjectionDiscard());
  form.back(9);
  form.back(10);
  TEST_ASSERT_TRUE(form.takePrivateProjectionDiscard());
  TEST_ASSERT_FALSE(form.takePrivateProjectionDiscard());
  TEST_ASSERT_FALSE(model.received.active());
  form.activate(11);
  form.turn(1, 12);
  form.activate(13);
  received(model, 14, 123, 20);
  form.discard();
  TEST_ASSERT_EQUAL(0, memory.used());
  TEST_ASSERT_TRUE(form.takePrivateProjectionDiscard());
  TEST_ASSERT_FALSE(form.takePrivateProjectionDiscard());
}
static void sensor_zero_and_observed_format_are_honest_without_fake_rssi() {
  Model model;
  JsonAllocator memory(4096, acquire, release);
  Advanced::Form form(model, memory);
  openLearn(form);
  form.turn(1, 5);
  form.activate(6);
  ReceiveObservation event;
  event.observedAt = 7;
  event.address = 123;
  event.rollingCode = 0;
  event.command = static_cast<uint8_t>(somfy_commands::Sensor);
  event.protocol = 2;
  event.bits = 80;
  event.flags = ReceiveObservation::validFlag;
  TEST_ASSERT_TRUE(model.received.publish(event));
  form.activate(8);
  FormView view;
  form.render(view);
  TEST_ASSERT_NOT_NULL(strstr(view.rows[2].label, "Code 0"));
  TEST_ASSERT_NOT_NULL(strstr(view.rows[2].value, "RTV 80-bit Sensor"));
  TEST_ASSERT_NOT_NULL(strstr(view.rows[2].value, "RSSI unavailable"));
  TEST_ASSERT_NOT_NULL(strstr(view.hint, "not a filter"));
  form.discard();
  TEST_ASSERT_EQUAL(0, model.links);
}
static void openPrimary(Advanced::Form &form) {
  TEST_ASSERT_TRUE(form.open(Scope::devices,1));form.activate(2);
  for(unsigned i=0;i<3;i++)form.turn(1,3+i);
  FormView view;form.render(view);TEST_ASSERT_EQUAL(4,view.total);TEST_ASSERT_EQUAL(3,view.offset);TEST_ASSERT_EQUAL_STRING("Primary rolling code",view.rows[0].label);
  form.activate(6);form.render(view);TEST_ASSERT_EQUAL(Mode::counterReview,view.mode);TEST_ASSERT_EQUAL(0,view.selected);
}
static void editPrimary(Advanced::Form &form,bool change=true) {
  form.turn(1,7);form.activate(8);if(change)form.turn(-1,9);form.activate(10);form.turn(1,11);form.activate(12);
}
static void primary_correction_requires_explicit_draft_apply_confirm_and_file_only_recovery() {
  Model model;model.file=ModelCommitState::failed;model.counterState=CounterDomainState::modelFailed;
  JsonAllocator memory(4096,acquire,release);Advanced::Form form(model,memory);openPrimary(form);
  editPrimary(form);FormView view;form.render(view);TEST_ASSERT_EQUAL_STRING("Set code: 999",view.rows[1].label);TEST_ASSERT_EQUAL(0,view.selected);
  TEST_ASSERT_EQUAL(0,model.corrections);form.turn(1,13);form.activate(14);TEST_ASSERT_EQUAL(0,model.corrections);
  model.learnedQueue.run(model.learnedToken,40);form.render(view);
  TEST_ASSERT_EQUAL(1,model.corrections);TEST_ASSERT_EQUAL(1,model.counterWrites);TEST_ASSERT_EQUAL(1,view.selected);TEST_ASSERT_EQUAL_STRING("Retry model file save",view.rows[0].label);
  TEST_ASSERT_TRUE(form.takePrivateProjectionDiscard());TEST_ASSERT_FALSE(form.takePrivateProjectionDiscard());
  form.turn(-1,41);form.activate(42);model.learnedQueue.run(model.learnedToken,43);form.render(view);
  TEST_ASSERT_EQUAL_STRING("Counter saved",view.title);TEST_ASSERT_EQUAL(1,model.retries);TEST_ASSERT_EQUAL(1,model.corrections);TEST_ASSERT_EQUAL(1,model.counterWrites);
  TEST_ASSERT_LESS_OR_EQUAL(2304,peak);form.discard();TEST_ASSERT_EQUAL(0,memory.used());
}
static void primary_noop_requires_exact_verified_reconciliation_and_fresh_counter_guard() {
  for(unsigned difference=0;difference<4;++difference) {
    Model model;if(difference==1)model.storedPresent=false;if(difference==2)model.storedPrimary=999;if(difference==3)model.aliasCode=999;
    JsonAllocator memory(4096,acquire,release);Advanced::Form form(model,memory);openPrimary(form);editPrimary(form,false);
    if(!difference){TEST_ASSERT_TRUE(form.closed());TEST_ASSERT_EQUAL(Notice::unchanged,form.notice());}
    else {TEST_ASSERT_FALSE(form.closed());FormView view;form.render(view);TEST_ASSERT_EQUAL_STRING("Set code: 1000",view.rows[1].label);}
    TEST_ASSERT_EQUAL(0,model.corrections);form.discard();
  }
  Model model;JsonAllocator memory(4096,acquire,release);Advanced::Form form(model,memory);openPrimary(form);editPrimary(form);
  form.turn(1,13);form.activate(14);++model.aliasCode;model.learnedQueue.run(model.learnedToken,40);
  TEST_ASSERT_TRUE(form.closed());TEST_ASSERT_EQUAL(Notice::changed,form.notice());TEST_ASSERT_EQUAL(0,model.corrections);TEST_ASSERT_EQUAL(0,memory.used());
}
static void primary_queue_back_age_auth_and_unknown_storage_never_replay() {
  for(unsigned exit=0;exit<4;++exit) {
    Model model;JsonAllocator memory(4096,acquire,release);Advanced::Form form(model,memory);openPrimary(form);editPrimary(form);form.turn(1,13);form.activate(14);
    TEST_ASSERT_TRUE(model.learnedQueue.active());
    if(exit==0)form.back(15);
    if(exit==1)model.learnedQueue.refresh(5014);
    if(exit==2){model.auth.allowed=false;model.learnedQueue.refresh(15);}
    if(exit==3){++model.history;model.learnedQueue.refresh(15);}
    TEST_ASSERT_TRUE(form.closed());TEST_ASSERT_FALSE(model.learnedQueue.active());TEST_ASSERT_EQUAL(0,memory.used());
    model.learnedQueue.run(model.learnedToken,6000);TEST_ASSERT_EQUAL(0,model.corrections);TEST_ASSERT_EQUAL(0,model.counterWrites);
  }
  Model model;model.counterState=CounterDomainState::nvsUncertain;JsonAllocator memory(4096,acquire,release);Advanced::Form form(model,memory);
  openPrimary(form);editPrimary(form);form.turn(1,13);form.activate(14);model.learnedQueue.run(model.learnedToken,40);
  FormView view;form.render(view);TEST_ASSERT_EQUAL_STRING("Counter uncertain",view.title);TEST_ASSERT_EQUAL(1,view.rowCount);TEST_ASSERT_EQUAL_STRING("Close",view.rows[0].label);
  form.activate(41);TEST_ASSERT_TRUE(form.closed());TEST_ASSERT_EQUAL(1,model.corrections);TEST_ASSERT_EQUAL(1,model.counterWrites);TEST_ASSERT_EQUAL(0,model.retries);
}
static void primary_editor_expiry_and_old_ticket_cannot_execute_reopened_session() {
  Model model;JsonAllocator memory(4096,acquire,release);Advanced::Form form(model,memory);openPrimary(form);form.turn(1,7);form.activate(8);form.tick(30008);
  TEST_ASSERT_TRUE(form.closed());TEST_ASSERT_EQUAL(Notice::expired,form.notice());TEST_ASSERT_EQUAL(0,memory.used());
  openPrimary(form);editPrimary(form);form.turn(1,13);form.activate(14);const auto old=model.learnedToken;form.discard();
  openPrimary(form);editPrimary(form);form.turn(1,13);form.activate(14);const auto replacement=model.learnedToken;
  TEST_ASSERT_NOT_EQUAL(old,replacement);model.learnedQueue.run(old,40);TEST_ASSERT_EQUAL(0,model.corrections);TEST_ASSERT_TRUE(model.learnedQueue.active(replacement));
  model.learnedQueue.run(replacement,40);TEST_ASSERT_EQUAL(1,model.corrections);form.discard();TEST_ASSERT_EQUAL(0,memory.used());
}
static void repeater_start_review_add_and_file_only_retry_are_explicit(){
  Model model;model.file=ModelCommitState::failed;JsonAllocator memory(4096,acquire,release);Advanced::Form form(model,memory);FormView view;
  TEST_ASSERT_TRUE(form.open(Scope::repeaters,0));form.turn(1,1);form.activate(2);form.render(view);
  TEST_ASSERT_EQUAL_STRING("Begin observation",view.rows[1].label);TEST_ASSERT_EQUAL(0,view.selected);TEST_ASSERT_FALSE(model.received.active());
  form.turn(1,3);form.activate(4);TEST_ASSERT_TRUE(model.received.active());received(model,5,123456,200);
  TEST_ASSERT_EQUAL(0,model.repeaterAdds);form.activate(6);form.render(view);TEST_ASSERT_EQUAL(0,view.selected);
  TEST_ASSERT_EQUAL_STRING("Add repeater",view.rows[1].label);TEST_ASSERT_NOT_NULL(strstr(view.hint,"retransmit"));
  form.turn(1,7);form.activate(8);TEST_ASSERT_EQUAL(0,model.repeaterAdds);model.learnedQueue.run(model.learnedToken,20);
  TEST_ASSERT_EQUAL(1,model.repeaterAdds);TEST_ASSERT_EQUAL(123456,model.repeater);TEST_ASSERT_EQUAL(0,model.counterWrites);TEST_ASSERT_EQUAL(0,model.links);
  form.render(view);TEST_ASSERT_EQUAL(Mode::result,view.mode);TEST_ASSERT_EQUAL(1,view.selected);TEST_ASSERT_FALSE(model.received.active());
  form.turn(-1,21);form.activate(22);TEST_ASSERT_EQUAL(1,model.retries);TEST_ASSERT_EQUAL(1,model.repeaterAdds);
  TEST_ASSERT_EQUAL(0,model.counterWrites);TEST_ASSERT_TRUE(form.closed());TEST_ASSERT_EQUAL(0,memory.used());
}
static void repeater_full_start_cancel_and_back_preserve_list_without_observation(){
  for(bool full:{false,true}){
    Model model;if(full)model.repeaterTotal=7;JsonAllocator memory(4096,acquire,release);Advanced::Form form(model,memory);FormView view;
    TEST_ASSERT_TRUE(form.open(Scope::repeaters,0));form.turn(-1,1);form.activate(2);
    if(full){TEST_ASSERT_EQUAL(Notice::capacity,form.notice());form.render(view);TEST_ASSERT_EQUAL_STRING("Full",view.rows[view.rowCount-1].value);}
    else {form.activate(3);form.render(view);TEST_ASSERT_EQUAL_STRING("Repeaters",view.title);TEST_ASSERT_EQUAL_STRING("Learn/add repeater",view.rows[1].label);}
    TEST_ASSERT_FALSE(model.received.active());TEST_ASSERT_EQUAL(0,model.repeaterAdds);TEST_ASSERT_EQUAL(1234,model.repeater);
    form.back(4);TEST_ASSERT_TRUE(form.closed());TEST_ASSERT_EQUAL(0,memory.used());
  }
}
static void repeater_queued_cancel_age_auth_and_list_changes_never_add(){
  for(unsigned change=0;change<5;++change){
    Model model;JsonAllocator memory(4096,acquire,release);Advanced::Form form(model,memory);
    TEST_ASSERT_TRUE(form.open(Scope::repeaters,0));form.turn(1,1);form.activate(2);form.turn(1,3);form.activate(4);
    received(model,5,123456,200);form.activate(6);form.turn(1,7);form.activate(8);
    if(change==0)form.back(9);if(change==1)model.auth.allowed=false;if(change==2)++model.auth.generation;if(change==3)++model.repeater;
    model.learnedQueue.run(model.learnedToken,change==4?5008:20);
    TEST_ASSERT_EQUAL(0,model.repeaterAdds);TEST_ASSERT_EQUAL(0,model.counterWrites);TEST_ASSERT_EQUAL(0,model.links);
    form.discard();TEST_ASSERT_FALSE(model.received.active());TEST_ASSERT_EQUAL(0,memory.used());
  }
}
int main() {
  UNITY_BEGIN();
  RUN_TEST(repeater_full_start_cancel_and_back_preserve_list_without_observation);
  RUN_TEST(repeater_queued_cancel_age_auth_and_list_changes_never_add);
  RUN_TEST(repeater_start_review_add_and_file_only_retry_are_explicit);
  RUN_TEST(primary_queue_back_age_auth_and_unknown_storage_never_replay);
  RUN_TEST(primary_editor_expiry_and_old_ticket_cannot_execute_reopened_session);
  RUN_TEST(primary_correction_requires_explicit_draft_apply_confirm_and_file_only_recovery);
  RUN_TEST(primary_noop_requires_exact_verified_reconciliation_and_fresh_counter_guard);
  RUN_TEST(sensor_zero_and_observed_format_are_honest_without_fake_rssi);
  RUN_TEST(projection_discard_event_survives_cache_and_session_erasure_once);
  RUN_TEST(uncertain_nvs_has_no_retry_and_observation_or_queue_expiry_wipes);
  RUN_TEST(
      failed_prewrite_stays_unapplied_and_file_uncertainty_preserves_prior_link);
  RUN_TEST(final_expired_witness_closes_and_wipes_without_linking);
  RUN_TEST(learning_requires_explicit_start_frozen_review_and_queue_execution);
  RUN_TEST(pending_candidate_or_auth_changes_and_back_cannot_link);
  RUN_TEST(
      applied_link_file_failure_retries_current_file_never_counter_or_link);
  RUN_TEST(cancel_is_default_and_rotation_never_applies);
  RUN_TEST(applied_failure_retries_file_only);
  RUN_TEST(context_and_authorization_changes_close_and_free);
  RUN_TEST(allocation_timeout_and_repeater_context);
  RUN_TEST(full_paging_repeater_change_and_admission_failure);
  RUN_TEST(file_retry_busy_keeps_commit_only_result);
  RUN_TEST(stable_label_identity_is_required);
  RUN_TEST(confirmation_names_exact_association_and_configuration_effect);
  RUN_TEST(linked_remote_list_keeps_readonly_counter_visible);
  RUN_TEST(busy_mutation_has_visible_refusal_and_explicit_review_again);
  return UNITY_END();
}
