#pragma once
#include "RestartTypes.h"
#include <stdint.h>
// Publication metadata only. Actual shutdown still acquires the shared owner
// and checks the exact source; these helpers do not certify durability.
template<class Record>
RestartResult publishGenericLegacyRestart(Record &record,uint32_t deadline) {
  if(record.reboot) return {false,RestartReason::pending};
  record.externalOwnerGeneration=0;record.externalOwnerKind=0;
  record.rebootTime=deadline;record.reboot=true;return {true,RestartReason::none};
}
template<class Record>
RestartResult publishExternalLegacyRestart(Record &record,uint32_t deadline,uint64_t generation,uint8_t kind) {
  if(!generation) return {false,RestartReason::invalidRequest};
  if(record.reboot) return record.externalOwnerGeneration==generation && record.externalOwnerKind==kind
      ? RestartResult{true,RestartReason::none} : RestartResult{false,RestartReason::pending};
  record.externalOwnerGeneration=generation;record.externalOwnerKind=kind;
  record.rebootTime=deadline;record.reboot=true;return {true,RestartReason::none};
}
template<class Record> void clearLegacyRestart(Record &record) {
  record.reboot=false;record.externalOwnerGeneration=0;record.externalOwnerKind=0;
}
