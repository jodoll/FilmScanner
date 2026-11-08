#pragma once

#include <stdint.h>

namespace scanner {

// STEP/DIR level. Only StepperMotionController and the HAL use this.
class IStepperMotor {
 public:
  virtual ~IStepperMotor() = default;
  virtual void enable() = 0;
  virtual void disable() = 0;
  virtual bool isEnabled() const = 0;
  virtual void setMaxSpeed(uint16_t stepsPerSecond) = 0;
  virtual void setAcceleration(uint16_t stepsPerSecond2) = 0;
  virtual void moveTo(int32_t absolutePosition) = 0;
  virtual void moveBy(int32_t steps) = 0;
  virtual void run() = 0;   // generate pulses as needed
  virtual void stop() = 0;  // decelerate to rest
  virtual void halt() = 0;  // stop issuing pulses immediately
  virtual int32_t position() const = 0;
  virtual bool isRunning() const = 0;
};

}  // namespace scanner
