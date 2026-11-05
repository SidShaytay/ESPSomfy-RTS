#ifndef BIT_LENGTH_H
#define BIT_LENGTH_H
#include <stdint.h>

// The encoded radio payload has room for these two protocol frame lengths.
inline bool supportedBitLength(uint8_t bits) { return bits == 56 || bits == 80; }

#endif
