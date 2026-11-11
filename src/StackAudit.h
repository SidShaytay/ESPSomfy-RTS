#pragma once
// Opt-in measurement only. No RTOS dependencies or code in ordinary builds.
namespace StackAudit {
#ifdef ESPSOMFY_STACK_AUDIT
void observeCurrent();
#else
inline void observeCurrent() {}
#endif
struct Scope { ~Scope() { observeCurrent(); } };
}
