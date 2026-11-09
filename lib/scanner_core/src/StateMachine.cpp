#include "scanner_core/StateMachine.h"

namespace scanner {

State StateMachine::state() const { return state_; }

Mode StateMachine::mode() const { return mode_; }

bool StateMachine::stopRequested() const { return stopRequested_; }

ErrorCode StateMachine::lastError() const { return lastError_; }

}  // namespace scanner
