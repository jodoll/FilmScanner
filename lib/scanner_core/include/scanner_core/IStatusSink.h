#pragma once

#include "scanner_core/Types.h"

namespace scanner {

// Used for STATUS lines only: the explicit command and the periodic report.
class IStatusSink {
 public:
  virtual ~IStatusSink() = default;
  virtual void write(const StatusSnapshot& status) = 0;
};

}  // namespace scanner
