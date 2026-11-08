#pragma once
#include <stdint.h>

// NVS updates use the existing per-key format. They are not transactions:
// a failed/partial result may leave durable values for the next retry/reboot.
enum class settings_save_state_t : uint8_t { saved, failed, partial };
struct settings_save_result_t {
  settings_save_state_t state = settings_save_state_t::saved;
  const char *failedKey = nullptr; // schema key only; never a stored value
  bool saved() const { return state == settings_save_state_t::saved; }
};
