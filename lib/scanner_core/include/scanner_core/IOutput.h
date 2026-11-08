#pragma once

namespace scanner {

class IOutput {
 public:
  virtual ~IOutput() = default;
  virtual void write(bool active) = 0;
};

}  // namespace scanner
