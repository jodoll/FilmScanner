#include "scanner_core/StateMachine.h"

namespace scanner {

Result StateMachine::bootCompleted() {
  state_ = State::IDLE;
  return {state_, ErrorCode::NONE, StopReason::NONE,
          static_cast<uint16_t>(Announcement::NONE)};
}

Result StateMachine::startContinuousRun() {
  if (state_ != State::IDLE && state_ != State::FAULT) {
    return {state_, ErrorCode::BUSY, StopReason::NONE,
            static_cast<uint16_t>(Announcement::NONE)};
  }

  state_ = State::SEEK;
  mode_ = Mode::RUN;
  return {state_, ErrorCode::NONE, StopReason::NONE,
          static_cast<uint16_t>(Announcement::RUN_STARTED)};
}

Result StateMachine::scanOneFrame() {
  if (state_ != State::IDLE) {
    return {state_, ErrorCode::BUSY, StopReason::NONE,
            static_cast<uint16_t>(Announcement::NONE)};
  }

  state_ = State::SEEK;
  mode_ = Mode::SINGLE;
  return {state_, ErrorCode::NONE, StopReason::NONE,
          static_cast<uint16_t>(Announcement::NONE)};
}

Result StateMachine::shootCurrentFrame() {
  state_ = State::CAMERA;
  mode_ = Mode::SHOOT;
  return {state_, ErrorCode::NONE, StopReason::NONE,
          static_cast<uint16_t>(Announcement::NONE)};
}

Result StateMachine::jog() {
  state_ = State::JOG;
  mode_ = Mode::NONE;
  return {state_, ErrorCode::NONE, StopReason::NONE,
          static_cast<uint16_t>(Announcement::NONE)};
}

State StateMachine::state() const { return state_; }

Mode StateMachine::mode() const { return mode_; }

bool StateMachine::stopRequested() const { return stopRequested_; }

ErrorCode StateMachine::lastError() const { return lastError_; }

}  // namespace scanner
