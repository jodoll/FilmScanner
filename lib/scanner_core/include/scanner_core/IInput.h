#pragma once

namespace scanner {

// true means "active": the pin reads LOW.
class IInput {
 public:
  virtual ~IInput() = default;
  virtual bool read() const = 0;
};

}  // namespace scanner
