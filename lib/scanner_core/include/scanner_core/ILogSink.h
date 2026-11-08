#pragma once

#include <stdint.h>

namespace scanner {

class ILogSink {
 public:
  virtual ~ILogSink() = default;
  virtual void write(uint8_t level, const char* message) = 0;
};

}  // namespace scanner
