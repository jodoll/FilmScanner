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
  NOTCH
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
            ErrorAction::KEEP};
  }

  Transition accept(State next, Mode mode, Announcement announcement,
                    StopReason reason = StopReason::NONE,
                    ErrorAction errorAction = ErrorAction::KEEP) const {
    return {ErrorCode::NONE, next, mode,
            static_cast<uint16_t>(announcement), reason, errorAction};
  }

 private:
  State id_;
};

const StateHandler* stateFor(State state);

}  // namespace detail
}  // namespace scanner
