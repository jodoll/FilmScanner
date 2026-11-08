#pragma once

#include <stdint.h>

namespace scanner {

struct MotionConfig {
  uint16_t seekSpeed;
  uint16_t acceleration;
  uint16_t jogSpeed;
  int32_t  offsetSteps;
  uint32_t maxSeekSteps;
};

}  // namespace scanner
