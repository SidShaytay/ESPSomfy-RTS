#include "RebootDeadline.h"
#include "RebootRequest.h"
#include <thread>
#include <vector>
#include <cassert>
#include <cstdint>
int main() {
  assert(!rebootDeadlineReached(100, 600));
  assert(rebootDeadlineReached(600, 600));
  assert(rebootDeadlineReached(601, 600));
  const uint32_t start = UINT32_MAX - 100;
  const uint32_t deadline = start + 500;
  assert(!rebootDeadlineReached(start, deadline));
  assert(!rebootDeadlineReached(398, deadline));
  assert(rebootDeadlineReached(399, deadline));
  assert(rebootDeadlineReached(500, deadline));
  assert(earlierRebootDeadline(start, 180000, true, deadline) == deadline);
  assert(earlierRebootDeadline(start, 500, false, 0) == deadline);
  assert(earlierRebootDeadline(1100, 500, true, 1000) == 1000);
  RebootRequest mailbox;
  uint32_t delay = 0;
  assert(!mailbox.requested() && !mailbox.take(delay));
  mailbox.request(0, 180000);
  mailbox.request(0, 500);
  mailbox.request(100, 1000);
  assert(mailbox.requested() && mailbox.take(delay) && delay == 500);
  assert(mailbox.requested() && !mailbox.take(delay));
  mailbox.request(0, 0);
  assert(mailbox.take(delay) && delay == 0);
  RebootRequest racing;
  std::vector<std::thread> producers;
  for(unsigned i = 0; i < 20; ++i) producers.emplace_back([&,i] { racing.request(0, 500 + i); });
  for(auto &producer : producers) producer.join();
  assert(racing.requested() && racing.take(delay) && delay == 500);
  RebootRequest wrap;
  wrap.request(start, 500);
  wrap.request(start + 1, 500); // later repeat cannot move the first deadline
  assert(wrap.take(delay) && delay == deadline);
  RebootRequest sentinel;
  sentinel.request(UINT32_MAX - 500, 500);
  assert(sentinel.take(delay) && delay == UINT32_MAX - 1);
}
