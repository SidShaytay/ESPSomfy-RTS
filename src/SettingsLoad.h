#pragma once
#include "SettingsLoadState.h"
#include <nvs.h>
#include <stddef.h>
enum class SettingsReadField : uint8_t { absent, present, failed };
// Bounded read-only preflight; callers publish the complete typed draft only
// after status().readable(). No missing/error read is converted to a value.
class SettingsLoadReader {
  nvs_handle_t handle=0;
  bool opened=false;
  SettingsLoadStatus value{SettingsLoadState::knownEmpty};
  SettingsReadField result(esp_err_t);
  SettingsReadField readText(const char *,char *,size_t);
  template<class T,class Get> SettingsReadField number(const char *key,T &out,Get get) {
    if(value.state==SettingsLoadState::failed)return SettingsReadField::failed;
    if(!opened)return SettingsReadField::absent;
    T draft{};const auto read=result(get(handle,key,&draft));
    if(read==SettingsReadField::present)out=draft;
    return read;
  }
public:
  explicit SettingsLoadReader(const char *name);
  ~SettingsLoadReader();
  SettingsLoadReader(const SettingsLoadReader &)=delete;
  SettingsLoadReader &operator=(const SettingsLoadReader &)=delete;
  SettingsLoadStatus status()const{return value;}
  void invalid(){if(value.readable())value.state=SettingsLoadState::invalid;}
  SettingsReadField i8(const char *key,int8_t &out){return number(key,out,nvs_get_i8);}
  SettingsReadField u8(const char *key,uint8_t &out){return number(key,out,nvs_get_u8);}
  SettingsReadField i16(const char *key,int16_t &out){return number(key,out,nvs_get_i16);}
  SettingsReadField flag(const char *,bool &);
  SettingsReadField real(const char *,float &);
  template<size_t N> SettingsReadField text(const char *key,char (&out)[N]) {
    static_assert(N<=65 && N>0,"Settings strings have a bounded byte capacity");
    return readText(key,out,N);
  }
};
