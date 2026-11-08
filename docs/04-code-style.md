# 04 — Code style

This project follows Clean Code (Robert C. Martin) adapted to a small embedded
C++11 codebase. The goal is code that a new maintainer can read top-down and
test without hardware.

## 1. General principles

- **Small functions.** Aim for ≤ 20 lines and a single level of abstraction.
  Extract a named helper instead of adding a comment.
- **One responsibility per class.** A class should have one reason to change.
- **Intention-revealing names.** `moveToNextNotch()` beats `doStep2()`.
- **No boolean flag parameters.** `setMode(Mode::RUN)` beats
  `setMode(true)`. Split into two methods if the behavior really differs.
- **Command–query separation.** A method either does something or answers a
  question, not both.
- **No hidden side effects.** A getter must not move the motor or write EEPROM.
- **Guard clauses over nesting.** Return early on invalid input.
- **No duplication.** If the same three lines appear twice, extract them.
- **Boy Scout rule.** Leave the code cleaner than you found it.
- **Dependency inversion.** Core depends on interfaces; HAL implements them.
- **Narrow dependencies.** Take the smallest type that solves the problem. Do
  not pass a manager, facade or service locator when a value or a small config
  struct is enough. A component needing debounce times defines and stores
  `NotchConfig`, not `ConfigManager`.
- **Composition over inheritance.** Inheritance is only for interface
  implementation.
- **Errors are values.** Return `ErrorCode`/`Status`; never throw.
- **No magic numbers.** Put tunables in `Config`; put physical constants in a
  named `constexpr`.

## 2. Embedded pragmatism

Clean Code is a guide, not a religion. On an ATmega328P:

- Do not introduce an abstraction that costs more flash/RAM than it saves in
  clarity.
- Do not use virtual dispatch in a per-step hot path. The interfaces are used at
  the state-machine level (once per frame or per tick), which is fine.
- Prefer plain structs and free functions for value types.
- Keep the number of translation units reasonable; LTO is not used by default.
- Avoid `std::function`, `std::string`, `std::vector`, `new`, `delete`,
  exceptions and RTTI.

### 2.1 C++ on the Arduino Uno: the language, not the standard library

The Arduino toolchain compiles C++ with `avr-g++`. The `uno` environment
selects `-std=gnu++11`, and `-fno-exceptions -fno-rtti` are set explicitly. The
native test environment uses `gnu++17` because GoogleTest requires it; the core
sources themselves stay C++11-compatible. C++ classes, namespaces,
`enum class`, single inheritance, virtual interfaces and `constexpr` all work
and cost little. What does **not** fit is the C++ **standard library**,
not the language.

| Used | Not used |
|---|---|
| `class`, `struct`, `enum class`, `namespace` | `std::string`, `std::vector`, `std::map` |
| virtual interfaces with `override` | exceptions (`throw` / `try`) |
| `constexpr`, `static_cast`, `nullptr` | RTTI (`dynamic_cast`, `typeid`) |
| `<stdint.h>`, `<string.h>`, `<stdio.h>` from avr-libc | `<iostream>`, `<sstream>`, `<algorithm>` |
| fixed arrays, plain structs, references | `new` / `delete`, `std::function`, smart pointers |

Why:

- The AVR toolchain does not provide a full hosted C++ library. Pulling in
  `<iostream>` or STL containers either fails to link or consumes flash/RAM that
  the firmware does not have.
- `snprintf` (avr-libc) is used only by the logger. It is the largest libc
  dependency; if flash becomes tight, replace it with a small formatter.
- Floating point is used only for calibration and derived values, never in the
  step loop. The motion path is integer arithmetic.
- Virtual dispatch happens once per tick or frame, never per step. The
  step-pulse path is plain integer code.
- The same core sources compile with the host compiler for `pio test -e native`;
  keeping them portable C++11 is what makes native unit tests possible.

The resource budget is in [03-architecture.md §10](03-architecture.md#10-memory-budget):
roughly 20–24 KB of 32 KB flash and 1.3–1.6 KB of 2 KB RAM.

## 3. Naming

| Kind | Style | Example |
|---|---|---|
| Class / struct | `PascalCase` | `ScannerController` |
| Method / function | `camelCase` | `moveToNextNotch()` |
| Local variable | `camelCase` | `frameIndex` |
| Member variable | `camelCase` + `_` | `notchSensor_` |
| Constant / `constexpr` | `kPascalCase` | `kDefaultSeekSpeed` |
| Enum type | `PascalCase` | `ErrorCode` |
| Enum value | `UPPER_SNAKE` | `ErrorCode::NOTCH_TIMEOUT` |
| File | `PascalCase.h/.cpp` | `NotchSensor.cpp` |
| Test | `test_<subject>.cpp` | `test_state_machine.cpp` |

- Units belong in the name: `notchTimeoutMs`, `runCurrentMa`,
  `seekSpeedStepsPerSecond`, `offsetSteps`.
- Booleans read as questions: `isMoving`, `hasFault`, `canStart`.
- Avoid abbreviations except `Ms`, `Ma`, `Tmc`, `Led`, `Config`.
- Namespace: `scanner` for core.

## 4. Functions

```cpp
// Good: one job, guard clause, no flag parameter, no hardware detail.
ErrorCode ScannerController::startContinuousRun() {
  const Result result = machine_.startContinuousRun();
  if (result.error != ErrorCode::NONE) {
    return result.error;
  }
  applyTransition(result);   // entry/exit actions, counters, events
  return ErrorCode::NONE;
}
```

```cpp
// Bad: flag parameter, mixed abstraction levels, hidden side effect.
void ScannerController::doThing(bool start, bool fast) {
  if (start) { motor.enable(); if (fast) motor.setSpeed(1000); }
  // ...
}
```

Rules:

- ≤ 4 parameters. More means a parameter object.
- No output parameters unless the language forces it; return a struct instead.
- Prefer `const` references and `const` methods.
- Mark single-argument constructors `explicit`.
- Use `enum class`, never plain enums.
- Switch statements cover every enum value; no `default` that hides a new value.

## 5. Headers and includes

- `#pragma once` in every header.
- Include what you use; do not rely on transitive includes.
- Include order: corresponding header, C headers, C++ headers, project headers.
- No `using namespace` in headers.
- Keep headers self-contained.
- Forward-declare where possible to reduce compile time.
- Core headers must not include `Arduino.h`.
- `StateMachine.h` includes no project header; it owns its vocabulary.
- Components must not include `ConfigManager.h`. Each consumer defines and stores
  its own slice and exposes `applyConfig`; `ConfigManager` pushes into it.
- Declarations live in headers, not in `docs/`. The docs state behavior and the
  wire contract; do not paste interfaces, structs or enums into them.
- Avoid macros except for compile-time build flags and pin definitions.

## 6. Error handling

- Return `ErrorCode` from operations that can fail.
- Use `ErrorCode::NONE` for success.
- Never ignore a return value silently; if a failure is acceptable, log it.
- Validate at the boundary (serial parser, config load) and keep the core
  assumptions simple.
- `FAULT` is for conditions that require operator attention; `NOTCH_TIMEOUT` is
  a normal end-of-film condition, not a fault.
- No `assert()` in production; tests use GoogleTest assertions.

## 7. Logging

- Use the injected `Logger`, never `Serial` from the core.
- Log at state transitions, errors and configuration changes — not per step.
- Keep lines short (< 64 characters) so a full USB buffer cannot stall motion.
- Levels: `ERROR`, `WARN`, `INFO`, `DEBUG`. Default `INFO`.
- Never log from inside a step loop or a timing deadline handler.
- The HAL log sink drops messages when `Serial.availableForWrite()` is low.

## 8. Comments

- Comments explain **why**, not **what**.
- Prefer a well-named function to a comment.
- Document non-obvious constraints (e.g. why the offset is applied after the
  stop).
- Keep comments up to date; a stale comment is a bug.
- No commented-out code.

## 9. Tests

- **TDD is mandatory for core logic:** write a failing test first (red), then
  the minimum production code (green), then refactor with the test still green.
- Every public method in `scanner_core` has at least one direct unit test.
- Every branch, guard, boundary and error code has a test.
- Test the happy path, boundary values, and every error path.
- Structure: arrange / act / assert.
- F.I.R.S.T.: fast, independent, repeatable, self-validating, timely.
- A bug fix starts with a failing test.
- Acceptance/hardware tests never replace unit tests.
- The Arduino HAL adapters are kept thin and are covered by the hardware
  acceptance checklist, not by host unit tests.

The full test conventions — one case per `TEST`/`TEST_F`/`TEST_P`, parameterized
values, assertion choice, fakes, and the required coverage — live in
[09-testing.md §1](09-testing.md#1-strategy). Do not restate them here.

## 10. Formatting

- 2-space indentation, no tabs.
- 80–100 column soft limit.
- Braces on the same line (K&R).
- One statement per line.
- No trailing whitespace.
- `clang-format` is optional; if used, commit the `.clang-format` file.

Suggested `.clang-format`:

```yaml
BasedOnStyle: Google
IndentWidth: 2
ColumnLimit: 100
AllowShortFunctionsOnASingleLine: Empty
DerivePointerAlignment: false
PointerAlignment: Left
```

## 11. Example: interface and adapter

```cpp
// lib/scanner_core/include/scanner_core/IStepperMotor.h
#pragma once
#include <stdint.h>

namespace scanner {

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
  virtual void run() = 0;
  virtual void stop() = 0;
  virtual void halt() = 0;
  virtual int32_t position() const = 0;
  virtual bool isRunning() const = 0;
};

} // namespace scanner
```

```cpp
// src/hal/AccelStepperMotor.cpp
#include "AccelStepperMotor.h"

namespace scanner {

AccelStepperMotor::AccelStepperMotor(uint8_t stepPin, uint8_t dirPin,
                                     uint8_t enablePin)
    : stepper_(AccelStepper::DRIVER, stepPin, dirPin),
      enablePin_(enablePin),
      enabled_(false) {
  pinMode(enablePin_, OUTPUT);
  disable();
}

void AccelStepperMotor::enable() {
  digitalWrite(enablePin_, LOW);   // TMC2209 EN is active-low
  enabled_ = true;
}

void AccelStepperMotor::disable() {
  digitalWrite(enablePin_, HIGH);
  enabled_ = false;
}

void AccelStepperMotor::run() {
  if (enabled_) {
    stepper_.run();
  }
}

} // namespace scanner
```

The adapter is thin: no decisions, no timing, no retries. All of that lives in
the core and is tested there.

## 12. Review checklist

Before committing, verify:

- [ ] No `Arduino.h` in `lib/scanner_core/`.
- [ ] No `delay()`, `millis()`, `micros()`, `Serial`, `EEPROM`, `digitalRead`
      or `digitalWrite` in core.
- [ ] No `new`, `delete`, `malloc`, `String`, STL containers, exceptions.
- [ ] Functions are small and named after intent.
- [ ] No component includes `ConfigManager.h`; it defines and stores its own
      slice and exposes `applyConfig`.
- [ ] `StateMachine.h` includes no project header.
- [ ] States represent activities; the state machine returns facts, not actions.
- [ ] A class does not receive a manager or facade when a value or slice suffices.
- [ ] Every new behavior has a native test.
- [ ] Error paths return an `ErrorCode` and are tested.
- [ ] No magic numbers; units in names.
- [ ] Comments explain why, not what.
- [ ] Docs updated if behavior changed.
- [ ] `pio run -e uno` and `pio test -e native` pass.
