#pragma once
#include <algorithm>
#include <cstddef>
#include <cstdint>

namespace GitUpdateTransfer {
struct Bounds {
  size_t capacity;
  int64_t contentLength;
  bool chunked;
  bool metadata;
};
enum class Result { Complete, InvalidBounds, Overflow, Truncated, Stopped };
inline bool valid(const Bounds &b) {
  if(!b.capacity || b.contentLength<0) return false;
  if(b.chunked) return b.metadata && uint64_t(b.contentLength)<=b.capacity;
  return b.contentLength>0 && uint64_t(b.contentLength)<=b.capacity;
}
// The same bounded body reader is used with ESP-IDF HTTPS and host fault
// transports. A clean framing boundary and a complete byte count are distinct
// requirements; neither an EOF nor an advertised length alone proves success.
template<class Read,class Done,class Stop,class Progress>
Result readBody(const Bounds &b,uint8_t *out,size_t &received,Read read,Done done,Stop stop,Progress progress) {
  received=0;
  if(!out || !valid(b)) return Result::InvalidBounds;
  while(b.chunked ? !done() : received<size_t(b.contentLength)) {
    if(stop()) return Result::Stopped;
    const size_t remaining=(b.chunked ? b.capacity : size_t(b.contentLength))-received;
    if(!remaining) return Result::Overflow;
    const size_t request=std::min(remaining,size_t(4096));
    const int bytes=read(out+received,request);
    if(bytes==0 && b.chunked && done()) break;
    if(bytes<=0) return Result::Truncated;
    if(size_t(bytes)>request) return Result::Overflow;
    received+=size_t(bytes);
    progress(received,b.chunked ? b.capacity : size_t(b.contentLength));
  }
  return done() ? Result::Complete : Result::Truncated;
}
}
