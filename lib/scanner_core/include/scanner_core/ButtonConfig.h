#pragma once

#include <stdint.h>

namespace scanner {

struct ButtonConfig {
  uint16_t debounceMs;
  uint16_t longPressMs;
};

}  // namespace scanner
