#pragma once
namespace StatusPanel {
void begin();
void loop(bool running = true);
// Successful initialization/presentation, not physical panel detection.
bool healthy();
}
