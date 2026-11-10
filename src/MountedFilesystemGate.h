#pragma once
#include <atomic>
#include <cstdint>
#include <utility>
// Mechanical mount-I/O exclusion, not a transfer owner. The existing updater
// owner alone seals/resumes it. A lease outlives every underlying open handle.
class MountedFilesystemGate {
  static constexpr uint32_t sealedBit=uint32_t(1)<<31,countMask=sealedBit-1;
  std::atomic<uint32_t> state{0};
  void leave(){state.fetch_sub(1,std::memory_order_release);}
public:
  class Lease {
    friend class MountedFilesystemGate;
    MountedFilesystemGate *gate=nullptr;
    explicit Lease(MountedFilesystemGate *value):gate(value){}
  public:
    Lease()=default;
    ~Lease(){close();}
    Lease(const Lease&)=delete;Lease& operator=(const Lease&)=delete;
    Lease(Lease &&other)noexcept:gate(std::exchange(other.gate,nullptr)){}
    Lease& operator=(Lease &&other)noexcept {
      if(this!=&other){close();gate=std::exchange(other.gate,nullptr);}return *this;
    }
    explicit operator bool()const{return gate!=nullptr;}
    void close(){if(auto *value=std::exchange(gate,nullptr))value->leave();}
  };
  Lease enter(){
    auto value=state.load(std::memory_order_acquire);
    while(!(value&sealedBit) && (value&countMask)!=countMask){
      if(state.compare_exchange_weak(value,value+1,std::memory_order_acq_rel,std::memory_order_acquire))return Lease(this);
    }
    return {};
  }
  bool seal(){return !(state.fetch_or(sealedBit,std::memory_order_acq_rel)&sealedBit);}
  bool drained()const{return state.load(std::memory_order_acquire)==sealedBit;}
  // Caller must prove LittleFS remains mounted (cancelled preparation) or has
  // been remounted and current-state recovery completed. No mount I/O here.
  void resume(){state.fetch_and(countMask,std::memory_order_release);}
};

inline MountedFilesystemGate &mountedFilesystemGate(){static MountedFilesystemGate gate;return gate;}
