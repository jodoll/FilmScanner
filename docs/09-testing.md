# 09 — Testing

## 1. Strategy

- **Native unit tests** cover all logic in `lib/scanner_core/` using fakes. They
  run on the PC with `pio test -e native`, need no hardware, and must be fast and
  deterministic.
- **Hardware acceptance tests** cover the electrical/mechanical integration and
  the real camera. They are manual and follow the checklist in §8.
- There is no CI workflow. Run both locally before considering a milestone done.

The core is deliberately hardware-free, so the unit tests are the primary safety
net. Do not weaken that boundary to make a test easier.

### 1.1 Test-driven development (mandatory)

Every behavior in `lib/scanner_core/` is developed **test-first**:

1. **Red:** add a small failing GoogleTest case that states the next behavior.
2. Run `pio test -e native`; confirm it fails for the expected reason (not a
   compile error in the test itself).
3. **Green:** write the minimum production code that makes it pass.
4. **Refactor** with the test green; keep the test as the specification.
5. Commit the test and the code in the same change.

This applies to methods and classes, not only to end-to-end acceptance:

- Every public method in `scanner_core` has at least one direct unit test.
- Every branch, guard, boundary and error code has a test.
- Every state transition in
  [03-architecture.md §6.1](03-architecture.md#61-transitions) has a test.
- Every command in [05-serial-protocol.md](05-serial-protocol.md) has a
  happy-path test and, where applicable, an error test.
- Tests assert observable behavior (return values, emitted events, output
  states), not private members.
- Use fakes (`FakeClock`, `FakeStepperMotor`, …); never real time or hardware.
- The tables in §4 are the **minimum** list. Add a test for every new behavior;
  do not treat the table as a ceiling.

The Arduino adapters in `src/hal/` are deliberately **not** unit-tested: they
need `Arduino.h` and real hardware. Keep them thin — no decisions, no timing
logic, no state. Anything testable is moved into the core. Their integration is
covered by the manual hardware acceptance checklist in §8. Do not skip a unit
test by putting logic in an adapter "because it is easier there".

### 1.2 One test case per test

Every test case must run and report independently:

- One `TEST`, `TEST_F` or `TEST_P` instance = one test case.
- One behavior per test case. Do not combine unrelated behaviors.
- Do not loop over several inputs inside one test to emulate parameterization.
  Use `TEST_P` with `INSTANTIATE_TEST_SUITE_P`; each value is a separate case.
- Multiple `EXPECT_*` assertions are allowed only for one coherent behavior
  with several independent aspects. They are assertions, not separate cases.
- Prefer `ASSERT_*` for preconditions and single-outcome checks. Use
  `EXPECT_*` only when the case intentionally checks several independent facts
  and all failures should be reported.
- A failed `ASSERT_*` aborts only the current test case; the remaining
  parameterized instances still run.

## 2. Native test environment

`platformio.ini`:

```ini
[env:native]
platform = native
test_framework = googletest
build_flags =
    -std=gnu++17
    -D UNIT_TEST
    -I lib/scanner_core/include
```

The native environment uses `gnu++17` because current GoogleTest requires it.
The core sources remain C++11-compatible; only the host test build selects the
newer standard. The Uno environment stays on `gnu++11`.

Command:

```bash
pio test -e native -v
```

Rules:

- Tests live in `test/test_native/`.
- Fakes live in `test/fakes/`.
- `test_main.cpp` provides the only `main()`:
  `InitGoogleTest` followed by `RUN_ALL_TESTS`.
- Include `<gtest/gtest.h>` in every test file; do not define `main()` there.
- No `delay()`, `sleep()`, or real time. Use `FakeClock`.
- One test case per `TEST` / `TEST_F` / `TEST_P` instance.
- `TEST_F` when a fixture provides shared setup; plain `TEST` otherwise.
- Use `TEST_P` for table-driven input; `INSTANTIATE_TEST_SUITE_P` creates one
  separately reported case per value.
- Prefer `ASSERT_*` for preconditions and single-outcome checks. Use
  `EXPECT_*` only for multiple independent aspects of one behavior.
- A failing test must point at the reason, not just "assert failed".

### 2.1 Test shape

```cpp
#include <gtest/gtest.h>

TEST(ConfigValidation, RejectsOutOfRangeSeekSpeed) {
  Config config = defaultConfig();
  config.motion.seekSpeed = 49;

  ASSERT_EQ(validateConfig(config), ErrorCode::VALUE_OUT_OF_RANGE);
}

class SeekSpeedBoundary
    : public ::testing::TestWithParam<uint16_t> {};

TEST_P(SeekSpeedBoundary, AcceptsBoundary) {
  Config config = defaultConfig();
  config.motion.seekSpeed = GetParam();

  ASSERT_EQ(validateConfig(config), ErrorCode::NONE);
}

INSTANTIATE_TEST_SUITE_P(MinMax, SeekSpeedBoundary,
                         ::testing::Values(50, 2000));
```

`pio test -e native -v` reports `MinMax/SeekSpeedBoundary.AcceptsBoundary/0`
and `/1` separately. If `/0` fails, `/1` still runs.

## 3. Fakes

| Fake | Behavior |
|---|---|
| `FakeClock` | `nowMs()`; `advance(ms)`; starts at 0 |
| `FakeInput` | `setActive(bool)`, `read()` |
| `FakeOutput` | records `lastValue`, `writeCount`, `history()` |
| `FakeStepperMotor` | records calls; `finishMove()` snaps to target and clears running; `isRunning()` controllable |
| `SimulatedStepperMotor` | integrates position from `FakeClock` at a configured speed for timing tests |
| `FakeTmcDriver` | records `apply()`; configurable `begin()` result; `hasFault()` |
| `FakeConfigStore` | in-memory config; `load()` result controllable; records saves; only used by `ConfigManager` tests |
| `RecordingLogSink` | stores formatted log lines |
| `RecordingEventSink` | stores emitted `Event`s; helpers `has(type)`, `last()` |
| `RecordingStatusSink` | stores `StatusSnapshot`s; `last()` |
| `FakeCameraOutputs` | two `FakeOutput`s for focus/shutter |

`FakeStepperMotor` example:

```cpp
class FakeStepperMotor : public IStepperMotor {
public:
  void moveBy(int32_t steps) override { position_ += steps; target_ = position_; }
  void run() override {}
  void stop() override { running_ = false; }
  void halt() override { running_ = false; }
  bool isRunning() const override { return running_; }
  void finishMove() { position_ = target_; running_ = false; }
  // ...
};
```

For tests that need the state machine to advance through a move, call
`finishMove()` and then `controller.tick()`.

## 4. Test cases

### 4.0 How to read the tables

- Each row is one test case unless it explicitly says otherwise.
- A cell with several values (`1, 2, 4, ...`) means a `TEST_P` suite with one
  reported instance per value, not a loop inside one test.
- Rows marked `(N cases)` produce N separately reported test cases.
- Use `TEST_F` when the cases share fixture setup; use `TEST` for a single case
  with no shared setup.
- The tables are the minimum list. Add a test case for every new behavior.

### 4.1 `Config`

| Test | Given | When | Then |
|---|---|---|---|
| defaults valid | — | `validateConfig(defaultConfig())` | `NONE` |
| microsteps valid set (7 cases) | — | 1, 2, 4, 8, 16, 32, 64 | `NONE` |
| microsteps invalid (3 cases) | — | 3, 100, 128 | `VALUE_OUT_OF_RANGE` |
| current bounds (2 cases) | — | 100, 2000 | `NONE` |
| current out of range (2 cases) | — | 99, 2001 | error |
| hold percent bounds (2 cases) | — | 0, 100 | `NONE` |
| seek speed bounds (2 cases) | — | 50, 2000 | `NONE` |
| acceleration bounds (2 cases) | — | 100, 20000 | `NONE` |
| offset bounds (2 cases) | — | −5000, 5000 | `NONE` |
| timeout bounds (2 cases) | — | 1000, 60000 | `NONE` |
| long press > debounce | debounce 100, long 500 | validate | `NONE` |
| long press <= debounce | debounce 100, long 50 | validate | error |
| maxSeekSteps 0 | — | validate | `NONE` |
| maxSeekSteps 1..99 (3 cases: 1, 50, 99) | — | validate | error |
| slice round trip | non-default values in all five slices | serialize/deserialize | equal |
| packed size | — | `sizeof(PersistedConfig)` | 47 |
| CRC round trip | config | serialize/deserialize | equal |
| corrupt CRC | valid bytes, flip one | deserialize | invalid |
| wrong magic | — | deserialize | invalid |
| wrong version | — | deserialize | invalid |
| wrong length | — | deserialize | invalid |
| derived values | microsteps 16 | `stepsPerMm` | 33.953 |
| derived frame | microsteps 16 | `expectedFrameSteps` | 1290 |

### 4.2 `CommandParser`

| Test | Input | Expected |
|---|---|---|
| simple start | `START` | `Type::START` |
| lowercase | `start` | `Type::START` |
| whitespace | `  START  ` | `Type::START` |
| CRLF | `START\r` | `Type::START` |
| empty | `` | ignore / `NONE` |
| unknown | `FOO` | `BAD_COMMAND` |
| too long | 64+ chars | `BAD_COMMAND` |
| GET | `GET offsetSteps` | key `offsetSteps` |
| GET missing | `GET` | `BAD_COMMAND` |
| SET | `SET offsetSteps 68` | key/value 68 |
| SET negative | `SET offsetSteps -68` | −68 |
| SET bool true | `SET stealthChop true` | 1 |
| SET bool on | `SET stealthChop on` | 1 |
| SET bool false | `SET stealthChop false` | 0 |
| JOG positive | `JOG 100` | 100 |
| JOG negative | `JOG -100` | −100 |
| JOG missing | `JOG` | `BAD_COMMAND` |
| JOG non-numeric | `JOG abc` | `BAD_COMMAND` |
| overflow | `SET offsetSteps 99999999999` | `BAD_COMMAND` |
| extra args | `START now` | `BAD_COMMAND` |
| key too long | 32-char key | `BAD_COMMAND` |
| unknown command case | `sTaRt` | `Type::START` |

### 4.3 `NotchSensor`

| Test | Given | When | Then |
|---|---|---|---|
| initial open | active=false | update | no event |
| close debounce | active=false → true | advance < 10 ms | no event |
| close accepted | active=true | advance 10 ms | `consumeClosedEvent()` true once |
| close one-shot | event consumed | update | no second event |
| bounce ignored | true 3 ms, false 2 ms, true | advance < 10 ms | no event |
| open event | closed stable | false + 10 ms | `consumeOpenEvent()` true |
| initial closed | active=true at begin | update | no event until open/close |
| live debounce change | `InputConfig::notchDebounceMs` 10 → 5 | close 5 ms | event |

### 4.4 `Button`

| Test | Given | When | Then |
|---|---|---|---|
| short press | press 100 ms, release | update | `SHORT_PRESS` on release |
| long press | press, hold 2000 ms | update | `LONG_PRESS` at 2000 ms |
| long release | after long | release | no `SHORT_PRESS` |
| bounce | 5 ms toggles | update | no event |
| boot held | active at begin | update 10 s | no event |
| boot held release | active at begin, release | update | no event; next press works |
| threshold exact | hold 2000 ms | update | `LONG_PRESS` |
| short after long | — | press/release | new short works |
| debounce | 20 ms | 10 ms press | no event |
| consume | event | consume twice | first true, second false |

### 4.5 `Camera`

| Test | Given | When | Then |
|---|---|---|---|
| idle outputs | — | begin | focus/shutter LOW |
| settle | start | advance 199 ms | focus LOW |
| focus on | start + 200 ms | tick | focus HIGH |
| focus done | focus + 300 ms | tick | shutter HIGH |
| shutter pulse | shutter + 150 ms | tick | shutter LOW, focus LOW |
| post | after shutter | advance 299 ms | not done |
| done | after post | advance 300 ms | done |
| abort settle | start | abort | both LOW, done, no shutter |
| abort focus | focus on | abort | focus LOW, done |
| abort shutter | shutter on 50 ms | abort | shutter remains HIGH until 150 ms, then LOW; no post |
| abort post | post | abort | both LOW, done |
| zero settle | settle=0 | start/tick | focus immediately |
| zero post | post=0 | shutter done | done immediately |
| live timing change | `CameraConfig::shutterMs` 150 → 50 | next `start()` | 50 ms pulse |

### 4.6 `MotionController`

| Test | Given | When | Then |
|---|---|---|---|
| moveBy | — | `moveBy(100)` | motor target 100 |
| moveTo | — | `moveTo(500)` | motor target 500 |
| poll | — | `poll()` | motor `run()` called |
| stop | moving | `stop()` | motor `stop()` called |
| halt | moving | `halt()` | motor `halt()` called |
| enable | disabled | `enable()` | motor enabled |
| disable | enabled | `disable()` | motor disabled |
| configure | `MotionConfig` 400/2000 | `configure()` | motor receives 400/2000 |
| live speed change | `MotionConfig::seekSpeed` 400 → 800 | next move | motor receives 800 |
| position | position 42 | `position()` | 42 |
| moving | running | `isMoving()` | true |

### 4.7 `ConfigManager`

| Test | Given | When | Then |
|---|---|---|---|
| load valid | store valid | `begin()` | loaded values |
| load empty | store false | `begin()` | defaults + warning |
| load invalid | store corrupt | `begin()` | defaults + warning |
| set valid | — | `set("offsetSteps", 68)` | `error=NONE`, value updated |
| set unknown | — | `set("foo", 1)` | `error=UNKNOWN_KEY` |
| set read-only | — | `set("pin.notch", 5)` | `error=READ_ONLY` |
| set out of range | — | `set("seekSpeed", 99999)` | `error=VALUE_OUT_OF_RANGE` |
| set bool | — | `set("stealthChop", 0)` | `error=NONE`, value updated |
| save | — | `save()` | store called |
| reset | — | `reset()` | defaults + store called |
| log level change | — | `set("logLevel", 3)` | `error=NONE`, `change=LOG_LEVEL` |
| driver change | — | `set("microsteps", 32)` | `error=NONE`, `change=DRIVER` |
| other change | — | `set("offsetSteps", 68)` | `error=NONE`, `change=NONE` |
| microsteps warning | — | `set("microsteps", 32)` | warn logged |
| reset in place | component holds `config.run` | `reset()` | reference still valid, values default |
| get read-only | — | `get("fw")` | value |

### 4.8 `StatusLed`

| Test | Given | When | Then |
|---|---|---|---|
| boot solid | state BOOT | tick | LED HIGH for 500 ms |
| idle blink | IDLE | 0–499 ms | HIGH |
| idle blink off | IDLE | 500–999 ms | LOW |
| active solid | SEEK | tick | HIGH |
| fault fast | FAULT | 100 ms | toggles |
| camera active | CAMERA | tick | HIGH |

### 4.9 `ScannerController` — state machine

Use `FakeClock`, `FakeStepperMotor`, `FakeOutput`s for focus/shutter, `FakeInput`s
for notch and button, `FakeTmcDriver`, `RecordingEventSink`,
`RecordingStatusSink`, and a plain `Config config;`. Construct the real
`NotchSensor`, `Button`, `Camera` and `MotionController` with `config.input`,
`config.camera` and `config.motion`; pass `config.run` to `ScannerController`.
No `ConfigManager` is involved.

| Test | Given | When | Then |
|---|---|---|---|
| start from IDLE | IDLE | `start()` | state SEEK, motor enabled, `RUN_STARTED` |
| start from FAULT | FAULT | `start()` | state SEEK, error cleared |
| start busy | SEEK | `start()` | `BUSY` |
| seek notch | SEEK, notch open | notch closes + debounce | `stop()` called, state ALIGN, `NOTCH` |
| align offset | ALIGN, stopped, offset 68 | tick | `moveBy(68)` |
| align done | offset move finished | tick | state CAMERA, `FRAME_STARTED` |
| camera done RUN | CAMERA done | tick | state SEEK, `frames=1`, `runFrames=1`, `FRAME_DONE` |
| camera done SINGLE | CAMERA done | tick | state IDLE, `frames=1`, `FRAME_DONE` + `STEP_DONE` |
| camera done SHOOT | CAMERA done | tick | state IDLE, `SHOT_DONE` |
| stop RUN | SEEK | `stop()` | `stopRequested=1`, no halt |
| stop completes | stopRequested, CAMERA done | tick | IDLE, `RUN_STOPPED request` |
| stop SINGLE | SEEK | `stop()` | halt, IDLE, `STEP_STOPPED request` |
| stop JOG | JOG | `stop()` | halt, IDLE, `JOG_STOPPED request` |
| stop SHOOT | CAMERA, mode SHOOT | `stop()` | camera abort, IDLE, `SHOT_STOPPED request` |
| abort RUN | SEEK | `abort()` | halt, camera abort, disable, IDLE, `RUN_STOPPED abort` |
| abort JOG | JOG | `abort()` | halt, disable, `JOG_STOPPED abort` |
| abort camera shutter | CAMERA, shutter high | `abort()` | pulse completes, then IDLE |
| timeout RUN | SEEK, advance 15000 | tick | halt, IDLE, `RUN_STOPPED notch_timeout` |
| timeout SINGLE | SEEK, advance 15000 | tick | halt, IDLE, `STEP_FAILED NOTCH_TIMEOUT` |
| JOG no timeout | JOG, advance 60000 | tick | still JOG |
| max seek RUN | SEEK, mode RUN, maxSeekSteps 1000, no notch | finish move | `RUN_STOPPED motion_timeout`, IDLE |
| max seek SINGLE | SEEK, mode SINGLE, maxSeekSteps 1000, no notch | finish move | `STEP_FAILED MOTION_TIMEOUT`, IDLE |
| button short IDLE | IDLE | short press | run starts |
| button long IDLE | IDLE | long press | run starts |
| button short RUN | SEEK | short press | `stopRequested=1` |
| button long RUN | SEEK | long press | abort |
| button short SINGLE | SEEK, mode SINGLE | short press | halt, IDLE |
| button short stopRequested | stopRequested | short press | ignored |
| boot-held | input active at begin | tick | no start |
| driver fault IDLE | fake fault | tick | FAULT, motor disabled, `FAULT` |
| driver fault RUN | SEEK, mode RUN, fake fault | tick | `RUN_STOPPED error`, FAULT, motor disabled |
| stop from FAULT | FAULT | `stop()` | IDLE, error cleared |
| enable/disable | IDLE | commands | motor state changes, event |
| periodic status | statusPeriodMs 1000 | advance 1000 | one `StatusSnapshot` written, no `OK STATUS` |
| explicit status snapshot | IDLE | `snapshot()` | fields match state, motor, counters, last error |

### 4.10 Timing tests

| Test | Given | When | Then |
|---|---|---|---|
| frame motion | SimulatedStepperMotor, 400 µsteps/s, 2000 µsteps/s², 38 mm | run SEEK + ALIGN | ~3.43 s ± 5 % |
| camera total | timings 200/300/150/300 | camera run | 950 ms |
| debounce | 10 ms | 9 ms pulse | no event |
| long press | 2000 ms | 1999 ms | no event |

These are the only tests that care about physical time. They use
`SimulatedStepperMotor` and `FakeClock`, never real time.

## 5. Coverage targets

- 100 % of public methods in `scanner_core` are exercised.
- Every `ErrorCode` path is reachable by a test.
- Every state-machine transition in §6.1 of the architecture has at least one
  test.
- Every command in the protocol has a happy-path and an error test where
  applicable.
- Every config key is tested for valid and invalid values.

Coverage is measured by review, not a tool (AVR/native coverage adds
complexity). The test list above is the checklist.

## 6. What is not unit-tested

- Arduino pin timing, `AccelStepper` internals, `TMCStepper` register writes,
  EEPROM hardware, `SoftwareSerial`, and the USB serial port.
- These are covered by the hardware acceptance checklist and by keeping the
  adapters thin.

## 7. Debugging native tests

- Run one test file: `pio test -e native -f test_state_machine`.
- Filter one GoogleTest case or suite:
  `pio test -e native --program-arg=--gtest_filter='ConfigValidation.*'`.
- Add `-v` for verbose output; it shows each GoogleTest case, including every
  `TEST_P` instance.
- If a test hangs, check for a loop without `FakeClock::advance`.
- If a fake is wrong, fix the fake, not the production code.

## 8. Hardware acceptance checklist

Run after the unit tests pass and the firmware is flashed.

### 8.1 Boot and serial

- [ ] Power on; LED blinks at 1 Hz.
- [ ] `PING` → `OK PING`.
- [ ] `VERSION` → `OK VERSION fw=... proto=1`.
- [ ] `STATUS` reports `state=IDLE`, `motor=disabled`.
- [ ] `CONFIG` prints all keys without truncation.
- [ ] `LOG` lines are readable; no garbage at 115200.

### 8.2 Driver and motor

- [ ] `ENABLE` energizes the motor; `DISABLE` releases it.
- [ ] `JOG 100` moves forward; `JOG -100` returns.
- [ ] `STOP` halts a long `JOG` immediately.
- [ ] Motor direction is correct; if not, swap one coil pair.
- [ ] No skipped steps during 100 × `JOG 200`.
- [ ] Driver and motor temperatures are acceptable after 10 minutes.

### 8.3 Notch and frames

- [ ] `STEP` finds the next notch, aligns, takes one photo, returns to IDLE.
- [ ] A closed notch at start is skipped (move out, then to the next).
- [ ] `START` takes one photo per notch on a 4-frame holder.
- [ ] A second holder follows without a timeout.
- [ ] No notch → run stops after 15 s with `reason=notch_timeout`.
- [ ] `JOG` does not time out.

### 8.4 Stop/abort/button

- [ ] `STOP` during a run finishes the current frame, then stops.
- [ ] `ABORT` during a run stops within ~1.2 mm and takes no further photo.
- [ ] Short press starts from IDLE.
- [ ] Short press during a run stops gracefully.
- [ ] Short press during JOG/STEP stops immediately.
- [ ] 2 s long press during a run aborts at the 2 s mark.
- [ ] Release after a long press does nothing.
- [ ] A cable held at boot causes no motion until released.

### 8.5 Camera

- [ ] `SHOOT` focuses and fires the shutter.
- [ ] `STEP` produces a focused, aligned image.
- [ ] Focus is held during the shutter pulse.
- [ ] `focusMs`, `shutterMs`, `settleMs`, `postShotMs` changes take effect.
- [ ] Abort during a shutter pulse completes the pulse (no truncated exposure).

### 8.6 Config and persistence

- [ ] `SET offsetSteps ...` takes effect on the next frame.
- [ ] `SAVE` then power-cycle preserves values.
- [ ] `RESET_CONFIG` restores defaults and persists.
- [ ] Out-of-range `SET` is rejected and leaves RAM unchanged.
- [ ] `SET` outside IDLE returns `ERR BUSY`.
- [ ] `GET`/`CONFIG` work in every state.

### 8.7 Safety and robustness

- [ ] 2 s watchdog resets a blocked loop (temporary test hook).
- [ ] Motor is disabled in IDLE and FAULT.
- [ ] After a fault, `START` and a button press restart; `STOP` returns to IDLE.
- [ ] 36-frame run completes in under 10 minutes.
- [ ] Offset repeatability within ±1 microstep over three frames.
- [ ] No lost frames over a 36-frame run.

## 9. Acceptance sign-off

| Test | Result | Date | Tester |
|---|---|---|---|
| Unit tests | | | |
| AC-01 compile | | | |
| AC-02 native tests | | | |
| AC-03 boot | | | |
| AC-04 notch | | | |
| AC-05 single frame | | | |
| AC-06 continuous run | | | |
| AC-07 timeout | | | |
| AC-08 jog | | | |
| AC-09 graceful stop | | | |
| AC-10 abort | | | |
| AC-11 button | | | |
| AC-12 boot-held | | | |
| AC-13 config | | | |
| AC-14 validation | | | |
| AC-15 LED | | | |
| AC-16 watchdog | | | |
| AC-17 thermal | | | |
| AC-18 offset repeatability | | | |
| AC-19 36-frame run | | | |
| AC-20 error recovery | | | |
