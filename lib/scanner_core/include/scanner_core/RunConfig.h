#pragma once

#include <stdint.h>

namespace scanner {

struct RunConfig {
  uint16_t notchTimeoutMs;
  uint16_t statusPeriodMs;
};

}  // namespace scanner
