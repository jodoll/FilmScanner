#include "StateHandler.h"

namespace scanner {
namespace detail {
namespace {

// ---------------------------------------------------------------------------
// BOOT
// ---------------------------------------------------------------------------
class BootState : public StateHandler {
 public:
  BootState() : StateHandler(State::BOOT) {}
};

// ---------------------------------------------------------------------------
// IDLE
// ---------------------------------------------------------------------------
class IdleState : public StateHandler {
 public:
  IdleState() : StateHandler(State::IDLE) {}
  Transition on(EventId event, StateView) const override;
};

Transition IdleState::on(EventId event, StateView) const {
  switch (event) {
    case EventId::START_RUN:
      return accept(State::SEEK, Mode::RUN, Announcement::RUN_STARTED,
                    StopReason::NONE, ErrorAction::CLEAR);
    case EventId::SCAN_ONE:
      return accept(State::SEEK, Mode::SINGLE, Announcement::NONE,
                    StopReason::NONE, ErrorAction::CLEAR);
    case EventId::SHOOT:
      return accept(State::CAMERA, Mode::SHOOT, Announcement::NONE,
                    StopReason::NONE, ErrorAction::CLEAR);
    case EventId::JOG:
      return accept(State::JOG, Mode::NONE, Announcement::NONE);
    default:
      return reject();
  }
}

// ---------------------------------------------------------------------------
// SEEK
// ---------------------------------------------------------------------------
class SeekState : public StateHandler {
 public:
  SeekState() : StateHandler(State::SEEK) {}
  Transition on(EventId event, StateView view) const override;
};

Transition SeekState::on(EventId event, StateView view) const {
  switch (event) {
    case EventId::NOTCH:
      return accept(State::STOPPING, view.mode, Announcement::NOTCH);
    default:
      return reject();
  }
}

// ---------------------------------------------------------------------------
// STOPPING
// ---------------------------------------------------------------------------
class StoppingState : public StateHandler {
 public:
  StoppingState() : StateHandler(State::STOPPING) {}
  Transition on(EventId event, StateView view) const override;
};

Transition StoppingState::on(EventId event, StateView view) const {
  switch (event) {
    case EventId::MOTION_COMPLETED:
      return accept(State::OFFSET, view.mode, Announcement::NONE);
    default:
      return reject();
  }
}

// ---------------------------------------------------------------------------
// OFFSET
// ---------------------------------------------------------------------------
class OffsetState : public StateHandler {
 public:
  OffsetState() : StateHandler(State::OFFSET) {}
};

// ---------------------------------------------------------------------------
// CAMERA
// ---------------------------------------------------------------------------
class CameraState : public StateHandler {
 public:
  CameraState() : StateHandler(State::CAMERA) {}
};

// ---------------------------------------------------------------------------
// JOG
// ---------------------------------------------------------------------------
class JogState : public StateHandler {
 public:
  JogState() : StateHandler(State::JOG) {}
};

// ---------------------------------------------------------------------------
// FAULT
// ---------------------------------------------------------------------------
class FaultState : public StateHandler {
 public:
  FaultState() : StateHandler(State::FAULT) {}
  Transition on(EventId event, StateView) const override;
};

Transition FaultState::on(EventId event, StateView) const {
  switch (event) {
    case EventId::START_RUN:
      return accept(State::SEEK, Mode::RUN, Announcement::RUN_STARTED,
                    StopReason::NONE, ErrorAction::CLEAR);
    default:
      return reject();
  }
}

}  // namespace

const StateHandler* stateFor(State state) {
  static const BootState kBoot;
  static const IdleState kIdle;
  static const SeekState kSeek;
  static const StoppingState kStopping;
  static const OffsetState kOffset;
  static const CameraState kCamera;
  static const JogState kJog;
  static const FaultState kFault;

  // Indexed by State; the order matches the enum.
  static const StateHandler* const kStates[] = {
      &kBoot, &kIdle,     &kSeek, &kStopping,
      &kOffset, &kCamera, &kJog,  &kFault,
  };
  static_assert(sizeof(kStates) / sizeof(kStates[0]) == 8,
                "Every State needs a handler");
  return kStates[static_cast<uint8_t>(state)];
}

}  // namespace detail
}  // namespace scanner
