#pragma once

#include <stdint.h>

#include "scanner_core/Config.h"

namespace scanner {

// The persisted record is a wire format and deliberately does not reuse the
// domain slice types: the slices must stay free to change without changing what
// is on the EEPROM. See ADR-0010.
struct ConfigRecord {
  uint16_t magic;
  uint8_t  version;
  uint8_t  reserved;
  uint16_t length;

  uint16_t notchDebounceMs;

  uint16_t buttonDebounceMs;
  uint16_t longPressMs;

  uint16_t settleMs;
  uint16_t focusMs;
  uint16_t shutterMs;
  uint16_t postShotMs;

  uint16_t seekSpeed;
  uint16_t acceleration;
  uint16_t jogSpeed;
  int32_t  offsetSteps;
  uint32_t maxSeekSteps;

  uint16_t notchTimeoutMs;
  uint16_t statusPeriodMs;

  uint16_t microsteps;
  uint16_t runCurrentMa;
  uint8_t  holdCurrentPercent;
  uint8_t  stealthChop;

  uint8_t logLevel;

  uint16_t crc;
};

// Build a complete, valid record from the aggregate.
ConfigRecord makeRecord(const Config& config);

// True when magic, version, length and checksum all match.
bool recordHasValidHeader(const ConfigRecord& record);

// Copy the fields into the aggregate. The caller validates the header first and
// the values afterwards.
void applyRecord(const ConfigRecord& record, Config& config);

}  // namespace scanner
