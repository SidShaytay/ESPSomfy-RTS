#pragma once
#include <ArduinoJson.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <new>

namespace LocalUi {
// Loop-owned storage for temporary settings drafts. Inject PSRAM allocation in
// firmware and a checked heap in tests. acquire must return max_align_t-aligned
// storage, paired with release. Documents must die before this object.
class JsonAllocator : public ArduinoJson::Allocator {
  struct alignas(max_align_t) Block { size_t capacity, length; };
  const size_t budget;
  size_t occupied=0;
  void *(*acquire)(size_t);
  void (*release)(void *);
  static void erase(void *p,size_t size) {
    auto *bytes=static_cast<volatile unsigned char *>(p);
    while(size--) *bytes++=0;
  }
public:
  JsonAllocator(size_t limit,void *(*a)(size_t),void (*r)(void *))
    :budget(limit),acquire(a),release(r) {}
  JsonAllocator(const JsonAllocator&)=delete;
  JsonAllocator &operator=(const JsonAllocator&)=delete;
  size_t used() const { return occupied; }
  void *allocate(size_t size) override {
    if(!size || !acquire || !release || size>SIZE_MAX-sizeof(Block)) return nullptr;
    const size_t total=sizeof(Block)+size;
    if(total>budget-occupied) return nullptr;
    void *raw=acquire(total);
    if(!raw) return nullptr;
    auto *block=new(raw) Block{size,size};
    occupied+=total;
    return block+1;
  }
  void deallocate(void *p) override {
    if(!p) return;
    auto *block=static_cast<Block *>(p)-1;
    const size_t total=sizeof(Block)+block->capacity;
    occupied-=total;
    erase(block,total);
    release(block);
  }
  void *reallocate(void *p,size_t size) override {
    if(!p) return allocate(size);
    if(!size) { deallocate(p); return nullptr; }
    auto *block=static_cast<Block *>(p)-1;
    if(size<=block->capacity) {
      if(size<block->length) erase(static_cast<unsigned char *>(p)+size,block->length-size);
      block->length=size;
      return p;
    }
    // Both blocks count against the budget during a growth copy. Failure
    // leaves the old allocation untouched; never bypass the cap with realloc.
    void *next=allocate(size);
    if(!next) return nullptr;
    memcpy(next,p,block->length);
    deallocate(p);
    return next;
  }
};
} // namespace LocalUi
