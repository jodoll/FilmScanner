#pragma once

#include <stdint.h>

#include "scanner_core/MotionConfig.h"

namespace scanner {

// Activity-level motion. Units, enable polarity and pulse generation stay in
// the implementation.
class IMotionController {
 public:
  virtual ~IMotionController() = default;

  virtual void startSeek() = 0;
  virtual void applyOffset() = 0;
  virtual void startJog(int32_t amount) = 0;
  virtual void stop() = 0;    // controlled deceleration
  virtual void halt() = 0;    // immediate
  virtual void idle() = 0;    // inactive; may de-energize
  virtual void enable() = 0;  // maintenance override
  virtual void disable() = 0;

  virtual void applyConfig(const MotionConfig& config) = 0;
  virtual void poll() = 0;  // generate pulses as needed

  virtual bool isMoving() const = 0;
  virtual bool isEnabled() const = 0;
  virtual int32_t position() const = 0;
};

}  // namespace scanner
