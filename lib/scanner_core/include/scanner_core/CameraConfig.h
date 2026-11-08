#pragma once

#include <stdint.h>

namespace scanner {

struct CameraConfig {
  uint16_t settleMs;
  uint16_t focusMs;
  uint16_t shutterMs;
  uint16_t postShotMs;
};

}  // namespace scanner
