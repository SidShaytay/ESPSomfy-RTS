#include "CounterResults.h"
const char *counterDomainDescription(CounterDomainState state) {
  switch(state) {
  case CounterDomainState::busy:return "Actions, movement, update or restart work is pending; counter/link unchanged.";
  case CounterDomainState::quarantined:return "An earlier counter operation needs resolution; no new counter/link was applied. Restart remains blocked.";
  case CounterDomainState::nvsFailed:return "Counter storage could not be read/opened before writing; counter/link unchanged.";
  case CounterDomainState::nvsUncertain:return "Counter write may have persisted; running counter/link unchanged. Identity and restart blocked; no counter replay.";
  case CounterDomainState::modelFailed:return "Counter/link applied; model file failed. Retry Commit only; do not repeat the counter operation. Restart blocked.";
  case CounterDomainState::modelUncertain:return "Counter/link applied; model file uncertain. Retry Commit only; do not repeat the counter operation. Restart blocked.";
  case CounterDomainState::stale:return "Received code is older than retained storage or live aliases. Receive a fresh code; nothing linked.";
  case CounterDomainState::full:return "This shade already has the most linked remotes it can.";
  case CounterDomainState::missing:return "Shade Id not found.";
  case CounterDomainState::invalid:return "Invalid counter identity; nothing changed.";
  case CounterDomainState::saved:return "Counter and current model file verified.";
  }
  return "Counter outcome unavailable; no success was reported.";
}
shade_request_result_t counterRequestResult(const CounterDomainResult &result) {
  switch(result.state) {
  case CounterDomainState::saved:return result.applied && result.file.saved() ? shade_request_result_t::done : shade_request_result_t::counter_failed;
  case CounterDomainState::busy:return shade_request_result_t::counter_busy;
  case CounterDomainState::quarantined:return shade_request_result_t::counter_blocked;
  case CounterDomainState::nvsUncertain:return shade_request_result_t::counter_uncertain;
  case CounterDomainState::modelFailed:return result.applied ? shade_request_result_t::counter_file_failed : shade_request_result_t::counter_failed;
  case CounterDomainState::modelUncertain:return result.applied ? shade_request_result_t::counter_file_uncertain : shade_request_result_t::counter_failed;
  case CounterDomainState::missing:return shade_request_result_t::not_found;
  case CounterDomainState::invalid:return shade_request_result_t::counter_invalid;
  case CounterDomainState::stale:return shade_request_result_t::counter_stale;
  default:return shade_request_result_t::counter_failed;
  }
}
edit_result_t counterLinkResult(uint8_t shadeId,const CounterDomainResult &result) {
  edit_result_t out;
  if(result.applied)out=edit_result_t::completed(shadeId,result.file);
  else if(result.state==CounterDomainState::missing)out=edit_result_t::missing(counterDomainDescription(result.state));
  else {
    out=edit_result_t::error(counterDomainDescription(result.state));
    if(result.state==CounterDomainState::busy || result.state==CounterDomainState::quarantined)out.failure=edit_failure_t::busy;
    if(result.state==CounterDomainState::nvsFailed || result.state==CounterDomainState::nvsUncertain || result.state==CounterDomainState::saved)out.failure=edit_failure_t::internal;
  }
  out.hasCounterResult=true;
  out.counterState=result.state;
  return out;
}
