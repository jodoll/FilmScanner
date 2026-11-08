#pragma once

#include <stdint.h>

namespace scanner {

// Durable bytes for the configuration record. The record always starts at
// address 0, so the interface carries no address and no notion of a record.
class INonVolatileStore {
 public:
  virtual ~INonVolatileStore() = default;
  virtual bool read(uint8_t* buffer, uint16_t length) = 0;
  virtual bool write(const uint8_t* buffer, uint16_t length) = 0;
};

}  // namespace scanner
