#pragma once
#include <stddef.h>
#include <stdint.h>
namespace LocalUi {
// Current bundled GFX fonts are ASCII. Preserve model UTF-8, but make each
// unsupported codepoint one '?' and neutralize control bytes in display labels.
inline void lcdLabel(const char *input,char *out,size_t capacity) {
  if(!out || !capacity) return;
  size_t n=0,at=0;
  while(input && at<128 && input[at] && n+1<capacity) {
    uint32_t code=uint8_t(input[at]); unsigned follow=0; uint32_t least=0;
    if(code>=0xc2 && code<=0xdf) {follow=1;code&=0x1f;least=0x80;}
    else if(code>=0xe0 && code<=0xef) {follow=2;code&=0x0f;least=0x800;}
    else if(code>=0xf0 && code<=0xf4) {follow=3;code&=7;least=0x10000;}
    bool valid=follow!=0;
    for(unsigned i=1;i<=follow;i++) {
      if(at+i>=128 || (uint8_t(input[at+i])&0xc0)!=0x80) {valid=false;break;}
      code=(code<<6)|(uint8_t(input[at+i])&0x3f);
    }
    valid &= code>=least && code<=0x10ffff && !(code>=0xd800 && code<=0xdfff);
    const uint8_t first=uint8_t(input[at]);
    out[n++]=first>=32 && first<=126?char(first):'?';
    at+=valid?follow+1:1;
  }
  out[n]=0;
}
} // namespace LocalUi
