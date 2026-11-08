#pragma once

#include <stdint.h>

#include "scanner_core/StateMachine.h"

namespace scanner {

// The command vocabulary and its arguments are defined in
// docs/05-serial-protocol.md §3.
struct Command {
  enum class Type : uint8_t {
    NONE,
    START,
    STOP,
    ABORT,
    STATUS,
    CONFIG,
    GET,
    SET,
    SAVE,
    RESET_CONFIG,
    JOG,
    STEP,
    SHOOT,
    ENABLE,
    DISABLE,
    VERSION,
    HELP,
    PING
  };

  Type    type;
  char    key[24];
  int32_t value;
  bool    hasValue;
};

ErrorCode parseCommand(const char* line, Command& out);

}  // namespace scanner
