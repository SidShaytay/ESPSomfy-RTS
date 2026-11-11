#pragma once
// Opt-in measurement only. No RTOS dependencies or code in ordinary builds.
namespace StackAudit {
#ifdef ESPSOMFY_STACK_AUDIT
void observeCurrent();
// Call from loop only, after the scheduler and default network stack are ready.
void beginSdk();
void pollSdk();
#else
inline void observeCurrent() {}
inline void beginSdk() {}
inline void pollSdk() {}
#endif
struct Scope { ~Scope() { observeCurrent(); } };
}
