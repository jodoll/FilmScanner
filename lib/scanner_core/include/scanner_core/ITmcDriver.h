#pragma once

#include "scanner_core/DriverConfig.h"

namespace scanner {

// Configuration and diagnostics only. It never generates steps.
class ITmcDriver {
 public:
  virtual ~ITmcDriver() = default;
  virtual bool begin() = 0;  // true if UART readback works
  virtual void applyConfig(const DriverConfig& config) = 0;
  virtual bool testConnection() = 0;
  virtual bool hasFault() = 0;  // false when readback is unavailable
};

}  // namespace scanner
