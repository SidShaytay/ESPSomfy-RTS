#pragma once
// Production firmware always includes the complete local interface on each
// supported physical LILYGO board. Host subsystem harnesses may test components.
#include "BoardProfile.h"
#if defined(ESPSOMFY_LILYGO_T_EMBED_CC1101_PLUS) || defined(ESPSOMFY_LILYGO_T_EMBED_CC1101)
#if !defined(ESPSOMFY_STATUS_DISPLAY) || !defined(ESPSOMFY_LOCAL_UI) || !defined(ESPSOMFY_LOCAL_COMMANDS)
#error "LILYGO firmware requires display, local UI and local commands"
#endif
#if ESPSOMFY_STATUS_DISPLAY != 1 || ESPSOMFY_LOCAL_UI != 1 || ESPSOMFY_LOCAL_COMMANDS != 1
#error "LILYGO display, UI and command macros must have canonical value 1"
#endif
#if defined(ESPSOMFY_LILYGO_T_EMBED_CC1101_PLUS) && ESPSOMFY_LILYGO_T_EMBED_CC1101_PLUS != 1
#error "LILYGO Plus board macro must have canonical value 1"
#endif
#if defined(ESPSOMFY_LILYGO_T_EMBED_CC1101) && ESPSOMFY_LILYGO_T_EMBED_CC1101 != 1
#error "LILYGO standard board macro must have canonical value 1"
#endif
#elif defined(ESPSOMFY_STATUS_DISPLAY) || defined(ESPSOMFY_LOCAL_UI) || defined(ESPSOMFY_LOCAL_COMMANDS)
#error "Local display features require an exact supported LILYGO board"
#endif
