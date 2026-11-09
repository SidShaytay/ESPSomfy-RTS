#pragma once
#include <stdint.h>
enum class settings_section_t : uint8_t;

// Loop-owned history, independent of setting values. A successful no-op save
// still invalidates older forms/sessions; ordinary motion changes no counter.
class SettingsRevisions {
  static constexpr uint8_t count=5;
  uint64_t values[count] = {1,1,1,1,1};
public:
  uint64_t get(settings_section_t section) const {
    const auto index=static_cast<uint8_t>(section);
    return index<count ? values[index] : 0;
  }
  void completed(settings_section_t section) {
    const auto index=static_cast<uint8_t>(section);
    if(index<count) ++values[index];
  }
  void invalidateAll() { for(auto &value:values) ++value; }
};
static_assert(sizeof(SettingsRevisions)==5*sizeof(uint64_t),"Keep settings history bounded");

// Writers are loop-owned; synchronized reads may bind asynchronous work.
// A history value alone is not authentication. No credentials are cached here.
uint64_t deviceSettingsRevision(settings_section_t section);
uint64_t securityConfigurationRevision();
// Restore integration calls this after validation, before mutating live state.
void invalidateDeviceSettingsRevisions();
void noteDeviceSettingsCompleted(settings_section_t section);
