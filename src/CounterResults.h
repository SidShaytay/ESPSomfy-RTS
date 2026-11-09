#pragma once
#include "ExplicitCounter.h"
#include "ShadeRequests.h"
#include "ModelEditor.h"
// Fixed typed outcomes shared by browser/socket callers; no model/storage I/O.
const char *counterDomainDescription(CounterDomainState state);
shade_request_result_t counterRequestResult(const CounterDomainResult &result);
edit_result_t counterLinkResult(uint8_t shadeId,const CounterDomainResult &result);
