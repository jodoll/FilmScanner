#pragma once

#include "scanner_core/Types.h"

namespace scanner {

class IEventSink {
 public:
  virtual ~IEventSink() = default;
  virtual void emit(const Event& event) = 0;
};

}  // namespace scanner
