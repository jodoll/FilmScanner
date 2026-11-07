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
- Keep `lib/scanner_core/` free of `Arduino.h`.
- Do not extend the prototype in `src/main.cpp`; replace it.
- Do not inject `ConfigManager` into a component. Components take only the
  config slice they need; see [ADR-0005](adr/0005-config-slices-and-dependency-direction.md).
- Run `pio test -e native` after every milestone.
- Run `pio run -e uno` after every milestone that touches the HAL.

## 1. Milestone 0 — Project setup

### Tasks

1. Replace `platformio.ini` with:

```ini
[platformio]
default_envs = uno

[env:uno]
platform = atmelavr
board = uno
framework = arduino
monitor_speed = 115200
lib_deps =
    teemuatlut/TMCStepper@0.7.3
    waspinator/AccelStepper@1.64
build_flags =
    -D VERSION=\"0.1.0\"
    -D PROTOCOL_VERSION=1
    -D LOG_LEVEL=2
    -std=gnu++11
    -fno-exceptions
    -fno-rtti
    -Wall
    -Wextra

[env:native]
platform = native
test_framework = unity
build_flags =
    -std=gnu++11
    -D UNIT_TEST
    -I lib/scanner_core/include
```

2. Create the directory structure from
   [03-architecture.md §11](03-architecture.md#11-file-layout).
3. Add `lib/scanner_core/library.json`:

```json
{
  "name": "scanner_core",
  "version": "0.1.0",
  "description": "Pure logic for the film scanner",
  "build": {
    "srcDir": "src",
    "includeDir": "include"
  }
}
```

4. Remove the Janelia TMC2209 dependency and the old `BAUD_RATE` build flag.
5. Delete `src/main.hpp`; it belongs to the old prototype.
6. Leave `src/main.cpp` as a placeholder until Milestone 6; make sure the
   `native` environment does not compile it (`test_build_src` stays at its
   default `false`).

### Acceptance

- `pio run -e uno` compiles (the prototype may still be there).
- `pio test -e native` runs and reports "No tests" or a trivial passing test.

## 2. Milestone 1 — Types, config slices, interfaces, Logger

### Files

- `Types.h/.cpp` — enums, `ConfigChange`, `StatusSnapshot`, `Event`, and
  `toString`.
- `Config.h/.cpp` — `InputConfig`, `CameraConfig`, `MotionConfig`, `RunConfig`,
  `DriverConfig`, the aggregate `Config`, defaults, ranges, validation,
  `PersistedConfig`, CRC16-CCITT.
- `IClock.h`, `IInput.h`, `IOutput.h`, `IStepperMotor.h`, `ITmcDriver.h`,
  `IConfigStore.h`, `ILogSink.h`, `IEventSink.h`, `IStatusSink.h`.
- `Logger.h/.cpp` — level filtering, fixed 96-byte formatting.
- `test/fakes/FakeClock.h`, `FakeInput.h`, `FakeOutput.h`,
  `RecordingLogSink.h`, `RecordingEventSink.h`, `RecordingStatusSink.h`.
- `test/test_native/test_config.cpp`, `test_logger.cpp`.

### Tests

- Defaults validate.
- Every writable key at min/max and out of range.
- `microsteps` power-of-two check.
- Cross-field check (`longPressMs > buttonDebounceMs`).
- Each config slice round-trips through `PersistedConfig`.
- CRC round trip, corrupt CRC, wrong magic/version/length.
- Derived values (`stepsPerMm`, `expectedFrameSteps`).
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
- `test_native/test_notch_sensor.cpp`, `test_button.cpp`, `test_camera.cpp`,
  `test_motion_controller.cpp`.
- `FakeStepperMotor.h`, `SimulatedStepperMotor.h`, `FakeCameraOutputs.h`,
  `FakeInput` enhancements.

### Tests

- Use the tables in [09-testing.md §4.3–4.6](09-testing.md#43-notchsensor).
- Debounce and one-shot edges; live `InputConfig` changes.
- Short press on release, long press at 2 s, boot-held behavior.
- Camera phase timings and abort behavior, including the active shutter pulse;
  live `CameraConfig` changes.
- Motion pass-through, poll, stop/halt, enable/disable, position, live
  `MotionConfig` changes.
- Timing test: 38 mm at 400 µsteps/s and 2,000 µsteps/s² ≈ 3.43 s.

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
- `test_native/test_state_machine.cpp` (happy-path tests first).
- Extend fakes as needed (`FakeTmcDriver`, `RecordingEventSink` helpers).

### Tests

Use the table in [09-testing.md §4.9](09-testing.md#49-scannercontroller--state-machine)
and the transition table in
[03-architecture.md §6.1](03-architecture.md#61-transitions). Cover at least:

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
- `test_native/test_state_machine.cpp` (edge cases).
- Extend fakes as needed.

### Tests

Use the same table in [09-testing.md §4.9](09-testing.md#49-scannercontroller--state-machine)
and cover at least:

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
- `FakeConfigStore.h`.
- `test_native/test_command_parser.cpp`, `test_config_manager.cpp`.

### Tests

- Parser: use the table in
  [09-testing.md §4.2](09-testing.md#42-commandparser).
  Case-insensitivity, whitespace, CRLF, overflow, extra args.
- `ConfigManager`: load valid/empty/corrupt, `get`/`set` by key, read-only,
  unknown, out of range.
- `SET` returns the right `ConfigSetResult` (`error`; `change` is `DRIVER`,
  `LOG_LEVEL` or `NONE`).
- `save`, `reset`; reset mutates the existing `Config` in place.
- Microsteps change logs the offset warning.
- No component receives `ConfigManager`.

### Acceptance

- All tests pass.
- `ConfigManager` never touches EEPROM directly; it uses `IConfigStore`.
- `ConfigManager` has no component dependencies; only `main.cpp` and
  `CommandHandler` will know it.
- `ConfigManager::set()` has no state dependency; `SET` state checking belongs to
  `CommandHandler` in Milestone 6.

## 7. Milestone 6 — StatusLed, HAL adapters, composition root

### Files

- `StatusLed.h/.cpp`; `test_native/test_status_led.cpp`.
- `src/hal/ArduinoClock.h/.cpp`.
- `src/hal/ArduinoInput.h/.cpp` (active-low, `INPUT_PULLUP`).
- `src/hal/ArduinoOutput.h/.cpp`.
- `src/hal/AccelStepperMotor.h/.cpp` (STEP/DIR, EN active-low).
- `src/hal/TmcStepperDriver.h/.cpp` (TMCStepper 0.7.3, SoftwareSerial).
- `src/hal/EepromConfigStore.h/.cpp` (packed struct + CRC).
- `src/hal/SerialLogSink.h/.cpp`.
- `src/hal/SerialEventSink.h/.cpp` (8-entry FIFO + `flush()`).
- `src/hal/SerialStatusSink.h/.cpp`.
- `src/hal/SerialCommandHandler.h/.cpp` (line assembly, parser dispatch,
  `ConfigChange` apply).
- `src/main.cpp` composition root.

### Adapter rules

- `AccelStepperMotor`:
  - Construct `AccelStepper(AccelStepper::DRIVER, stepPin, dirPin)`.
  - `enable()` → `digitalWrite(EN, LOW)`, `disable()` → `HIGH`.
  - `run()` → `stepper.run()` only when enabled.
  - `halt()` → `stepper.setSpeed(0)` / stop issuing pulses.
- `TmcStepperDriver`:
  - `TMC2209Stepper driver(&softSerial, 0.11, 0)`.
  - `begin()`: `driver.begin()`, `test_connection()`.
  - `apply(const DriverConfig&)`: the register sequence in
    [07-stepper-tmc2209-guide.md §10.4](07-stepper-tmc2209-guide.md#104-what-the-firmware-writes).
  - `hasFault()`: `driver.driver_error()` if readback, else false.
- `EepromConfigStore`:
  - Use `EEPROM.get/put` with the packed struct.
  - CRC16-CCITT.
  - Verify readback on save.
- `SerialLogSink`:
  - `LOG <LEVEL> <message>\n`.
  - Drop the message if `Serial.availableForWrite()` is below the line length.
- `SerialEventSink`:
  - Format `EVT` lines exactly as in
    [05-serial-protocol.md §5](05-serial-protocol.md#5-events).
  - Buffer events in an 8-entry FIFO; `flush()` writes them in order.
  - `loop()` calls `flush()` after `controller.tick()` and before
    `commandHandler.poll()`, so a command response is written before the events
    it caused (see [03-architecture.md §9.1](03-architecture.md#91-responseevent-ordering)).
- `SerialStatusSink`:
  - Print one `STATUS` line from a `StatusSnapshot`, no trailing `OK STATUS`.
  - The explicit `STATUS` command calls `write(snapshot())` and then prints
    `OK STATUS`, reusing the formatter.
- `SerialCommandHandler`:
  - Read one byte per loop.
  - Max 63 characters; discard overlong lines.
  - Dispatch and print `OK`/`ERR`/`CFG`/`STATUS`.
  - Apply `ConfigChange::DRIVER` via `tmc.apply(config.driver)` and
    `ConfigChange::LOG_LEVEL` via `logger.setLevel(config.logLevel)`.
  - Never flush the event FIFO itself; the main loop owns that.

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

1. `build: switch to TMCStepper + AccelStepper + native tests`
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
