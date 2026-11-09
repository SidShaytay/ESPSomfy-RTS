#pragma once
#include "EthPhy.h"
#include "LivePinClaims.h"
#if defined(ARDUINO) && HAS_ETHERNET
#include <ETH.h>
#endif
// Firmware uses the pinned SDK's actual RMII macros, including board overrides.
// Host defaults below reproduce its ESP32/P4 lists; they are synthetic tests,
// not evidence of physical wiring or release of an external PHY's pads.
inline uint64_t ethernetDataPinClaims() {
#if defined(ETH_RMII_TX_EN)
  return LivePinClaims::pinMask(ETH_RMII_TX_EN)|LivePinClaims::pinMask(ETH_RMII_TX0)|
    LivePinClaims::pinMask(ETH_RMII_TX1)|LivePinClaims::pinMask(ETH_RMII_RX0)|
    LivePinClaims::pinMask(ETH_RMII_RX1_EN)|LivePinClaims::pinMask(ETH_RMII_CRS_DV)
#if defined(ETH_RMII_CLK)
    |LivePinClaims::pinMask(ETH_RMII_CLK)
#endif
    ;
#elif defined(CONFIG_IDF_TARGET_ESP32P4)
  return LivePinClaims::pinMask(49)|LivePinClaims::pinMask(34)|LivePinClaims::pinMask(35)|
    LivePinClaims::pinMask(29)|LivePinClaims::pinMask(30)|LivePinClaims::pinMask(28)|LivePinClaims::pinMask(50);
#else
  return LivePinClaims::pinMask(21)|LivePinClaims::pinMask(19)|LivePinClaims::pinMask(22)|
    LivePinClaims::pinMask(25)|LivePinClaims::pinMask(26)|LivePinClaims::pinMask(27);
#endif
}
