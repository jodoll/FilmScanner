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
  NOTCH_TIMEOUT,
  MOTION_COMPLETED,
  PICTURE_TAKEN,
  STOP,
  ABORT
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

// A node of the transition graph: its identity and its outgoing edges. The
// machine queries a node for the transition an event produces; an event the
// node has no edge for inherits transitionFor() and answers with BUSY.
class StateNode {
 public:
  State id() const { return id_; }

  virtual Transition transitionFor(EventId, StateView) const {
    return noTransition();
  }

 protected:
  explicit StateNode(State id) : id_(id) {}

  Transition noTransition() const {
    return {ErrorCode::BUSY, id_, Mode::NONE, 0, StopReason::NONE,
            ErrorAction::KEEP, false};
  }

  Transition accepted(State next, Mode mode, Announcement announcement,
                      StopReason reason = StopReason::NONE,
                      ErrorAction errorAction = ErrorAction::KEEP) const {
    return accepted(next, mode, static_cast<uint16_t>(announcement), reason,
                    errorAction);
  }

  Transition accepted(State next, Mode mode, uint16_t announcementBits,
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

  // Any active activity ends in IDLE with its mode's stop announcement.
  Transition abortTransition(Mode mode) const {
    Announcement stop;
    switch (mode) {
      case Mode::SINGLE:
        stop = Announcement::STEP_STOPPED;
        break;
      case Mode::SHOOT:
        stop = Announcement::SHOT_STOPPED;
        break;
      default:
        stop = Announcement::RUN_STOPPED;
        break;
    }
    return accepted(State::IDLE, Mode::NONE, stop, StopReason::ABORT);
  }

  // RUN latches and finishes the frame; SINGLE stops immediately.
  Transition stopTransition(StateView view) const {
    if (view.mode == Mode::RUN) {
      return stopAfterFrame(view.mode);
    }
    if (view.mode == Mode::SINGLE) {
      return accepted(State::IDLE, Mode::NONE, Announcement::STEP_STOPPED,
                      StopReason::REQUEST);
    }
    return noTransition();
  }

 private:
  State id_;
};

const StateNode* stateFor(State state);

}  // namespace detail
}  // namespace scanner
