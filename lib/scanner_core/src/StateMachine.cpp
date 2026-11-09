#include "scanner_core/StateMachine.h"

namespace scanner {

Result StateMachine::bootCompleted() {
  state_ = State::IDLE;
  return {state_, ErrorCode::NONE, StopReason::NONE,
          static_cast<uint16_t>(Announcement::NONE)};
}

Result StateMachine::startContinuousRun() {
  state_ = State::SEEK;
  mode_ = Mode::RUN;
  return {state_, ErrorCode::NONE, StopReason::NONE,
          static_cast<uint16_t>(Announcement::RUN_STARTED)};
}

State StateMachine::state() const { return state_; }

Mode StateMachine::mode() const { return mode_; }

bool StateMachine::stopRequested() const { return stopRequested_; }

ErrorCode StateMachine::lastError() const { return lastError_; }

}  // namespace scanner
