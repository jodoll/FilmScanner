#include "StateNode.h"

namespace scanner {
namespace detail {
namespace {

// ---------------------------------------------------------------------------
// BOOT
// ---------------------------------------------------------------------------
class BootState : public StateNode {
 public:
  BootState() : StateNode(State::BOOT) {}
};

// ---------------------------------------------------------------------------
// IDLE
// ---------------------------------------------------------------------------
class IdleState : public StateNode {
 public:
  IdleState() : StateNode(State::IDLE) {}
  Transition transitionFor(EventId event, StateView) const override;
};

Transition IdleState::transitionFor(EventId event, StateView) const {
  switch (event) {
    case EventId::START_RUN:
      return accepted(State::SEEK, Mode::RUN, Announcement::RUN_STARTED,
                      StopReason::NONE, ErrorAction::CLEAR);
    case EventId::SCAN_ONE:
      return accepted(State::SEEK, Mode::SINGLE, Announcement::NONE,
                      StopReason::NONE, ErrorAction::CLEAR);
    case EventId::SHOOT:
      return accepted(State::CAMERA, Mode::SHOOT, Announcement::NONE,
                      StopReason::NONE, ErrorAction::CLEAR);
    case EventId::JOG:
      return accepted(State::JOG, Mode::NONE, Announcement::NONE);
    default:
      return noTransition();
  }
}

// ---------------------------------------------------------------------------
// SEEK
// ---------------------------------------------------------------------------
class SeekState : public StateNode {
 public:
  SeekState() : StateNode(State::SEEK) {}
  Transition transitionFor(EventId event, StateView view) const override;
};

Transition SeekState::transitionFor(EventId event, StateView view) const {
  switch (event) {
    case EventId::NOTCH:
      return accepted(State::STOPPING, view.mode, Announcement::NOTCH);
    case EventId::NOTCH_TIMEOUT:
      if (view.mode == Mode::RUN) {
        return accepted(State::IDLE, Mode::NONE, Announcement::RUN_STOPPED,
                        StopReason::NOTCH_TIMEOUT);
      }
      if (view.mode == Mode::SINGLE) {
        return accepted(State::IDLE, Mode::NONE, Announcement::STEP_FAILED,
                        StopReason::NOTCH_TIMEOUT);
      }
      return noTransition();
    case EventId::MOTION_COMPLETED:
      if (view.mode == Mode::RUN) {
        return accepted(State::IDLE, Mode::NONE, Announcement::RUN_STOPPED,
                        StopReason::MOTION_TIMEOUT);
      }
      if (view.mode == Mode::SINGLE) {
        return accepted(State::IDLE, Mode::NONE, Announcement::STEP_FAILED,
                        StopReason::MOTION_TIMEOUT);
      }
      return noTransition();
    case EventId::STOP:
      return stopTransition(view);
    case EventId::ABORT:
      return abortTransition(view.mode);
    case EventId::DRIVER_FAULT:
      return {ErrorCode::DRIVER_FAULT, State::FAULT, Mode::NONE,
              announcements(Announcement::RUN_STOPPED,
                            Announcement::FAULT_RAISED),
              StopReason::ERROR, ErrorAction::SET, false};
    default:
      return noTransition();
  }
}

// ---------------------------------------------------------------------------
// STOPPING
// ---------------------------------------------------------------------------
class StoppingState : public StateNode {
 public:
  StoppingState() : StateNode(State::STOPPING) {}
  Transition transitionFor(EventId event, StateView view) const override;
};

Transition StoppingState::transitionFor(EventId event, StateView view) const {
  switch (event) {
    case EventId::MOTION_COMPLETED:
      return accepted(State::OFFSET, view.mode, Announcement::NONE);
    case EventId::STOP:
      return stopTransition(view);
    case EventId::ABORT:
      return abortTransition(view.mode);
    default:
      return noTransition();
  }
}

// ---------------------------------------------------------------------------
// OFFSET
// ---------------------------------------------------------------------------
class OffsetState : public StateNode {
 public:
  OffsetState() : StateNode(State::OFFSET) {}
  Transition transitionFor(EventId event, StateView view) const override;
};

Transition OffsetState::transitionFor(EventId event, StateView view) const {
  switch (event) {
    case EventId::MOTION_COMPLETED:
      return accepted(State::CAMERA, view.mode, Announcement::FRAME_STARTED);
    case EventId::STOP:
      return stopTransition(view);
    case EventId::ABORT:
      return abortTransition(view.mode);
    default:
      return noTransition();
  }
}

// ---------------------------------------------------------------------------
// CAMERA
// ---------------------------------------------------------------------------
class CameraState : public StateNode {
 public:
  CameraState() : StateNode(State::CAMERA) {}
  Transition transitionFor(EventId event, StateView view) const override;

 private:
  Transition pictureTakenTransition(StateView view) const;
  Transition runPictureTakenTransition(StateView view) const;
};

Transition CameraState::transitionFor(EventId event, StateView view) const {
  switch (event) {
    case EventId::PICTURE_TAKEN:
      return pictureTakenTransition(view);
    case EventId::STOP:
      if (view.mode == Mode::SHOOT) {
        return accepted(State::IDLE, Mode::NONE, Announcement::SHOT_STOPPED,
                        StopReason::REQUEST);
      }
      return stopTransition(view);
    case EventId::ABORT:
      return abortTransition(view.mode);
    default:
      return noTransition();
  }
}

Transition CameraState::pictureTakenTransition(StateView view) const {
  switch (view.mode) {
    case Mode::RUN:
      return runPictureTakenTransition(view);
    case Mode::SINGLE:
      return accepted(State::IDLE, Mode::NONE,
                      announcements(Announcement::FRAME_DONE,
                                    Announcement::STEP_DONE));
    case Mode::SHOOT:
      return accepted(State::IDLE, Mode::NONE, Announcement::SHOT_DONE);
    default:
      return noTransition();
  }
}

Transition CameraState::runPictureTakenTransition(StateView view) const {
  if (view.stopRequested) {
    return accepted(State::IDLE, Mode::NONE,
                    announcements(Announcement::FRAME_DONE,
                                  Announcement::RUN_STOPPED),
                    StopReason::REQUEST);
  }
  return accepted(State::SEEK, Mode::RUN, Announcement::FRAME_DONE);
}

// ---------------------------------------------------------------------------
// JOG
// ---------------------------------------------------------------------------
class JogState : public StateNode {
 public:
  JogState() : StateNode(State::JOG) {}

  Transition transitionFor(EventId event, StateView view) const override {
    switch (event) {
      case EventId::MOTION_COMPLETED:
        return accepted(State::IDLE, Mode::NONE, Announcement::JOG_DONE);
      case EventId::STOP:
        return accepted(State::IDLE, Mode::NONE, Announcement::JOG_STOPPED,
                        StopReason::REQUEST);
      case EventId::ABORT:
        return accepted(State::IDLE, Mode::NONE, Announcement::JOG_STOPPED,
                        StopReason::ABORT);
      default:
        return noTransition();
    }
  }
};

// ---------------------------------------------------------------------------
// FAULT
// ---------------------------------------------------------------------------
class FaultState : public StateNode {
 public:
  FaultState() : StateNode(State::FAULT) {}
  Transition transitionFor(EventId event, StateView) const override;
};

Transition FaultState::transitionFor(EventId event, StateView) const {
  switch (event) {
    case EventId::START_RUN:
      return accepted(State::SEEK, Mode::RUN, Announcement::RUN_STARTED,
                      StopReason::NONE, ErrorAction::CLEAR);
    default:
      return noTransition();
  }
}

}  // namespace

const StateNode* stateFor(State state) {
  static const BootState kBoot;
  static const IdleState kIdle;
  static const SeekState kSeek;
  static const StoppingState kStopping;
  static const OffsetState kOffset;
  static const CameraState kCamera;
  static const JogState kJog;
  static const FaultState kFault;

  // Indexed by State; the order matches the enum.
  static const StateNode* const kStates[] = {
      &kBoot, &kIdle,     &kSeek, &kStopping,
      &kOffset, &kCamera, &kJog,  &kFault,
  };
  static_assert(sizeof(kStates) / sizeof(kStates[0]) == 8,
                "Every State needs a node");
  return kStates[static_cast<uint8_t>(state)];
}

}  // namespace detail
}  // namespace scanner
