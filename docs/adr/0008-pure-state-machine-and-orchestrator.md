# ADR-0008 — Pure activity state machine and orchestrator

- **Status:** Accepted
- **Date:** 2026-11-08
- **Deciders:** Project owner, firmware architect

## Context

The firmware needs a state machine that is the single source of truth for the
scan sequence, while keeping hardware and implementation details out of it. Two
designs were considered:

- a state machine that returns an effect list (`ENABLE_MOTOR`, `START_SEEK`, …)
  for the controller to execute;
- a state machine whose states represent activities, with entry and exit actions
  owned by the orchestrator.

The second is preferred: enabling a motor, setting microsteps or expressing an
offset in steps are implementation details. A different motor (for example a DC
motor with time-based offsets) should not force a change in the state machine or
its transitions. The same applies to protocol events: the state machine should
report what happened, not format wire messages.

## Decision

### Pure state machine

`StateMachine` is a pure C++11 class with no project includes beyond its own
vocabulary header. It defines `State`, `Mode`, `StopReason`, `Announcement`,
`Result` and `ErrorCode`. It has no clock, no config, no hardware interface and
no protocol type.

States represent activities that last over time:

```
BOOT, IDLE, SEEK, STOPPING, OFFSET, CAMERA, JOG, FAULT
```

`SEEK` is the search for the next notch. `STOPPING` is the controlled
deceleration after the notch. `OFFSET` applies the calibrated offset after the
stop. `CAMERA` is the focus/shutter sequence. `JOG` is a manual move.

`ALIGN` is removed: its hidden "waiting for stop, then applying the offset"
sub-phase is now the two explicit states `STOPPING` and `OFFSET`.

`Mode` remembers why the machine is active: `RUN`, `SINGLE` or `SHOOT`. It is
what allows `RUN` and `SINGLE` to share the frame states instead of duplicating
them. `JOG` is a state, not a mode.

### One method per event

There is no event enum. Each event is a method whose signature fits it:

```cpp
Result bootCompleted();
Result startContinuousRun();
Result scanOneFrame();
Result shootCurrentFrame();
Result jog();
Result stop();
Result abort();
Result notchDetected();
Result motionCompleted();
Result pictureTaken();
Result notchTimedOut();
Result driverFaultDetected();
Result buttonPressedShort();
Result buttonPressedLong();
```

Commands keep the protocol's verbs; notifications describe what happened.
`jog()` carries no payload; the requested amount is a controller concern.

### Result

```cpp
struct Result {
  State      state;
  ErrorCode  error;          // NONE if accepted
  StopReason reason;         // NONE unless this transition stopped something
  uint16_t   announcements;  // semantic facts, not actions
};
```

`announcements` is a bitmask of facts such as `RUN_STARTED`, `FRAME_DONE`,
`RUN_STOPPED`, `STEP_FAILED`, `FAULT_RAISED`. `FRAME_DONE` can co-occur with
`RUN_STOPPED` or `STEP_DONE`, which is why it is a bitmask. The orchestrator maps
bits to protocol `Event`s and fills values such as position and frame index.

There are no physical-effect bits. Physical actions are entry and exit actions.

### Orchestrator

`ScannerController` owns the entry/exit action table:

| Enter | Action |
|---|---|
| `SEEK` | `motion.startSeek()` |
| `STOPPING` | none (the stop is issued on exit from `SEEK`) |
| `OFFSET` | `motion.applyOffset()` |
| `CAMERA` | `camera.start()` |
| `JOG` | `motion.startJog(pendingAmount)` |
| `IDLE` / `FAULT` | `motion.idle()` |

| Exit | Action |
|---|---|
| `SEEK`, next `STOPPING` | `motion.stop()` |
| `SEEK`/`STOPPING`/`OFFSET`/`JOG`, reason ≠ NONE | `motion.halt()` |
| `CAMERA`, reason ≠ NONE | `camera.abort()` |

The controller also owns deadlines (notch timeout), command payloads (jog
amount), frame counters, the protocol event formatting and the status snapshot.
`lastError` and `stopRequested` live in the state machine because they are part
of its state; a run command clears `lastError`.

`motionCompleted()` carries the state's meaning: stop finished in `STOPPING`,
offset finished in `OFFSET`, jog finished in `JOG`, and seek budget exhausted in
`SEEK`.

### No action states, no second machine

Instantaneous actions (enable, set target, start the camera) are not states.
Making them states would add a mandatory acknowledgement call per command, a
stuck-state failure mode, and `STOP`/`ABORT`/timeout handling in every transient
state, without adding information.

A second state machine or a hierarchical machine is unnecessary: mode memory
covers the shared frame sequence, and `JOG` is the only genuinely different
activity.

## Consequences

Positive:

- The state machine is fully testable with plain values; no fakes, clock or
  config.
- The state graph is the single source of truth for the sequence.
- Hardware details stay in the motion and camera layers; a different motor
  changes only `IMotionController`.
- The controller has a small, explicit entry/exit table instead of a duplicated
  transition table.
- Protocol formatting has one home.

Negative:

- The orchestrator must compare the previous and next state to run entry/exit
  actions; it keeps one extra field.
- Exit actions depend on `reason`, so the controller must interpret it.
- Announcements are a second vocabulary next to protocol `EventType`; the
  mapping is a small, tested switch.

## Alternatives rejected

- **Effects bitmask.** Complete, but it puts motor-level actions back into the
  pure machine and makes the result carry implementation detail.
- **Action states with acknowledgements** (`START_SEEK`, `seekStarted()`).
  Explicit, but synchronous actions gain a handshake, a stuck-state risk and
  many extra transitions.
- **State and error only, controller infers effects.** Smallest result, but the
  controller would recreate the transition table.
- **Second state machine or hierarchical machine.** More machinery and shared
  ownership for no gain at this size.
