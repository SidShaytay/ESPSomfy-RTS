#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
// The scan uses the existing configuration owner's generation. A boot nonce
// distinguishes an old browser's capability after a controller reboot.
struct FrequencyScanToken {
  uint64_t bootSession=0,generation=0;
  bool valid() const {return bootSession && generation;}
  bool same(const FrequencyScanToken &other) const {
    return valid() && other.valid() && bootSession==other.bootSession && generation==other.generation;
  }
};
static_assert(sizeof(FrequencyScanToken)==16,"A scan capability holds no request pointer or credential");
namespace FrequencyScanTokenDetail {
inline bool decimal(const char *text,size_t length,uint64_t &out) {
  if(!length || length>20 || text[0]=='0')return false;
  uint64_t value=0;
  for(size_t i=0;i<length;++i) {
    const char c=text[i];
    if(c<'0' || c>'9' || value>(UINT64_MAX-uint64_t(c-'0'))/10)return false;
    value=value*10+uint64_t(c-'0');
  }
  out=value;return true;
}
}
// Length is mandatory: an embedded NUL cannot hide an unconsumed suffix.
inline bool parseFrequencyScanToken(const char *text,size_t length,FrequencyScanToken &out) {
  if(!text || length<3 || length>41)return false;
  size_t separator=length;
  for(size_t i=0;i<length;++i)if(text[i]=='.') {
    if(separator!=length)return false;
    separator=i;
  }
  if(separator==length)return false;
  FrequencyScanToken candidate;
  if(!FrequencyScanTokenDetail::decimal(text,separator,candidate.bootSession) ||
     !FrequencyScanTokenDetail::decimal(text+separator+1,length-separator-1,candidate.generation))return false;
  out=candidate;return true;
}
inline bool formatFrequencyScanToken(FrequencyScanToken value,char *out,size_t capacity) {
  if(!out || !capacity)return false;
  out[0]=0;
  if(!value.valid())return false;
  const int count=snprintf(out,capacity,"%llu.%llu",static_cast<unsigned long long>(value.bootSession),
                         static_cast<unsigned long long>(value.generation));
  if(count<0 || size_t(count)>=capacity){out[0]=0;return false;}
  return true;
}
