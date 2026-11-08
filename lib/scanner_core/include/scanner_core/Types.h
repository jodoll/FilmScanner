#pragma once

#include <stdint.h>

#include "scanner_core/StateMachine.h"

namespace scanner {

enum class CameraPhase : uint8_t {
  IDLE,
  SETTLE,
  FOCUS,
  SHUTTER,
  POST,
  DONE
};

enum class ButtonEvent : uint8_t {
  NONE,
  SHORT_PRESS,
  LONG_PRESS
};

enum class EventType : uint8_t {
  READY,
  RUN_STARTED,
  RUN_STOPPED,
  FRAME_STARTED,
  FRAME_DONE,
  NOTCH,
  JOG_DONE,
  JOG_STOPPED,
  STEP_DONE,
  STEP_FAILED,
  STEP_STOPPED,
  SHOT_DONE,
  SHOT_STOPPED,
  FAULT,
  CONFIG_SAVED,
  CONFIG_RESET,
  MOTOR_STATE
};

struct Event {
  EventType type;
  uint8_t   code;    // ErrorCode, StopReason or a boolean
  int32_t   value;   // frame index, position or steps
  int32_t   value2;  // second value when needed
};

// Exactly the fields of the STATUS line.
struct StatusSnapshot {
  State       state;
  Mode        mode;
  bool        stopRequested;
  bool        motorEnabled;
  CameraPhase camera;
  int32_t     pos;
  uint32_t    frames;     // total since boot
  uint32_t    runFrames;  // current or most recently finished run
  ErrorCode   lastError;
  uint32_t    uptimeMs;
};

// Wire names. These must match the tables in docs/05-serial-protocol.md.
const char* toString(State state);
const char* toString(Mode mode);
const char* toString(StopReason reason);
const char* toString(ErrorCode error);
const char* toString(CameraPhase phase);
const char* toString(EventType type);

}  // namespace scanner
