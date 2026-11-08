#pragma once

#include <stdint.h>

namespace scanner {

// stealthChop is a byte, not a bool, so the packed layout is explicit.
struct DriverConfig {
  uint16_t microsteps;
  uint16_t runCurrentMa;
  uint8_t  holdCurrentPercent;
  uint8_t  stealthChop;
};

}  // namespace scanner
