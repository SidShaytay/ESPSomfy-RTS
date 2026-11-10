#pragma once
#include "LocalControlQueue.h"
namespace LocalUi {
// Loop-owned admission shared by local control and settings operations. The
// installer separately coordinates web/HA/model quiescence before committing.
void setLocalControlsFrozen(bool);
bool localControlsFrozen();
}
