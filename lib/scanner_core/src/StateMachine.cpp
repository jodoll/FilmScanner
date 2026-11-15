#include "scanner_core/StateMachine.h"

#include "StateNode.h"

namespace scanner {

namespace {

detail::StateView stateView(Mode mode, bool stopRequested) {
  return {mode, stopRequested};
}

}  // namespace

StateMachine::StateMachine() : current_(detail::stateFor(State::BOOT)) {}

Result StateMachine::bootCompleted() {
  current_ = detail::stateFor(State::IDLE);
  return {state(), ErrorCode::NONE, StopReason::NONE,
          static_cast<uint16_t>(Announcement::NONE)};
}

Result StateMachine::startContinuousRun() {
  return apply(current_->transitionFor(detail::EventId::START_RUN,
                            stateView(mode_, stopRequested_)));
}

Result StateMachine::scanOneFrame() {
  return apply(current_->transitionFor(detail::EventId::SCAN_ONE,
                            stateView(mode_, stopRequested_)));
}

Result StateMachine::shootCurrentFrame() {
  return apply(current_->transitionFor(detail::EventId::SHOOT,
                            stateView(mode_, stopRequested_)));
}

Result StateMachine::jog() {
  return apply(current_->transitionFor(detail::EventId::JOG,
                            stateView(mode_, stopRequested_)));
}

Result StateMachine::notchDetected() {
  return apply(current_->transitionFor(detail::EventId::NOTCH,
                            stateView(mode_, stopRequested_)));
}

Result StateMachine::notchTimedOut() {
  return apply(current_->transitionFor(detail::EventId::NOTCH_TIMEOUT,
                            stateView(mode_, stopRequested_)));
}

Result StateMachine::motionCompleted() {
  return apply(current_->transitionFor(detail::EventId::MOTION_COMPLETED,
                            stateView(mode_, stopRequested_)));
}

Result StateMachine::pictureTaken() {
  return apply(current_->transitionFor(detail::EventId::PICTURE_TAKEN,
                            stateView(mode_, stopRequested_)));
}

Result StateMachine::stop() {
  return apply(current_->transitionFor(detail::EventId::STOP,
                            stateView(mode_, stopRequested_)));
}

Result StateMachine::abort() {
  return apply(current_->transitionFor(detail::EventId::ABORT,
                            stateView(mode_, stopRequested_)));
}

Result StateMachine::driverFaultDetected() {
  return apply(current_->transitionFor(detail::EventId::DRIVER_FAULT,
                            stateView(mode_, stopRequested_)));
}

State StateMachine::state() const { return current_->id(); }

Mode StateMachine::mode() const { return mode_; }

bool StateMachine::stopRequested() const { return stopRequested_; }

ErrorCode StateMachine::lastError() const { return lastError_; }

Result StateMachine::apply(const detail::Transition& transition) {
  if (transition.errorAction == detail::ErrorAction::REJECT) {
    return {state(), transition.error, StopReason::NONE,
            static_cast<uint16_t>(Announcement::NONE)};
  }

  if (transition.errorAction == detail::ErrorAction::SET) {
    lastError_ = transition.error;
  } else if (transition.errorAction == detail::ErrorAction::CLEAR) {
    lastError_ = ErrorCode::NONE;
  }
  if (transition.stopRequested) {
    stopRequested_ = true;
  }
  current_ = detail::stateFor(transition.next);
  mode_ = transition.mode;
  // The latch only matters while a run frame is active; every arrival in
  // IDLE ends the activity, so no stop request may leak into the next one.
  if (transition.next == State::IDLE) {
    stopRequested_ = false;
  }
  return {state(), ErrorCode::NONE, transition.reason,
          transition.announcements};
}

}  // namespace scanner
