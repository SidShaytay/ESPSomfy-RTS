#pragma once
#include <stddef.h>
#include <stdint.h>
// Only loading/restoring a validated immutable model. This does not change
// automatic TX/RX counter advancement or grant updater/restore ownership.
enum class CounterLoadState : uint8_t { collecting, ready, failed, uncertain };
enum class CounterLoadStage : uint8_t { none, memory, capacity, address, state, readOpen, readValue, writeOpen, writeValue, commit, verifyOpen, verifyValue, interrupted };
struct CounterLoadResult {
  CounterLoadState state=CounterLoadState::collecting;
  CounterLoadStage stage=CounterLoadStage::none;
  bool writeAttempted=false;
  bool ready()const{return state==CounterLoadState::ready;}
};
struct CounterLoadControl {
  const void *context=nullptr;
  bool (*continuing)(const void *)=nullptr;
  void (*cooperate)(const void *)=nullptr;
};
class CounterLoad {
public:
  static constexpr size_t maximumEntries=272;
  CounterLoad();
  CounterLoad(void *(*allocate)(size_t),void (*release)(void *,size_t));
  CounterLoad(const CounterLoad &)=delete;
  CounterLoad &operator=(const CounterLoad &)=delete;
  ~CounterLoad(){clear();}
  bool include(uint32_t address,uint16_t fileFloor=0);
  CounterLoadResult resolve(CounterLoadControl control={});
  bool lookup(uint32_t address,uint16_t &value)const;
  CounterLoadResult result()const{return outcome;}
  void clear();
private:
  struct Entry{uint32_t address;uint16_t maximum,stored;};
  Entry *entries=nullptr;
  size_t count=0;
  void *(*allocateBytes)(size_t);
  void (*releaseBytes)(void *,size_t);
  CounterLoadResult outcome;
  CounterLoadResult fail(CounterLoadStage stage,bool written=false);
};
