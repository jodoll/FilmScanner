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
  [03-architecture.md §6.1](03-architecture.md#61-events-and-transitions) has a test.
- Every command in [05-serial-protocol.md](05-serial-protocol.md) has a
  happy-path test and, where applicable, an error test.
- Tests assert observable behavior (return values, emitted events, output
  states), not private members.
- Use fakes (`FakeClock`, `FakeStepperMotor`, …); never real time or hardware.
- Add a test for every new behavior; §4 states the required depth, not a
  ceiling.

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

This is the canonical statement of the rule; other documents point here.

## 2. Native test environment

Native unit tests run through PlatformIO's `native` platform with GoogleTest.

Requirements:

- The `native` environment does not compile `src/`.
- Core sources stay C++11-compatible. The host test build may select C++17,
  because current GoogleTest requires it. The Uno environment stays on C++11.
- Tests live in `test/test_native/`; fakes live in `test/fakes/`.
- `test_main.cpp` provides the only `main()`: `InitGoogleTest` followed by
  `RUN_ALL_TESTS`.
- Include `<gtest/gtest.h>` in every test file; do not define `main()` there.
- No `delay()`, `sleep()`, or real time. Use `FakeClock`.
- A failing test must point at the reason, not just "assert failed".

Commands:

```bash
pio test -e native       # run all native tests
pio test -e native -v    # one line per GoogleTest case
```

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

Every core interface is replaced by a fake in the native tests: clock, notch and
button inputs, focus and shutter outputs, stepper motor, motion controller, TMC
driver, non-volatile store, and recording log/event/status sinks. Keep them in
`test/fakes/`.

- The pure `StateMachine` needs no fakes at all.
- `StepperMotionController` is tested against `FakeStepperMotor`.
- `ScannerController` is tested against `FakeMotionController` plus the real
  `NotchSensor`, `Button` and `Camera` over `FakeInput`/`FakeOutput`/`FakeClock`.
- No real time and no hardware.
- A fake records what the core asked for and lets the test drive what happens
  next (`advance(ms)`, `finishMove()`, `setActive()`, `setFault()`).
- Timing-sensitive behavior uses a simulated motor driven by `FakeClock`, never
  real time.
- If a fake is wrong, fix the fake, not the production code.

Example:

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

## 4. Required coverage

The implementer chooses the individual test cases. This section states the depth
expected, not a checklist; every behavior named here needs direct tests,
including its boundaries and error paths.

- **Config validation** — defaults, every range boundary and an out-of-range
  value, the `microsteps` allowed set, and the cross-field rules
  (`longPressMs > buttonDebounceMs`, `maxSeekSteps` is 0 or at least 100). See
  [06-configuration.md §3](06-configuration.md#3-validation).
- **Persistence** — the record round-trips through `ConfigRecord`; a wrong
  magic, version, length or checksum, or an out-of-range value, discards the
  record wholesale and loads the defaults; a save is verified by read-back. See
  [06-configuration.md §4](06-configuration.md#4-persistence).
- **Serial grammar** — case-insensitivity, whitespace, CRLF, argument count,
  numeric overflow, overlong keys and lines. See
  [05-serial-protocol.md](05-serial-protocol.md).
- **Inputs** — notch and button debounce, one-shot edges, boot-held button,
  short versus long press. See
  [03-architecture.md §7.1–7.2](03-architecture.md#71-notchsensor).
- **Camera** — phase order and timing, abort in each phase, and an active
  shutter pulse always completes. See
  [03-architecture.md §7.3](03-architecture.md#73-camera).
- **Motion** — `StepperMotionController`: seek, offset and jog start; the enable
  policy and override; `idle()` de-energizes unless overridden; `halt()` clears
  the override; stop versus halt; completion. See
  [03-architecture.md §7.5](03-architecture.md#75-imotioncontroller-and-steppermotioncontroller).
- **State machine** — every transition in
  [03-architecture.md §6.1](03-architecture.md#61-events-and-transitions), every
  guard and rejection, every stop reason, every announcement bit, and mode
  memory. It is pure, so these tests use no fakes and no clock.
- **Orchestration** — `ScannerController`: the entry and exit actions in
  [03-architecture.md §6.3](03-architecture.md#63-entry-and-exit-actions), the
  announcement-to-event mapping, counters, deadlines and the snapshot. Use
  `FakeMotionController`; keep the other collaborators real over fakes.
- **Config push** — `ConfigManager`: a valid set pushes to exactly one consumer,
  a rejected set leaves the aggregate and every consumer unchanged, `begin()`
  and `reset()` push all slices, `save()` serialises the aggregate, and
  `get()`/read-only/unknown behave as specified. See
  [06-configuration.md §5](06-configuration.md#5-live-updates-and-ownership).
- **Command handling** — each command maps to the right controller or manager
  call, `SET` is rejected outside `IDLE`, and save/reset emit their events.
- **Status LED** — the pattern for each state, including `STOPPING` and
  `OFFSET`. See [03-architecture.md §7.8](03-architecture.md#78-statusled).

Where the specification gives a concrete number, test it: the default frame move
is 1290 microsteps.

Wire names are pinned the same way: one parameterized case per enum member
asserts that `toString` returns the string in
[05-serial-protocol.md](05-serial-protocol.md), so a new state, event or error
code fails the test until it is documented and mapped.

## 5. Coverage targets

- 100 % of public methods in `scanner_core` are exercised.
- Every `ErrorCode` path is reachable by a test.
- Every state-machine transition in
  [03-architecture.md §6.1](03-architecture.md#61-events-and-transitions) has at least one
  test.
- Every command in the protocol has a happy-path and an error test where
  applicable.
- Every config key is tested for valid and invalid values.

Coverage is measured by review, not by a tool; AVR/native coverage adds
complexity without changing the outcome.

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
