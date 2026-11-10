# 11 — Implementation plan

This is the ordered work plan for the implementing agent. The order is
**core-first** and **class by class**: the pure state machine first, then one
collaborator at a time, and only then the configuration, serial and HAL layers.

## 0. How to work

- One failing test at a time: write the test, run it, confirm it fails for the
  right reason, write the minimum code, refactor, commit. Never batch tests at
  the end of a milestone.
- Each step below is a behavior or a small class, not a test list. The test
  conventions are in [09-testing.md §1](09-testing.md#1-strategy); follow them.
- Review after each step. A milestone is done when its acceptance line passes
  and its behavior has direct tests.
- The docs are the source of truth for behavior; the headers are the source of
  truth for declarations. Do not edit `docs/` without explicit approval. If a
  behavior is missing or ambiguous, propose the doc change and wait.
- Keep `lib/scanner_core/` free of `Arduino.h`.
- Do not extend the prototype in `src/main.cpp`; replace it.
- Do not inject `ConfigManager` into a component. Each consumer defines and
  stores its own config slice; the manager pushes.
- The contract headers are already in place. Treat their declarations as the
  agreed shape: change one only when a test or a doc change requires it, and
  never copy the declaration back into `docs/`.
- Run `pio test -e native` after every step.
- Run `pio run -e uno` after every step that touches the HAL.

## 1. Milestone 0 — Project setup

Already in place: `platformio.ini` has the `uno` and `native` environments with
the agreed libraries; `lib/scanner_core/` has its manifest and include tree with
the contract headers; `test/test_native/` has the GoogleTest entry point and a
smoke test.

### Remaining

1. Replace the prototype `src/main.cpp` at the HAL milestone. Do not extend it.
2. Keep the `native` environment free of `src/` sources.

### Acceptance

- `pio run -e uno` compiles.
- `pio test -e native` builds GoogleTest and runs the smoke test.

## 2. Milestone 1 — Pure state machine

`StateMachine.h` already declares the vocabulary, `Result` and the class. This
milestone implements
[03-architecture.md §6](03-architecture.md#6-state-machine) completely. It has
no collaborators.

### Files

- `StateMachine.cpp` — the transition logic.
- A state-machine test file.

### What to cover

- `BOOT` → `IDLE`.
- `startContinuousRun`, `scanOneFrame`, `shootCurrentFrame`, `jog` and the
  `BUSY`/invalid-state rejections.
- `SEEK` → `STOPPING` → `OFFSET` → `CAMERA` and the frame outcomes for `RUN`,
  `SINGLE` and `SHOOT`.
- `JOG` completion and stop.
- Graceful stop vs. abort, including the `stopRequested` latch.
- Notch timeout and seek-budget exhaustion, in both `RUN` and `SINGLE`.
- Driver fault and recovery from `FAULT`.
- Button short/long mapping.
- Announcement bits, stop reasons and error clearing.

### Acceptance

- The state machine is fully tested with plain values: no fakes, no clock, no
  config.
- `StateMachine.h` includes no project header.
- No `ALIGN` state remains.

## 3. Milestone 2 — Motion seam

### Files

- `IMotionController.h`, `MotionConfig.h`.
- `StepperMotionController.h/.cpp`.
- `FakeStepperMotor.h`, `FakeClock.h`, `FakeMotionController.h`.
- A test file for `StepperMotionController`.

### What to cover

- `startSeek`, `applyOffset` and `startJog` produce the right motor setup and
  move.
- Any move clears the maintenance override and enables the driver.
- `idle()` de-energizes unless the override is set.
- `enable()` / `disable()` set and clear the override.
- `stop()` decelerates; `halt()` stops immediately and clears the override.
- `poll()`, `isMoving()`, `position()`, `applyConfig`.

### Acceptance

- `StepperMotionController` is tested against `FakeStepperMotor`; no real time.
- The class knows nothing about `ScannerController` or `ConfigManager`.

## 4. Milestone 3 — Notch sensor

### Files

- `NotchConfig.h`, `NotchSensor.h/.cpp`, a test file.

### What to cover

- Debounce, one-shot edges, initial state, and a config push that restarts the
  window without generating an event.

### Acceptance

- No real time; `FakeClock` and `FakeInput`.
- `NotchSensor` owns `NotchConfig` and has `applyConfig`.

## 5. Milestone 4 — Camera

### Files

- `CameraConfig.h`, `Camera.h/.cpp`, `FakeOutput.h`, a test file.

### What to cover

- Phase order and timing.
- Abort in each phase; an active shutter pulse completes.
- A config push applies to the next sequence only.

### Acceptance

- No real time; `FakeClock` and `FakeOutput`.

## 6. Milestone 5 — Button

### Files

- `ButtonConfig.h`, `Button.h/.cpp`, a test file.

### What to cover

- Debounce, short press on release, long press at the threshold, boot-held input,
  and a config push.

### Acceptance

- No real time; `FakeClock` and `FakeInput`.

## 7. Milestone 6 — ScannerController

Now the orchestrator. It owns the entry/exit action table and uses the pure
state machine plus the collaborators built above. The small collaborators come
first because the controller composes them; that keeps the controller tests
against real notch/camera/button behavior and only fakes the motion seam.

### Files

- `RunConfig.h`, `ScannerController.h/.cpp`, a test file.
- `FakeTmcDriver.h`, `RecordingEventSink.h`, `RecordingStatusSink.h` if not
  already present.

### What to cover

- `begin()` and the initial `IDLE` entry action.
- The entry/exit actions in
  [03-architecture.md §6.3](03-architecture.md#63-entry-and-exit-actions).
- Announcement-to-`Event` mapping, including values (frame index, position,
  reason, steps).
- Frame counters, run reset and the SINGLE index.
- Notch deadline and the `notchTimedOut()` call.
- `scanOneFrame` / `shootCurrentFrame` / `jog` payload handling.
- Maintenance `enable` / `disable` and `MOTOR_STATE`.
- `snapshot()`.
- `applyConfig`.

### Acceptance

- `ScannerController` is tested with `FakeMotionController` and the real
  `NotchSensor` / `Button` / `Camera` over fakes.
- It contains no hardware detail and no transition table of its own.
- It does not know `ConfigManager`.

## 8. Milestone 7 — Config aggregate, persistence and push

The consumer slices already exist. This milestone adds `LogConfig`, the
aggregate, validation, persistence and the pushes.

### Files

- `Config.h/.cpp` — aggregate, defaults, ranges, validation.
- `ConfigRecord.h/.cpp` — the record, its header and checksum, and the mapping
  to and from `Config`.
- `ConfigManager.h/.cpp`, `LogConfig.h`, `Logger.h/.cpp`.
- `FakeNonVolatileStore.h`, `RecordingLogSink.h`, tests for config, record,
  manager and logger.

### What to cover

- Defaults and every range boundary.
- Cross-field rules.
- Record round-trip; rejection on a wrong magic, version, length or checksum;
  rejection on an out-of-range value; save verified by read-back.
- `set()` validates, mutates and pushes exactly one consumer; rejection changes
  nothing.
- `begin()` / `reset()` push all slices.
- `save()` builds the record from the aggregate, never from component state.
- `get()` / `CONFIG` and read-only keys.
- The offset-recalibration warning.
- Logger level push and buffer handling.

### Acceptance

- `ConfigManager` is the only owner of the aggregate.
- Components never include `ConfigManager.h`.
- `Config` does not know it is persisted; the record is its own type.

## 9. Milestone 8 — CommandParser

### Files

- `CommandParser.h/.cpp`, a test file.

### What to cover

- Case, whitespace, CRLF, argument count, overflow, overlong lines, unknown
  commands. See [05-serial-protocol.md](05-serial-protocol.md).

### Acceptance

- Pure function; no Arduino.

## 10. Milestone 9 — StatusLed

### Files

- `StatusLed.h/.cpp`, a test file.

### What to cover

- `BOOT`, `IDLE`, active states (including `STOPPING` and `OFFSET`) and `FAULT`.

### Acceptance

- No real time; `FakeClock` and `FakeOutput`.

## 11. Milestone 10 — HAL adapters and composition root

### Files

- `src/hal/ArduinoClock`, `ArduinoInput` (active-low, `INPUT_PULLUP`),
  `ArduinoOutput`, `AccelStepperMotor` (STEP/DIR, EN active-low),
  `TmcStepperDriver` (TMCStepper 0.7.3, SoftwareSerial),
  `EepromStore` (byte block at address 0), `SerialLogSink`,
  `SerialEventSink`, `SerialStatusSink`, `SerialCommandHandler`.
- `src/main.cpp` composition root.

### Adapter rules

- `AccelStepperMotor` owns STEP/DIR pulse generation and drives `EN`
  active-low. It issues no pulses while disabled.
- `TmcStepperDriver` configures the driver only; the register sequence is in
  [07-stepper-tmc2209-guide.md §10.4](07-stepper-tmc2209-guide.md#104-what-the-firmware-writes).
  It implements `applyConfig` for `DriverConfig`.
- `EepromStore` reads and writes the record bytes at address 0. It knows nothing
  about `Config` or `ConfigRecord`; the manager builds and validates the record.
- `SerialLogSink` writes `LOG <LEVEL> <message>` and drops a message rather than
  block.
- `SerialEventSink` buffers events in a small FIFO and writes `EVT` lines in the
  order defined by [05-serial-protocol.md §5](05-serial-protocol.md#5-events).
  The main loop flushes it so a command response precedes the events it caused
  (see [03-architecture.md §9.1](03-architecture.md#91-responseevent-ordering)).
- `SerialStatusSink` prints one `STATUS` line from a `StatusSnapshot`.
- `SerialCommandHandler` reads one byte per loop, enforces the 63-character
  limit, prints responses, maps commands to `ScannerController`/`ConfigManager`,
  emits `CONFIG_SAVED`/`CONFIG_RESET`, and never flushes the event FIFO. It does
  not re-apply config on `SET`.

### Composition order

The root creates the components with defaults, creates `ConfigManager`, calls
`tmc.begin()`, then `configManager.begin()` (which pushes every slice), then
`controller.begin()`. See
[03-architecture.md §12](03-architecture.md#12-composition-root).

### Acceptance

- `pio run -e uno` compiles; flash/RAM fit with margin.
- No blocking serial loops; one byte per loop.
- Only `main.cpp` and `SerialCommandHandler` know `ConfigManager`.
- `SerialCommandHandler` rejects `SET` outside `IDLE` with `ERR BUSY`.

## 12. Milestone 11 — Bring-up

1. Flash the Uno.
2. Open the monitor at 115200.
3. Confirm `EVT READY`, LED, `PING`, `VERSION`, `STATUS`, `CONFIG`.
4. Run `ENABLE`/`DISABLE`, `JOG 100`, `JOG -100`.
5. Run `SHOOT`.
6. Run `STEP`.
7. Run `START` on a 4-frame holder; `STOP`; `ABORT`.
8. Test the button short/long and boot-held.
9. Test the notch timeout.

### Acceptance

- All steps in
  [09-testing.md §8](09-testing.md#8-hardware-acceptance-checklist) pass or
  have an understood explanation.

## 13. Milestone 12 — Calibration

Follow [08-calibration.md](08-calibration.md) completely.

### Acceptance

- Current, direction, speed, offset, camera timing, button and timeout verified.
- Machine-specific values recorded.
- Offset repeatable within ±1 microstep over three frames.

## 14. Milestone 13 — Acceptance and handover

1. Run the full checklist in
   [09-testing.md §8](09-testing.md#8-hardware-acceptance-checklist) and §9.
2. Run a 36-frame film; confirm < 10 minutes and no lost frames.
3. Check motor/driver temperature.
4. Update the docs if anything changed.
5. Record the final `CONFIG` output in the machine log.

### Acceptance

- All acceptance criteria in
  [01-requirements.md §9](01-requirements.md#9-acceptance-criteria) pass.
- `pio run -e uno` and `pio test -e native` pass on a clean checkout.

## 15. Risks and mitigations

| Risk | Mitigation |
|---|---|
| SoftwareSerial unreliable at 115200 | TX-only is enough; lower baud if readback fails; fall back to write-only |
| AccelStepper step-rate limit | Default 400 µsteps/s is 10× below the limit |
| Flash/RAM overflow | Measure after the HAL milestone; remove DEBUG strings / lower logging if needed |
| Step loss under load | Conservative speed/accel; raise current in small steps; check mechanics |
| Offset drift | Recalibrate after any speed/accel/microstep change; sensor re-references each frame |
| Camera interface differences | Optocouplers + configurable timings; verify with `SHOOT` |
| UART address/baud | MS1/MS2 = 0, auto-baud, one-wire fallback |
| Watchdog reset loop | Clear `MCUSR`, `wdt_disable()` early, enable after init |
| EEPROM wear | Explicit `SAVE` only; no auto-save |
| Loop blocking from logs | Log at transitions only; sink drops when the TX buffer is full |
| Button bounce / boot-held | Debounce; ignore until released at boot |
| Config cache diverges from aggregate | Push only, never read back, never clamp in `applyConfig` |
| State-machine assumptions drift | The pure machine's transition tests are the contract; the orchestrator never re-implements them |
| `OFFSET` with zero offset | The controller reports `motionCompleted()` immediately on entry |

## 16. Definition of done (repeated)

- All native tests pass.
- All hardware acceptance criteria pass.
- Calibration values recorded.
- Docs updated to match the implementation.
- No project warnings in `pio run -e uno`.
- The prototype `src/main.cpp` has been fully replaced.
