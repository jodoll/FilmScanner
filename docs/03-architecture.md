# 03 — Architecture

## 1. Goals

- Keep all decision logic independent of Arduino so it can be unit-tested on a
  PC.
- Keep the main loop non-blocking so step pulses and sensor polling stay
  responsive.
- Make every hardware interaction replaceable by a fake.
- Keep the code small enough for an Uno.

## 2. Layering

```
┌──────────────────────────────────────────────────────────────┐
│ src/  (Arduino only)                                         │
│  main.cpp            composition root                        │
│  hal/                ArduinoClock, ArduinoInput,             │
│                      ArduinoOutput, AccelStepperMotor,       │
│                      TmcStepperDriver, EepromConfigStore,    │
│                      SerialLogSink, SerialEventSink,         │
│                      SerialStatusSink, SerialCommandHandler   │
└───────────────────────────┬──────────────────────────────────┘
                            │ implements / uses
┌───────────────────────────▼──────────────────────────────────┐
│ lib/scanner_core/  (pure C++11, no Arduino.h)                │
│  Types, Config (narrow slices), ConfigManager                │
│  NotchSensor, Button, Camera, MotionController               │
│  ScannerController (state machine), StatusLed, Logger        │
│  CommandParser                                              │
│  interfaces: IClock, IInput, IOutput, IStepperMotor,         │
│              ITmcDriver, IConfigStore, ILogSink, IEventSink, │
│              IStatusSink                                     │
└──────────────────────────────────────────────────────────────┘
```

Dependency rule: arrows point **inward**. `scanner_core` never includes
`Arduino.h`, `TMCStepper.h`, `AccelStepper.h`, or `EEPROM.h`. Within the core,
components receive only a narrow `Config` slice, never `ConfigManager`; see
[ADR-0005](adr/0005-config-slices-and-dependency-direction.md).

## 3. Module responsibilities

| Module | Responsibility |
|---|---|
| `Types` | Enums (`State`, `Mode`, `ErrorCode`, `ButtonEvent`, `StopReason`, `EventType`) and `toString` helpers |
| `Config` | Narrow config slices (`InputConfig`, `CameraConfig`, `MotionConfig`, `RunConfig`, `DriverConfig`), the aggregate `Config`, defaults, validation, key names, CRC, persistence layout |
| `ConfigManager` | Own the single `Config`; load/save/reset, `get`/`set` by key, validation, persistence. No component dependencies |
| `NotchSensor` | Debounce the notch input, expose stable state and one-shot edges |
| `Button` | Debounce the control input, classify short/long press, handle boot-held contact |
| `Camera` | Focus/shutter pulse sequencer with settle and post-shot delays; abort completes an active pulse |
| `MotionController` | Wrap `IStepperMotor`: speed, acceleration, absolute/relative moves, poll, stop, halt |
| `ScannerController` | State machine: run, single, jog, shoot, stop, abort, fault, frame counting |
| `StatusLed` | LED patterns from `State` |
| `Logger` | Level filtering and fixed-buffer formatting |
| `CommandParser` | Parse one ASCII line into a `Command`; pure function |
| `CommandHandler` (HAL) | Dispatch parsed commands to `ScannerController` / `ConfigManager`, print responses, re-apply driver/log changes |
| HAL adapters | Thin wrappers around Arduino, AccelStepper, TMCStepper, EEPROM, Serial |

## 4. Interfaces

Exact signatures. All interfaces have a virtual destructor. All methods are
`noexcept` in spirit (no exceptions are used).

```cpp
// IClock.h
class IClock {
public:
  virtual ~IClock() = default;
  virtual uint32_t nowMs() const = 0;
};

// IInput.h — true means "active" (the pin reads LOW)
class IInput {
public:
  virtual ~IInput() = default;
  virtual bool read() const = 0;
};

// IOutput.h
class IOutput {
public:
  virtual ~IOutput() = default;
  virtual void write(bool active) = 0;
};

// IStepperMotor.h
class IStepperMotor {
public:
  virtual ~IStepperMotor() = default;
  virtual void enable() = 0;
  virtual void disable() = 0;
  virtual bool isEnabled() const = 0;
  virtual void setMaxSpeed(uint16_t stepsPerSecond) = 0;
  virtual void setAcceleration(uint16_t stepsPerSecond2) = 0;
  virtual void moveTo(int32_t absolutePosition) = 0;
  virtual void moveBy(int32_t steps) = 0;
  virtual void run() = 0;       // generate pulses as needed
  virtual void stop() = 0;      // decelerate to rest
  virtual void halt() = 0;      // stop issuing pulses immediately
  virtual int32_t position() const = 0;
  virtual bool isRunning() const = 0;
};

// ITmcDriver.h — configuration/diagnostics only
class ITmcDriver {
public:
  virtual ~ITmcDriver() = default;
  virtual bool begin() = 0;                  // true if UART readback works
  virtual void apply(const DriverConfig& config) = 0;
  virtual bool testConnection() = 0;
  virtual bool hasFault() = 0;               // false when readback unavailable
};

// IConfigStore.h
class IConfigStore {
public:
  virtual ~IConfigStore() = default;
  virtual bool load(Config& out) = 0;        // false = empty/invalid
  virtual bool save(const Config& in) = 0;
  virtual void clear() = 0;
};

// ILogSink.h
class ILogSink {
public:
  virtual ~ILogSink() = default;
  virtual void write(uint8_t level, const char* message) = 0;
};

// IEventSink.h
class IEventSink {
public:
  virtual ~IEventSink() = default;
  virtual void emit(const Event& event) = 0;
};

// IStatusSink.h — used for periodic STATUS lines only
class IStatusSink {
public:
  virtual ~IStatusSink() = default;
  virtual void write(const StatusSnapshot& status) = 0;
};
```

`StatusSnapshot` (in `Types.h`) carries exactly the fields of the `STATUS`
line: `state`, `mode`, `stopRequested`, `motorEnabled`, `camera`, `pos`,
`frames`, `runFrames`, `lastError`, `uptimeMs`. The explicit `STATUS` command
and the periodic status both use it.

`IStepperMotor::halt()` is used only for `ABORT` and timeouts. At the default
400 µsteps/s it is mechanically safe. `stop()` is the normal deceleration path.

## 5. Types

```cpp
enum class State : uint8_t {
  BOOT, IDLE, SEEK, ALIGN, CAMERA, JOG, FAULT
};

enum class Mode : uint8_t {
  NONE, RUN, SINGLE, SHOOT
};

enum class CameraPhase : uint8_t {
  IDLE, SETTLE, FOCUS, SHUTTER, POST, DONE
};

enum class ErrorCode : uint8_t {
  NONE,
  BAD_COMMAND,
  UNKNOWN_KEY,
  READ_ONLY,
  VALUE_OUT_OF_RANGE,
  BUSY,
  NOTCH_TIMEOUT,
  MOTION_TIMEOUT,
  DRIVER_FAULT,
  DRIVER_UART,
  EEPROM_INVALID,
  CONFIG_INVALID,
  INTERNAL
};

enum class ButtonEvent : uint8_t { NONE, SHORT_PRESS, LONG_PRESS };

enum class StopReason : uint8_t {
  NONE, REQUEST, ABORT, NOTCH_TIMEOUT, MOTION_TIMEOUT, ERROR
};

// What a successful ConfigManager::set() needs the caller to apply.
enum class ConfigChange : uint8_t {
  NONE, DRIVER, LOG_LEVEL
};

struct ConfigSetResult {
  ErrorCode error;       // NONE on success
  ConfigChange change;   // what the caller must apply
};

enum class EventType : uint8_t {
  READY,
  RUN_STARTED,
  RUN_STOPPED,
  FRAME_STARTED,
  FRAME_DONE,
  NOTCH,
  JOG_DONE,
  JOG_STOPPED,
  STEP_DONE,
  STEP_FAILED,
  STEP_STOPPED,
  SHOT_DONE,
  SHOT_STOPPED,
  FAULT,
  CONFIG_SAVED,
  CONFIG_RESET,
  MOTOR_STATE
};

struct Event {
  EventType type;
  uint8_t code;        // ErrorCode / StopReason / bool
  int32_t value;       // frame index, position, or steps
  int32_t value2;      // second value if needed
};

struct StatusSnapshot {
  State state;
  Mode mode;
  bool stopRequested;
  bool motorEnabled;
  CameraPhase camera;
  int32_t pos;
  uint32_t frames;      // total since boot
  uint32_t runFrames;   // current/last run
  ErrorCode lastError;
  uint32_t uptimeMs;
};

// Config.h — narrow slices, each owned by the one component that needs it.
struct InputConfig {
  uint16_t notchDebounceMs;
  uint16_t buttonDebounceMs;
  uint16_t longPressMs;
};

struct CameraConfig {
  uint16_t settleMs;
  uint16_t focusMs;
  uint16_t shutterMs;
  uint16_t postShotMs;
};

struct MotionConfig {
  uint16_t seekSpeed;
  uint16_t acceleration;
  uint16_t jogSpeed;
};

struct RunConfig {
  int32_t  offsetSteps;
  uint32_t maxSeekSteps;
  uint16_t notchTimeoutMs;
  uint16_t statusPeriodMs;
};

struct DriverConfig {
  uint16_t microsteps;
  uint16_t runCurrentMa;
  uint8_t  holdCurrentPercent;
  uint8_t  stealthChop;
};

struct Config {
  InputConfig  input;
  CameraConfig camera;
  MotionConfig motion;
  RunConfig    run;
  DriverConfig driver;
  uint8_t      logLevel;
};
```

## 6. State machine

States: `BOOT`, `IDLE`, `SEEK`, `ALIGN`, `CAMERA`, `JOG`, `FAULT`.
Camera sub-phases (inside `CAMERA`): `SETTLE`, `FOCUS`, `SHUTTER`, `POST`.
Modes: `NONE`, `RUN`, `SINGLE` (`STEP`), `SHOOT` (`SHOOT`).

```
                 START / button
   BOOT ──► IDLE ───────────────► SEEK ──notch──► ALIGN ──done──► CAMERA
              ▲                    │                                │
              │                    │ timeout                        │ done
              │                    ▼                                │
              └──────────────── IDLE ◄──────────────────────────────┘
                                   ▲
   FAULT ◄── error ────────────────┘
     │  START / button
     └──────────────► SEEK

   IDLE ──STEP──► SEEK (mode SINGLE) ──► ... ──► IDLE
   IDLE ──JOG───► JOG ──done/stop/abort──► IDLE
   IDLE ──SHOOT─► CAMERA (mode SHOOT) ──► IDLE
```

### 6.1 Transitions

| Current | Event | Guard | Next | Actions |
|---|---|---|---|---|
| `BOOT` | init complete | — | `IDLE` | disable motor, emit `READY` |
| `IDLE` | `START` | — | `SEEK` | clear error, enable motor, mode=RUN, runFrames=0, emit `RUN_STARTED` |
| `IDLE` | `STEP` | — | `SEEK` | enable motor, mode=SINGLE |
| `IDLE` | `JOG` | steps ≠ 0 | `JOG` | enable motor, moveBy(steps) |
| `IDLE` | `SHOOT` | — | `CAMERA` | mode=SHOOT, camera.start(), skip seek |
| `IDLE` | button short/long | — | `SEEK` | same as `START` |
| `IDLE` | `STOP` | — | `IDLE` | no-op, `OK` |
| `IDLE` | `ABORT` | — | `IDLE` | disable motor, clear enable override, `OK` |
| `IDLE` | `ENABLE` | — | `IDLE` | enable motor, set enable override, emit `MOTOR_STATE enabled=1` |
| `IDLE` | `DISABLE` | — | `IDLE` | disable motor, clear enable override, emit `MOTOR_STATE enabled=0` |
| `SEEK` | notch closed | — | `ALIGN` | motion.stop(), emit `NOTCH pos` |
| `SEEK` | timeout | mode=RUN | `IDLE` | motion.halt(), disable, error=NOTCH_TIMEOUT, emit `RUN_STOPPED reason=notch_timeout` |
| `SEEK` | timeout | mode=SINGLE | `IDLE` | motion.halt(), disable, emit `STEP_FAILED code=NOTCH_TIMEOUT` |
| `SEEK` | `maxSeekSteps` reached | >0, mode=RUN | `IDLE` | motion.halt(), disable, error=MOTION_TIMEOUT, emit `RUN_STOPPED reason=motion_timeout` |
| `SEEK` | `maxSeekSteps` reached | >0, mode=SINGLE | `IDLE` | motion.halt(), disable, emit `STEP_FAILED code=MOTION_TIMEOUT` |
| `ALIGN` | motion stopped | offset ≠ 0 | `ALIGN` | moveBy(offset), start offset timer |
| `ALIGN` | offset done | — | `CAMERA` | emit `FRAME_STARTED`, camera.start() |
| `CAMERA` | camera done | mode=RUN | `SEEK` | frames++, runFrames++, emit `FRAME_DONE`; if stopRequested → `IDLE` + `RUN_STOPPED reason=request frames=<runFrames>` |
| `CAMERA` | camera done | mode=SINGLE | `IDLE` | frames++, emit `FRAME_DONE` and `STEP_DONE` |
| `CAMERA` | camera done | mode=SHOOT | `IDLE` | emit `SHOT_DONE` |
| `CAMERA` | `STOP` / short press | mode=SHOOT | `IDLE` | camera.abort(), disable, emit `SHOT_STOPPED reason=request` |
| `JOG` | motion done | — | `IDLE` | disable motor, emit `JOG_DONE` |
| `JOG` | `STOP` / short press | — | `IDLE` | motion.halt(), disable, emit `JOG_STOPPED reason=request` |
| any active | `ABORT` / long press | — | `IDLE` | motion.halt(), camera.abort(), disable, emit `RUN_STOPPED` / `STEP_STOPPED` / `JOG_STOPPED` / `SHOT_STOPPED` by mode, reason=abort |
| `FAULT` | `START` / button | — | `SEEK` | clear error, enable, mode=RUN |
| `FAULT` | `STOP` / `ABORT` | — | `IDLE` | clear error, disable |
| any | driver fault | readback says fault | `FAULT` | motion.halt(), camera.abort(), disable, error=DRIVER_FAULT, emit the mode-specific stop event with `reason=error` (if active), emit `FAULT` |

`stopRequested` is a flag, not a state. When set during `RUN`, the machine keeps
the current mode and frame; after `CAMERA` completes it goes to `IDLE`.

Two frame counters are kept: `frames` is the total since boot (monotonic) and
`runFrames` counts the current run, reset by `START`. `EVT FRAME_DONE index`
uses the per-run index; `EVT RUN_STOPPED frames=N` reports `runFrames`.

Motor enable is normally tied to activity: it is enabled for motion and disabled
on entry to `IDLE` or `FAULT`. `ENABLE` is an explicit maintenance override that
keeps the driver energized while in `IDLE`; `DISABLE`, any motion command,
`ABORT`, or entry into `FAULT` clears the override and restores the normal rule.

### 6.2 Frame sequence

```
SEEK:      motor.moveBy(largeTarget); wait for notch
NOTCH:     motor.stop()                 // decelerate with acceleration
ALIGN:     wait until !isMoving()
           motor.moveBy(offsetSteps)    // signed; skipped if 0
           wait until !isMoving()
CAMERA:    camera.start(now)
           SETTLE  (settleMs)  → focus HIGH
           FOCUS   (focusMs)   → shutter HIGH
           SHUTTER (shutterMs) → shutter LOW, focus LOW
           POST    (postShotMs)
DONE:      RUN → SEEK; SINGLE/SHOOT → IDLE
```

The offset is applied **after** the controlled stop. The stop distance depends on
seek speed and acceleration, so changing either requires recalibrating
`offsetSteps`.

### 6.3 Graceful stop vs. abort

- `STOP` / short press while `mode=RUN`: set `stopRequested`; continue the
  current frame to completion; then `IDLE` + `RUN_STOPPED reason=request`.
- `STOP` / short press while `mode=SINGLE` or in `JOG`: halt motion immediately;
  camera abort; `IDLE` + `STEP_STOPPED` / `JOG_STOPPED reason=request`.
- `STOP` / short press while `mode=SHOOT`: abort the camera sequence (an active
  shutter pulse completes); `IDLE` + `SHOT_STOPPED reason=request`.
- `ABORT` / long press in any active state: halt motion, abort camera, disable
  motor, `IDLE`, event with `reason=abort` (`RUN_STOPPED`, `STEP_STOPPED`,
  `JOG_STOPPED` or `SHOT_STOPPED` by mode).
- A driver fault while active halts the same way, emits the mode-specific stop
  event with `reason=error`, then enters `FAULT` and emits `EVT FAULT`.
- If the camera is in `SHUTTER`, `Camera::abort()` lets the pulse finish before
  releasing the outputs. If it has not reached `SHUTTER`, it releases without
  firing.

### 6.4 Button handling

`Button` produces `SHORT_PRESS` on release (< 2 s) or `LONG_PRESS` at the 2 s
mark while held. `ScannerController` maps it:

| State / mode | Short | Long |
|---|---|---|
| `IDLE`, `FAULT` | start run | start run |
| `RUN` (`SEEK`/`ALIGN`/`CAMERA`) | `stopRequested = true` | abort |
| `SINGLE` (`SEEK`/`ALIGN`/`CAMERA`) | immediate stop | abort |
| `JOG` | immediate stop | abort |
| `SHOOT` (`CAMERA`) | abort | abort |
| `stopRequested == true` | ignored | abort |
| `BOOT` | ignored | ignored |

A contact held at power-on is ignored until released. There is no level mode and
no auto-start at boot.

### 6.5 Dependencies and status snapshot

`ScannerController` is constructed with `NotchSensor&`, `Button&`,
`MotionController&`, `Camera&`, `ITmcDriver&`, `const RunConfig&`, `IClock&`,
`IEventSink&`, `IStatusSink&` and `ILogSink&`. It does not know
`ConfigManager`; `RunConfig` supplies the offset, notch timeout, seek budget and
status period.

`ScannerController::snapshot()` is a pure query: it returns a `StatusSnapshot`
with the current state, mode, flags, camera phase, position, counters, last error
and `clock.nowMs()`. It does not change state or emit events.

When `statusPeriodMs > 0` and the deadline expires, `tick()` calls
`statusSink.write(snapshot())`. The explicit `STATUS` command calls the same
method. The `StatusLed` is driven separately by `main` with `state()`.

## 7. Component details

### 7.1 `NotchSensor`

- Constructor: `NotchSensor(IInput& input, const InputConfig& config)`.
- Samples `IInput` each `update()`.
- Active = pin LOW.
- Reads `notchDebounceMs` from `InputConfig` on each update, so a live `SET`
  takes effect without a setter.
- Debounce: the raw value must be stable for `notchDebounceMs` before it becomes
  the stable state.
- `consumeClosedEvent()` returns true once per open→closed transition.
- `consumeOpenEvent()` returns true once per closed→open transition.
- Initial state at `begin()` is the raw value; no event is generated for the
  initial state.
- One event flag each, cleared on consume.

### 7.2 `Button`

- Constructor: `Button(IInput& input, const InputConfig& config)`.
- Samples `IInput` each `update()`.
- Active = pin LOW.
- Reads `buttonDebounceMs` and `longPressMs` from `InputConfig` when needed, so
  live `SET` changes apply without setters.
- If the input is active at `begin()`, enter `IgnoreUntilRelease`: no press is
  recognized until the input has been inactive for the debounce time.
- On a debounced press, record `pressStart`.
- If still pressed at `pressStart + longPressMs`, emit `LONG_PRESS` once and mark
  the press as long.
- On debounced release: if the press was long, emit nothing; if it was short,
  emit `SHORT_PRESS`.
- `consumeEvent()` returns and clears the single pending event.

### 7.3 `Camera`

- Constructor: `Camera(IOutput& focus, IOutput& shutter,
  const CameraConfig& config)`.
- Owns `CameraPhase`; reads the four timings from `CameraConfig` at the start of
  each phase, so a live `SET` applies to the next sequence.
- `start(now)`: phase `SETTLE`, deadline `now + settleMs`.
- `SETTLE` done → `FOCUS`, focus output HIGH, deadline `+focusMs`.
- `FOCUS` done → `SHUTTER`, shutter HIGH, deadline `+shutterMs`.
- `SHUTTER` done → shutter LOW, focus LOW, `POST`, deadline `+postShotMs`.
- `POST` done → `DONE`; `consumeDone()` returns true once.
- `abort()`:
  - `SETTLE`/`FOCUS`: outputs LOW, phase `DONE` (no photo).
  - `SHUTTER`: keep the pulse until the original shutter deadline, then outputs
    LOW and `DONE` (no post delay).
  - `POST`: outputs LOW, `DONE`.
- `tick(now)` must be called every loop, even in `IDLE`, so an abort can finish.

### 7.4 `MotionController`

- Constructor: `MotionController(IStepperMotor& motor,
  const MotionConfig& config)`.
- Wraps `IStepperMotor`; reads `seekSpeed`, `acceleration` and `jogSpeed` from
  `MotionConfig` when it starts a move.
- `configure()` applies the current `seekSpeed`/`acceleration` to the motor.
- `moveBy` and `moveTo` pass through.
- `poll()` calls `motor.run()`; must be called every iteration.
- `stop()` = decelerate; `halt()` = immediate.
- `isMoving()` = `motor.isRunning()`.
- `enable()` / `disable()` pass through.
- For `SEEK`, the controller uses `moveBy(seekTarget)` where `seekTarget` is
  `maxSeekSteps` if > 0, otherwise 1,000,000 µsteps (~29 m). The notch stops it
  long before the target.

### 7.5 `ConfigManager`

- Owns exactly one `Config` instance for the lifetime of the program.
- `begin()` loads from `IConfigStore` into that existing instance; on failure it
  loads defaults in place and logs a warning. It never swaps in a temporary
  object, because other components hold references to its slices.
- `config()` returns `const Config&` for the composition root.
- `get(key, int32_t& value)` returns `UNKNOWN_KEY` or `READ_ONLY`.
- `set(key, int32_t value)` returns a `ConfigSetResult`. On success `error` is
  `NONE`; on failure it is `UNKNOWN_KEY`, `READ_ONLY` or
  `VALUE_OUT_OF_RANGE`. A successful set mutates the existing `Config` in place
  and does not persist.
- `result.change` is `DRIVER` for driver keys, `LOG_LEVEL` for `logLevel`, and
  `NONE` for every other key. `CommandHandler` uses it to re-apply the TMC
  registers or the logger level. No component is called back.
- `save()` writes EEPROM and emits `CONFIG_SAVED`.
- `reset()` restores defaults into the existing `Config`, saves, and emits
  `CONFIG_RESET`.
- All other live values are read by their owning component from its config slice
  on next use; see [06-configuration.md §5](06-configuration.md#5-live-reads-and-ownership).
- `CommandHandler` accepts `SET` only when `state == IDLE`; otherwise it returns
  `ERR BUSY` without calling `set()`. `ConfigManager` itself has no state
  dependency.

### 7.6 `StatusLed`

- `tick(nowMs, state)` drives the LED from the supplied state.
- `IDLE`: 1 Hz.
- `SEEK`, `ALIGN`, `CAMERA`, `JOG`: solid.
- `FAULT`: 5 Hz.
- `BOOT`: solid for 500 ms.
- Uses `IOutput` and `IClock`; fully testable.

### 7.7 `CommandParser`

Pure function, no Arduino:

```cpp
struct Command {
  enum class Type : uint8_t { NONE, START, STOP, ABORT, STATUS, CONFIG, GET,
                              SET, SAVE, RESET_CONFIG, JOG, STEP, SHOOT,
                              ENABLE, DISABLE, VERSION, HELP, PING };
  Type type;
  char key[24];
  int32_t value;
  bool hasValue;
};

ErrorCode parseCommand(const char* line, Command& out);
```

- Case-insensitive command and key names (stored lowercase).
- Whitespace separated; leading/trailing whitespace ignored; multiple spaces
  collapsed.
- Max line length 63 characters; longer → `BAD_COMMAND`.
- `JOG` requires a signed integer.
- `SET` requires a key and an integer (booleans accept `0/1`, `true/false`,
  `on/off`).
- All other commands take no arguments; extra arguments → `BAD_COMMAND`.

## 8. Timing model

- `millis()` is the only time source in core; `IClock` abstracts it.
- Deadlines use rollover-safe comparison:
  ```cpp
  static bool reached(uint32_t now, uint32_t deadline) {
    return static_cast<int32_t>(now - deadline) >= 0;
  }
  ```
- No `delay()` anywhere in core.
- `ScannerController::tick()` calls, in order:
  1. `notch.update()`
  2. `button.update()`
  3. `motion.poll()`
  4. state-specific logic
  5. `camera.tick(now)` (always)
  6. periodic `STATUS` if configured
- The main loop then calls `statusLed.tick(now, controller.state())`. The LED is
  deliberately outside the state machine: it renders state, it does not drive
  it.
- The main loop must complete in well under 1 ms. Serial input is read one byte
  per iteration, not in a blocking loop.
- At 400 µsteps/s the step interval is 2.5 ms; there is ample margin.
- Logging is only done at transitions. The serial log sink checks
  `Serial.availableForWrite()` and drops the message if the buffer is full, so a
  full USB buffer cannot stall motion.

## 9. Error handling

- `ErrorCode` is returned from command handlers immediately (`ERR`), or stored
  as `lastError` and reported in `STATUS` / events.
- `NOTCH_TIMEOUT` is not a fault: the machine returns to `IDLE` and is ready.
- `DRIVER_FAULT` (only with UART readback) enters `FAULT`, disables the motor,
  emits `EVT FAULT`.
- `DRIVER_UART` (readback failed) is a `WARN`; the firmware continues write-only.
- `EEPROM_INVALID` / `CONFIG_INVALID` at boot load defaults and log a warning.
- `INTERNAL` is a last-resort error (e.g., invalid state) and enters `FAULT`.
- Recovery: `START` from `FAULT` clears the error and starts; `STOP` returns to
  `IDLE`; a button press starts.

### 9.1 Response/event ordering

The protocol requires the response to a command to appear before any events it
triggers. The implementation guarantees this without putting a queue in the
core:

- `SerialEventSink::emit()` appends the event to a small fixed FIFO (8 entries)
  instead of writing immediately. `flush()` writes the queued `EVT` lines.
- `CommandHandler::poll()` prints `OK`/`ERR`/`CFG`/`STATUS` first. It does not
  flush the event FIFO.
- `main loop()` runs `controller.tick()`, then `eventSink.flush()`, then
  `commandHandler.poll()`. Events produced by a command are therefore written on
  the next iteration, after the response.
- If the FIFO is full, the new event is dropped and a `LOG WARN` is emitted.
  A single command produces at most two events, so overflow indicates a bug.
- Periodic `STATUS` does not go through `IEventSink`. When `statusPeriodMs`
  expires, `ScannerController::tick()` calls `IStatusSink::write(snapshot())`;
  `SerialStatusSink` prints the `STATUS` line without `OK STATUS`. The explicit
  `STATUS` command calls the same `write(snapshot())` and then prints
  `OK STATUS`, so there is only one formatter.

## 10. Memory budget

| Item | Approx. |
|---|---|
| Arduino core + HardwareSerial | ~2 KB flash, 128 B RAM (Serial buffers 64+64) |
| TMCStepper 0.7.3 | ~6–10 KB flash |
| AccelStepper 1.64 | ~4 KB flash |
| scanner_core + HAL | ~8–10 KB flash |
| **Total flash** | **~20–24 KB of 32 KB** |
| Objects + config + log buffer | < 1 KB RAM |
| **Total RAM** | **~1.3–1.6 KB of 2 KB** |

Rules to stay within budget:

- No `std::`, no `String`, no `new`/`malloc`.
- Fixed buffers only: log line 96 bytes, command line 64 bytes, config 47 bytes.
- `F()` for constant strings in the HAL.
- Keep `snprintf` use in the logger; avoid `printf` in hot paths.
- Check `pio run -e uno` output for the actual flash/RAM usage.

## 11. File layout

```
lib/scanner_core/
  library.json
  include/scanner_core/
    Types.h
    Config.h
    ConfigManager.h
    CommandParser.h
    IClock.h
    IInput.h
    IOutput.h
    IStepperMotor.h
    ITmcDriver.h
    IConfigStore.h
    ILogSink.h
    IEventSink.h
    IStatusSink.h
    Logger.h
    NotchSensor.h
    Button.h
    Camera.h
    MotionController.h
    ScannerController.h
    StatusLed.h
  src/
    Types.cpp
    Config.cpp
    ConfigManager.cpp
    CommandParser.cpp
    Logger.cpp
    NotchSensor.cpp
    Button.cpp
    Camera.cpp
    MotionController.cpp
    ScannerController.cpp
    StatusLed.cpp

src/
  main.cpp
  hal/
    ArduinoClock.h/.cpp
    ArduinoInput.h/.cpp
    ArduinoOutput.h/.cpp
    AccelStepperMotor.h/.cpp
    TmcStepperDriver.h/.cpp
    EepromConfigStore.h/.cpp
    SerialLogSink.h/.cpp
    SerialEventSink.h/.cpp
    SerialStatusSink.h/.cpp
    SerialCommandHandler.h/.cpp

test/
  test_native/
    test_config.cpp
    test_logger.cpp
    test_command_parser.cpp
    test_notch_sensor.cpp
    test_button.cpp
    test_camera.cpp
    test_motion_controller.cpp
    test_config_manager.cpp
    test_state_machine.cpp
    test_status_led.cpp
  fakes/
    FakeClock.h
    FakeInput.h
    FakeOutput.h
    FakeStepperMotor.h
    SimulatedStepperMotor.h
    FakeTmcDriver.h
    FakeConfigStore.h
    FakeCameraOutputs.h
    RecordingLogSink.h
    RecordingEventSink.h
    RecordingStatusSink.h
```

## 12. Composition root

`src/main.cpp` is the only place that knows concrete types. It:

1. Creates `ArduinoClock`, inputs, outputs.
2. Creates `AccelStepperMotor` (STEP/DIR/EN), `TmcStepperDriver`.
3. Creates `EepromConfigStore`, `SerialLogSink`, `SerialEventSink`,
   `SerialStatusSink`.
4. Creates `ConfigManager`, calls `begin()` and takes
   `const Config& config = configManager.config()`. `ConfigManager` owns the
   single `Config`.
5. Creates `Logger`, `NotchSensor`, `Button`, `MotionController`, `Camera`,
   `StatusLed` and `ScannerController`, passing only the slices they need:
   `config.input`, `config.camera`, `config.motion`, `config.run`.
6. Calls `tmc.begin()`, `tmc.apply(config.driver)`, `controller.begin()`, and
   creates `SerialCommandHandler` with `configManager`, `controller`, the sinks
   and `tmc`.
7. Emits `EVT READY fw=<version> proto=1`.
8. Starts the watchdog.
9. In `loop()`: `wdt_reset()`, `controller.tick()`, `eventSink.flush()`,
   `commandHandler.poll()`, `statusLed.tick(clock.nowMs(), controller.state())`.

`CommandHandler` is implemented by `src/hal/SerialCommandHandler.h/.cpp` and is
allowed to use Arduino types.
It reads one byte per loop, assembles lines, calls `parseCommand`, dispatches to
the controller/config manager, and prints `OK`/`ERR`/`CFG`/`EVT`/`LOG` lines.
After a successful `SET`, it applies `ConfigChange::DRIVER` by calling
`tmc.apply(config.driver)` and `ConfigChange::LOG_LEVEL` by calling
`logger.setLevel(config.logLevel)`. It never flushes the event FIFO.

## 13. Design rules

- One reason to change per class.
- Dependencies point inward; the core knows nothing about Arduino.
- Interfaces are small and named after the role, not the implementation.
- **Depend on the narrowest type that solves the problem.** A component that
  needs debounce times takes `InputConfig`, not `Config`; a component that needs
  no data takes no config argument at all.
- **Never inject `ConfigManager` into a component.** Only `main.cpp` and
  `CommandHandler` know it. Components receive a `const XConfig&` slice or plain
  values.
- **No service locator, no global config access.** If a class reaches through a
  manager to get values, the dependency is wrong.
- No global mutable state; pass dependencies through constructors.
- No boolean parameters that switch behavior; use enums or separate methods.
- Every state transition is explicit and testable.
- A command returns immediately; long operations report completion via events.
- Nothing in the core calls `millis()`, `micros()`, `digitalRead()`,
  `digitalWrite()`, `Serial`, `EEPROM`, or `delay()` directly.

## 14. Architecture decision records

- [ADR-0001 — Library stack](adr/0001-library-stack.md)
- [ADR-0002 — STEP/DIR and sensor-referenced positioning](adr/0002-step-dir-and-positioning.md)
- [ADR-0003 — Configuration persistence](adr/0003-config-persistence.md)
- [ADR-0004 — ASCII serial protocol](adr/0004-serial-protocol.md)
- [ADR-0005 — Config slices and dependency direction](adr/0005-config-slices-and-dependency-direction.md)
