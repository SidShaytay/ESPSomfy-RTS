#pragma once
#include "LoopQueue.h"
#include <deque>
#include <assert.h>
#include <string.h>
using QueueHandle_t=void *;
inline uint64_t lastLocalToken=0;
struct FakeRtosQueue {std::deque<loop_item_t> items;unsigned capacity;};
inline bool queueAllocationOK=true;
inline void *xQueueCreate(unsigned capacity,unsigned bytes) {assert(bytes==sizeof(loop_item_t));if(!queueAllocationOK)return nullptr;return new FakeRtosQueue{{},capacity};}
inline int xQueueSend(QueueHandle_t q,const void *item,unsigned wait) {
  assert(!wait);auto &queue=*static_cast<FakeRtosQueue *>(q);
  if(queue.items.size()==queue.capacity)return 0;
  const auto &value=*static_cast<const loop_item_t *>(item);if(value.type==loop_item_type_t::local_control)lastLocalToken=value.localToken;
  queue.items.push_back(*static_cast<const loop_item_t *>(item));return 1;
}
inline int xQueueSendToFront(QueueHandle_t q,const void *item,unsigned wait) {
  assert(!wait);auto &queue=*static_cast<FakeRtosQueue *>(q);
  if(queue.items.size()==queue.capacity)return 0;
  const auto &value=*static_cast<const loop_item_t *>(item);if(value.type==loop_item_type_t::local_control)lastLocalToken=value.localToken;
  queue.items.push_front(*static_cast<const loop_item_t *>(item));return 1;
}
inline int xQueuePeek(QueueHandle_t q,void *out,unsigned wait) {
  assert(!wait);auto &items=static_cast<FakeRtosQueue *>(q)->items;if(items.empty())return 0;
  *static_cast<loop_item_t *>(out)=items.front();return 1;
}
inline int xQueueReceive(QueueHandle_t q,void *out,unsigned wait) {
  if(!xQueuePeek(q,out,wait))return 0;static_cast<FakeRtosQueue *>(q)->items.pop_front();return 1;
}

inline unsigned uxQueueMessagesWaiting(QueueHandle_t q) {return static_cast<FakeRtosQueue *>(q)->items.size();}
