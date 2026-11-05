#ifndef SSDP_REQUEST_LINE_H
#define SSDP_REQUEST_LINE_H

#include <stddef.h>
#include <stdint.h>

// Keep the request-line suffix terminated while ignoring excess UDP bytes.
template <size_t Capacity>
inline void appendSsdpRequestLineChar(char (&buffer)[Capacity], uint8_t &pos, char c) {
  if(pos < Capacity - 1) {
    buffer[pos++] = c;
    buffer[pos] = '\0';
  }
}

#endif
