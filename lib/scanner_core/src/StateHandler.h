#pragma once

#include <stdint.h>

#include "scanner_core/StateMachine.h"

namespace scanner {
namespace detail {

enum class EventId : uint8_t {
  START_RUN,
  SCAN_ONE,
  SHOOT,
  JOG,
  NOTCH,
  MOTION_COMPLETED,
  PICTURE_TAKEN,
  STOP
};

enum class ErrorAction : uint8_t {
  KEEP,
  CLEAR
};

struct StateView {
  Mode mode;
  bool stopRequested;
};

struct Transition {
  ErrorCode error;
  State next;
  Mode mode;
  uint16_t announcements;
  StopReason reason;
  ErrorAction errorAction;
  bool stopRequested;
};

// A state is a description: its identity and the transitions it allows.
// Unsupported events inherit on() and answer with BUSY.
class StateHandler {
 public:
  State id() const { return id_; }

  virtual Transition on(EventId, StateView) const { return reject(); }

 protected:
  explicit StateHandler(State id) : id_(id) {}

  Transition reject() const {
    return {ErrorCode::BUSY, id_, Mode::NONE, 0, StopReason::NONE,
            ErrorAction::KEEP, false};
  }

  Transition accept(State next, Mode mode, Announcement announcement,
                    StopReason reason = StopReason::NONE,
                    ErrorAction errorAction = ErrorAction::KEEP) const {
    return accept(next, mode, static_cast<uint16_t>(announcement), reason,
                  errorAction);
  }

  Transition accept(State next, Mode mode, uint16_t announcementBits,
                    StopReason reason = StopReason::NONE,
                    ErrorAction errorAction = ErrorAction::KEEP) const {
    return {ErrorCode::NONE, next, mode, announcementBits, reason, errorAction,
            false};
  }

  static uint16_t announcements(Announcement first,
                                Announcement second = Announcement::NONE) {
    return static_cast<uint16_t>(first) | static_cast<uint16_t>(second);
  }

  // Keeps the current state and mode; the stop takes effect after the frame.
  Transition stopAfterFrame(Mode mode) const {
    return {ErrorCode::NONE, id_, mode, 0, StopReason::NONE,
            ErrorAction::KEEP, true};
  }

  Transition stopInRun(StateView view) const {
    if (view.mode == Mode::RUN) {
      return stopAfterFrame(view.mode);
    }
    return reject();
  }

 private:
  State id_;
};

const StateHandler* stateFor(State state);

}  // namespace detail
}  // namespace scanner
