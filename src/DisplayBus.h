#pragma once
// Executes a short SPI operation under the same mutex as CC1101 TX/config.
// False means busy; caller must retry later. Callback must not wait or submit commands.
bool tryDisplayBus(void (*transfer)(void *), void *context);
