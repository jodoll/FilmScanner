#include "scanner_core/StateMachine.h"

#include "StateHandler.h"

namespace scanner {

StateMachine::StateMachine() : current_(detail::stateFor(State::BOOT)) {}

Result StateMachine::bootCompleted() {
  current_ = detail::stateFor(State::IDLE);
  return {state(), ErrorCode::NONE, StopReason::NONE,
          static_cast<uint16_t>(Announcement::NONE)};
}

Result StateMachine::startContinuousRun() {
  return apply(current_->on(detail::EventId::START_RUN));
}

Result StateMachine::scanOneFrame() {
  return apply(current_->on(detail::EventId::SCAN_ONE));
}

Result StateMachine::shootCurrentFrame() {
  return apply(current_->on(detail::EventId::SHOOT));
}

Result StateMachine::jog() {
  return apply(current_->on(detail::EventId::JOG));
}

State StateMachine::state() const { return current_->id(); }

Mode StateMachine::mode() const { return mode_; }

bool StateMachine::stopRequested() const { return stopRequested_; }

ErrorCode StateMachine::lastError() const { return lastError_; }

Result StateMachine::apply(const detail::Transition& transition) {
  if (transition.error != ErrorCode::NONE) {
    return {state(), transition.error, StopReason::NONE,
            static_cast<uint16_t>(Announcement::NONE)};
  }

  if (transition.errorAction == detail::ErrorAction::CLEAR) {
    lastError_ = ErrorCode::NONE;
  }
  current_ = detail::stateFor(transition.next);
  mode_ = transition.mode;
  return {state(), ErrorCode::NONE, transition.reason,
          transition.announcements};
}

}  // namespace scanner
