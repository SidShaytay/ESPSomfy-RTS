#pragma once
#include "RestartTypes.h"
#include <stdint.h>
#if defined(ESP_PLATFORM)
#include <freertos/FreeRTOS.h>
#else
#include <mutex>
#endif

namespace LegacyRestartDetail {
#if defined(ESP_PLATFORM)
inline portMUX_TYPE &lock() { static portMUX_TYPE value=portMUX_INITIALIZER_UNLOCKED;return value; }
struct Guard { Guard(){portENTER_CRITICAL(&lock());} ~Guard(){portEXIT_CRITICAL(&lock());} };
#else
inline std::mutex &lock() { static std::mutex value;return value; }
struct Guard { std::lock_guard<std::mutex> held;Guard():held(lock()){} };
#endif
}
// Copy the complete source/deadline/armed tuple under the same publication lock.
// This lock is never held across service acquisition or any I/O.
template<class Record> Record snapshotLegacyRestart(const Record &record) {
  LegacyRestartDetail::Guard guard;return record;
}
// Publication metadata only. Actual shutdown still acquires the shared owner
// and checks the exact source; these helpers do not certify durability.
template<class Record>
RestartResult publishGenericLegacyRestart(Record &record,uint32_t deadline) {
  LegacyRestartDetail::Guard guard;
  if(record.reboot) return {false,RestartReason::pending};
  record.externalOwnerGeneration=0;record.externalOwnerKind=0;
  record.rebootTime=deadline;record.reboot=true;return {true,RestartReason::none};
}
template<class Record>
RestartResult publishExternalLegacyRestart(Record &record,uint32_t deadline,uint64_t generation,uint8_t kind) {
  if(!generation) return {false,RestartReason::invalidRequest};
  LegacyRestartDetail::Guard guard;
  if(record.reboot) return record.externalOwnerGeneration==generation && record.externalOwnerKind==kind
      ? RestartResult{true,RestartReason::none} : RestartResult{false,RestartReason::pending};
  record.externalOwnerGeneration=generation;record.externalOwnerKind=kind;
  record.rebootTime=deadline;record.reboot=true;return {true,RestartReason::none};
}
template<class Record> bool clearLegacyRestart(Record &record,const Record &expected) {
  LegacyRestartDetail::Guard guard;
  if(record.reboot!=expected.reboot || record.rebootTime!=expected.rebootTime ||
     record.externalOwnerGeneration!=expected.externalOwnerGeneration ||
     record.externalOwnerKind!=expected.externalOwnerKind) return false;
  record.reboot=false;record.externalOwnerGeneration=0;record.externalOwnerKind=0;return true;
}
