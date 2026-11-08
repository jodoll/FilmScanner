#pragma once

#include <stdint.h>

namespace scanner {

// States are activities that last over time. Instantaneous actions (enable a
// driver, start the camera) are entry and exit actions in ScannerController.
enum class State : uint8_t {
  BOOT,
  IDLE,
  SEEK,
  STOPPING,
  OFFSET,
  CAMERA,
  JOG,
  FAULT
};

// Why the machine is active. RUN and SINGLE share the frame states.
enum class Mode : uint8_t {
  NONE,
  RUN,
  SINGLE,
  SHOOT
};

enum class StopReason : uint8_t {
  NONE,
  REQUEST,
  ABORT,
  NOTCH_TIMEOUT,
  MOTION_TIMEOUT,
  ERROR
};

enum class ErrorCode : uint8_t {
  NONE,
  BAD_COMMAND,
  UNKNOWN_KEY,
  READ_ONLY,
  VALUE_OUT_OF_RANGE,
  BUSY,
  NOTCH_TIMEOUT,
  MOTION_TIMEOUT,
  DRIVER_FAULT,
  DRIVER_UART,
  EEPROM_INVALID,
  CONFIG_INVALID,
  INTERNAL
};

// Semantic facts, not actions. ScannerController maps them to protocol events.
enum class Announcement : uint16_t {
  NONE          = 0,
  NOTCH         = 1u << 0,
  RUN_STARTED   = 1u << 1,
  FRAME_STARTED = 1u << 2,
  FRAME_DONE    = 1u << 3,
  RUN_STOPPED   = 1u << 4,
  STEP_DONE     = 1u << 5,
  STEP_STOPPED  = 1u << 6,
  STEP_FAILED   = 1u << 7,
  JOG_DONE      = 1u << 8,
  JOG_STOPPED   = 1u << 9,
  SHOT_DONE     = 1u << 10,
  SHOT_STOPPED  = 1u << 11,
  FAULT_RAISED  = 1u << 12
};

struct Result {
  State      state;
  ErrorCode  error;          // NONE if the call was accepted
  StopReason reason;         // NONE unless this transition stopped something
  uint16_t   announcements;  // bitmask of Announcement
};

// Pure activity state machine. No clock, config, hardware or protocol types.
class StateMachine {
 public:
  void begin();

  Result bootCompleted();

  Result startContinuousRun();
  Result scanOneFrame();
  Result shootCurrentFrame();
  Result jog();
  Result stop();
  Result abort();

  Result notchDetected();
  Result motionCompleted();
  Result pictureTaken();
  Result notchTimedOut();
  Result driverFaultDetected();
  Result buttonPressedShort();
  Result buttonPressedLong();

  State state() const;
  Mode mode() const;
  bool stopRequested() const;
  ErrorCode lastError() const;
};

}  // namespace scanner
