#pragma once
#include <stdint.h>
namespace AuditConsole {
#if defined(ESPSOMFY_AUDIT_CONTROL) && !defined(ESPSOMFY_STACK_AUDIT)
#error "Audit controls require stack audit instrumentation"
#endif
#ifdef ESPSOMFY_AUDIT_CONTROL
struct UiInput {int8_t turn=0;bool pressed=false,back=false;};
void loop();
bool uiInput(UiInput &);
void uiState(uint8_t screen,uint8_t selected,uint8_t mode,uint8_t formSelected,bool visible);
#else
inline void loop() {}
#endif
}
