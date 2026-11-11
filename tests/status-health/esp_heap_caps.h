#pragma once
#include <assert.h>
#include <stdint.h>
constexpr unsigned MALLOC_CAP_INTERNAL=1,MALLOC_CAP_SPIRAM=2,MALLOC_CAP_8BIT=4;
namespace FakeHeap {
inline uint32_t internalFree=65536,minimum=32768,largest=16384,psramFree=6u*1024*1024,psramTotal=8u*1024*1024,internalTotal=128u*1024;
inline unsigned psramReads=0;
}
inline uint32_t heap_caps_get_free_size(unsigned caps) {
  if(caps==(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT)) return FakeHeap::internalFree;
  assert(caps==(MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));++FakeHeap::psramReads;return FakeHeap::psramFree;
}
inline uint32_t heap_caps_get_minimum_free_size(unsigned caps) { assert(caps==(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT));return FakeHeap::minimum; }
inline uint32_t heap_caps_get_largest_free_block(unsigned caps) { assert(caps==(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT));return FakeHeap::largest; }
inline uint32_t heap_caps_get_total_size(unsigned caps) {
  if(caps==(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT))return FakeHeap::internalTotal;
  assert(caps==(MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));return FakeHeap::psramTotal;
}
