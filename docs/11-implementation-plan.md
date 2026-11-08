# 11 — Implementation plan

This is the ordered work plan for the implementing agent. Each milestone is
small, testable, and leaves the repository buildable. Follow the order; do not
start the HAL before the core is tested.

The order is **core-first**: after setup and the shared types, implement the
small collaborators that the state machine needs, then the state machine itself.
The serial parser, `ConfigManager`, status LED and Arduino adapters come later.
The state machine is where specification ambiguities surface, so it is
deliberately early.

## 0. Ground rules

- The docs are the source of truth.
- Use TDD: **red–green–refactor** for every behavior. Write the failing native
  test first, then the minimum code, then refactor. Never batch tests at the end
  of a milestone.
- Every new public method in `scanner_core` and every error path must have a
  direct unit test.
- Test conventions (one case per `TEST`/`TEST_F`/`TEST_P`, parameterized values,
  assertion choice) are defined in
  [09-testing.md §1](09-testing.md#1-strategy). Do not restate or vary them.
- Keep `lib/scanner_core/` free of `Arduino.h`.
- Do not extend the prototype in `src/main.cpp`; replace it.
- Do not inject `ConfigManager` into a component. Components take only the
  config slice they need; see [ADR-0005](adr/0005-config-slices-and-dependency-direction.md).
- Run `pio test -e native` after every milestone.
- Run `pio run -e uno` after every milestone that touches the HAL.

## 1. Milestone 0 — Project setup

### Tasks

1. Update `platformio.ini` to two environments:
   - `uno`: ATmega328P/Uno with the Arduino framework, 115200 monitor, and
     library dependencies `teemuatlut/TMCStepper@0.7.3` and
     `waspinator/AccelStepper@1.64`. Build as C++11 with exceptions and RTTI
     disabled, `-Wall -Wextra`, and defines for firmware version, protocol
     version and default log level.
   - `native`: PlatformIO's `native` platform with GoogleTest, the
     `lib/scanner_core/include` include path, and no `src/` sources. The host
     test build may use C++17; core sources stay C++11-compatible.
2. Remove the Janelia TMC2209 dependency and the obsolete `BAUD_RATE` flag.
3. Create the directory structure from
   [03-architecture.md §11](03-architecture.md#11-file-layout).
4. Add a PlatformIO library manifest for `scanner_core` pointing at
   `include/` and `src/`.
5. Provide the single GoogleTest entry point in `test/test_native/`:
   `InitGoogleTest` followed by `RUN_ALL_TESTS`.
6. Delete `src/main.hpp`; it belongs to the old prototype.
7. Leave `src/main.cpp` as a placeholder until Milestone 6; make sure the
   `native` environment does not compile it.

### Acceptance

- `pio run -e uno` compiles (the prototype may still be there).
- `pio test -e native` builds GoogleTest and runs the smoke test.

## 2. Milestone 1 — Types, config slices, interfaces, Logger

### Files

- `Types.h/.cpp` — enums, `ConfigChange`, `StatusSnapshot`, `Event`, and
  `toString`.
- `Config.h/.cpp` — `InputConfig`, `CameraConfig`, `MotionConfig`, `RunConfig`,
  `DriverConfig`, the aggregate `Config`, defaults, ranges, validation,
  `PersistedConfig`, CRC16-CCITT.
- The core interfaces from [03-architecture.md §4](03-architecture.md#4-interfaces).
- `Logger.h/.cpp` — level filtering, fixed-buffer formatting.
- Fakes for clock, input, output and the recording sinks; tests for config and
  logger.

### Tests

- Config defaults and every range boundary.
- `microsteps` allowed values and rejection of anything else.
- Cross-field validation.
- Persistence round-trip, CRC, and corrupt or mismatched header data.
- Derived values used by the rest of the system.
- Logger level filtering and buffer truncation.

### Acceptance

- `pio test -e native` passes.
- No Arduino include in any core header.
- `Config` is a plain aggregate of the five slices; no component interface
  mentions `ConfigManager`.

## 3. Milestone 2 — Core collaborators

These four small classes are the state machine's test harness. Each one takes a
narrow config slice and hardware interfaces, never `ConfigManager`.

If the review feels large, split it into 2a (`NotchSensor`, `Button`) and 2b
(`Camera`, `MotionController`); the order inside the milestone does not matter.

### Files

- `NotchSensor.h/.cpp` — `IInput&`, `const InputConfig&`.
- `Button.h/.cpp` — `IInput&`, `const InputConfig&`.
- `Camera.h/.cpp` — two `IOutput&`, `const CameraConfig&`.
- `MotionController.h/.cpp` — `IStepperMotor&`, `const MotionConfig&`.
- Fakes for the stepper motor, simulated motion, camera outputs, and input
  behavior; one test file per collaborator.

### Tests

Cover the behavior defined in [03-architecture.md §7.1–7.4](03-architecture.md#71-notchsensor):

- Debounce and one-shot edges, and live `InputConfig` changes.
- Short press on release, long press, boot-held input.
- Camera phase order and timing, abort in each phase, and the active shutter
  pulse; live `CameraConfig` changes.
- Motion pass-through, poll, stop/halt, enable/disable, position; live
  `MotionConfig` changes.
- Simulated frame motion at the default speed and acceleration.

### Acceptance

- All collaborator tests pass.
- The simulated frame motion is within ±5 % of 3.43 s.
- No collaborator includes `ConfigManager.h`.
- No timing uses real time.

## 4. Milestone 3 — State machine, happy path

This is the first core milestone. Implement only the normal path; leave stop,
abort, timeout, fault, button and event edge cases for Milestone 4.

### Files

- `ScannerController.h/.cpp`.
- The state-machine test file (happy-path tests first).
- Extend fakes as needed.

### Tests

Follow the transition table in
[03-architecture.md §6.1](03-architecture.md#61-transitions) and cover:

- Start from IDLE and FAULT; busy otherwise.
- Seek → notch → align → camera → next seek.
- Offset move (positive, negative, zero).
- Frame counting and events.
- `STEP` and `SHOOT` complete and return to `IDLE`.
- Periodic status snapshot.

### Acceptance

- The happy-path transitions pass.
- Every event emitted on the happy path is asserted at least once.
- `pio test -e native` passes.
- `ScannerController` takes `RunConfig` and component references; it does not
  know `ConfigManager`.

## 5. Milestone 4 — State machine, edge cases

Complete the state machine. This is the highest-risk milestone; test every
transition and every stop reason.

### Files

- `ScannerController.h/.cpp` (continued).
- The state-machine test file (edge cases).
- Extend fakes as needed.

### Tests

Use the same transition table in
[03-architecture.md §6.1](03-architecture.md#61-transitions) and cover:

- Graceful stop during RUN (frame completes).
- Immediate stop during SINGLE, JOG and SHOOT.
- Abort in every active state.
- Abort during a shutter pulse (pulse completes).
- Notch timeout in RUN and SINGLE.
- JOG never times out.
- `maxSeekSteps` → `MOTION_TIMEOUT`.
- Button short/long in every state, boot-held.
- Driver fault → FAULT; `START`/`STOP` recovery.
- Enable/disable commands and the `ENABLE` override.

### Acceptance

- Every transition has a test.
- Every event is asserted at least once.
- `pio test -e native` passes.

## 6. Milestone 5 — CommandParser and ConfigManager

The serial-facing and persistence-facing layer. It depends on the core, not the
other way round.

### Files

- `CommandParser.h/.cpp`.
- `ConfigManager.h/.cpp`.
- A fake config store, and one test file per class.

### Tests

- Parser grammar: case-insensitivity, whitespace, CRLF, overflow, argument
  count, overlong lines, and unknown commands. See
  [05-serial-protocol.md](05-serial-protocol.md).
- `ConfigManager`: load valid/empty/corrupt, `get`/`set` by key, read-only,
  unknown, out of range.
- `SET` returns the right `ConfigSetResult` (`error`; `change` is `DRIVER`,
  `LOG_LEVEL` or `NONE`).
- `save`, `reset`; reset mutates the existing `Config` in place.
- Microsteps change logs the offset warning.

### Acceptance

- All tests pass.
- `ConfigManager` never touches EEPROM directly; it uses `IConfigStore`.
- `ConfigManager` has no component dependencies; only `main.cpp` and
  `CommandHandler` will know it.
- `ConfigManager::set()` has no state dependency; `SET` state checking belongs to
  `CommandHandler` in Milestone 6.

## 7. Milestone 6 — StatusLed, HAL adapters, composition root

### Files

- `StatusLed.h/.cpp`; a test file for it.
- `src/hal/`: `ArduinoClock`, `ArduinoInput` (active-low, `INPUT_PULLUP`),
  `ArduinoOutput`, `AccelStepperMotor` (STEP/DIR, EN active-low),
  `TmcStepperDriver` (TMCStepper 0.7.3, SoftwareSerial), `EepromConfigStore`
  (packed struct + CRC), `SerialLogSink`, `SerialEventSink`,
  `SerialStatusSink`, and `SerialCommandHandler`.
- `src/main.cpp` composition root.

### Adapter rules

- `AccelStepperMotor` owns STEP/DIR pulse generation and drives `EN`
  active-low. It issues no pulses while disabled.
- `TmcStepperDriver` configures the driver only. The register sequence is in
  [07-stepper-tmc2209-guide.md §10.4](07-stepper-tmc2209-guide.md#104-what-the-firmware-writes).
  It reports a fault only when UART readback is available.
- `EepromConfigStore` stores the packed struct with CRC16-CCITT and verifies the
  readback after a save.
- `SerialLogSink` writes `LOG <LEVEL> <message>` and drops a message rather than
  block when the transmit buffer is too full.
- `SerialEventSink` buffers events in a small FIFO and writes `EVT` lines in the
  order defined by [05-serial-protocol.md §5](05-serial-protocol.md#5-events).
  The main loop flushes it so a command response precedes the events it caused
  (see [03-architecture.md §9.1](03-architecture.md#91-responseevent-ordering)).
- `SerialStatusSink` prints one `STATUS` line from a `StatusSnapshot`. The
  explicit `STATUS` command reuses that formatter and then prints `OK STATUS`.
- `SerialCommandHandler` reads one byte per loop, enforces the 63-character line
  limit, prints `OK`/`ERR`/`CFG`/`STATUS`, and applies `ConfigChange::DRIVER` and
  `ConfigChange::LOG_LEVEL`. It never flushes the event FIFO itself.

### Acceptance

- `pio run -e uno` compiles.
- The flash/RAM usage is reported; it must fit with margin.
- No blocking serial loops; one byte per loop.
- Only `main.cpp` and `SerialCommandHandler` know `ConfigManager`.
- `SerialCommandHandler` rejects `SET` outside `IDLE` with `ERR BUSY`.

## 8. Milestone 7 — Bring-up

### Tasks

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

- All steps in [09-testing.md §8.1–8.6](09-testing.md#8-hardware-acceptance-checklist)
  pass or have an understood explanation.

## 9. Milestone 8 — Calibration

Follow [08-calibration.md](08-calibration.md) completely.

### Acceptance

- Current, direction, speed, offset, camera timing, button, timeout all verified.
- Machine-specific values recorded.
- Offset repeatable within ±1 microstep over three frames.

## 10. Milestone 9 — Acceptance and handover

### Tasks

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

## 11. Suggested commit sequence

1. `build: switch to TMCStepper + AccelStepper + GoogleTest`
2. `feat(core): types, config slices, interfaces, logger`
3. `feat(core): notch sensor, button, camera, motion controller`
4. `feat(core): state machine happy path`
5. `feat(core): state machine edge cases and events`
6. `feat(core): command parser and config manager`
7. `feat(hal): status LED, arduino adapters and composition root`
8. `feat(proto): serial command handler`
9. `docs: calibration and acceptance results`

## 12. Risks and mitigations

| Risk | Mitigation |
|---|---|
| SoftwareSerial unreliable at 115200 | TX-only is enough; lower baud if readback fails; fall back to write-only |
| AccelStepper step-rate limit | Default 400 µsteps/s is 10× below the limit |
| Flash/RAM overflow | Measure after Milestone 6; remove DEBUG strings / lower logging if needed |
| Step loss under load | Conservative speed/accel; raise current in small steps; check mechanics |
| Offset drift | Recalibrate after any speed/accel/microstep change; sensor re-references each frame |
| Camera interface differences | Optocouplers + configurable timings; verify with `SHOOT` |
| UART address/baud | MS1/MS2 = 0, auto-baud, one-wire fallback |
| Watchdog reset loop | Clear `MCUSR`, `wdt_disable()` early, enable after init |
| EEPROM wear | Explicit `SAVE` only; no auto-save |
| Loop blocking from logs | Log at transitions only; sink drops when the TX buffer is full |
| Button bounce / boot-held | 20 ms debounce; ignore until released at boot |
| ConfigManager becomes a god object | Config slices + ADR-0005; no component includes it |
| State-machine assumptions drift | Freeze the collaborator contracts in 03 §7.1–7.4 before Milestone 3 |

## 13. Definition of done (repeated)

- All native tests pass.
- All hardware acceptance criteria pass.
- Calibration values recorded.
- Docs updated to match the implementation.
- No project warnings in `pio run -e uno`.
- The prototype `src/main.cpp` has been fully replaced.
