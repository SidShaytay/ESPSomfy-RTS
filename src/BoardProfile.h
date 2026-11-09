#pragma once
#include <Arduino.h>
#include <math.h>

#ifdef ESPSOMFY_LILYGO_T_EMBED_CC1101_PLUS
#error "Retired Plus-only macro: use ESPSOMFY_LILYGO_T_EMBED_CC1101 for CC1101 and Plus"
#endif
namespace BoardProfile {
#if defined(ESPSOMFY_LILYGO_T_EMBED_CC1101)
#if !defined(CONFIG_IDF_TARGET_ESP32S3)
#error "The shared LILYGO T-Embed CC1101 profile requires an ESP32-S3 target"
#endif
constexpr bool integratedRadio = true;
constexpr const char *name = "CC1101 / Plus";
#else
constexpr bool integratedRadio = false;
constexpr const char *name = "Generic";
#endif

// LILYGO utilities.h and CC1101 schematics: see LILYGO.md for pinned sources.
constexpr uint8_t power = 15;
constexpr uint8_t powerSDA = 8;
constexpr uint8_t powerSCL = 18;
constexpr uint8_t sck = 11;
constexpr uint8_t mosi = 9;
constexpr uint8_t miso = 10;
constexpr uint8_t cs = 12;
constexpr uint8_t tx = 3;
constexpr uint8_t rx = 38;
constexpr uint8_t sw1 = 47;
constexpr uint8_t sw0 = 48;
constexpr uint8_t displayCS = 41;
constexpr uint8_t displayDC = 16;
constexpr uint8_t wakeButton = 0;
constexpr uint8_t backButton = 6;
constexpr uint8_t encoderA = 4;
constexpr uint8_t encoderB = 5;
constexpr uint8_t sdCS = 13;
constexpr uint8_t nrfCS = 44;
constexpr uint8_t nrfCE = 43;
constexpr uint8_t backlight = 21;

void begin();
// Match the pinned driver's PA/calibration branches; the integrated board is 433 MHz only.
inline bool radioFrequencyAllowed(float frequency) {
  if(!isfinite(frequency))return false;
  if(integratedRadio)return frequency>=387.0f && frequency<=464.0f;
  return (frequency>=300.0f && frequency<=348.0f) ||
    (frequency>=378.0f && frequency<=464.0f) || (frequency>=779.0f && frequency<=928.0f);
}
bool prepareRadio(float frequency);
bool usesPin(uint8_t pin);
}
