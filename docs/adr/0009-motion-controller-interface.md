# ADR-0009 — Motion controller interface and stepper implementation

- **Status:** Accepted
- **Date:** 2026-11-08
- **Deciders:** Project owner, firmware architect

## Context

`ScannerController` needs to start and stop motion, but it must not know how the
motor works. Enabling a driver, choosing microsteps, setting a step rate and
expressing an offset in microsteps are details of the stepper hardware. A
different actuator (for example a DC motor with time-based offsets) should not
require changes in the state machine or the orchestrator.

ADR-0008 already keeps these details out of the state machine. The question is
where they live in the rest of the core.

## Decision

Split motion into an interface and a stepper implementation:

```cpp
class IMotionController {
public:
  virtual ~IMotionController() = default;

  virtual void startSeek() = 0;
  virtual void applyOffset() = 0;
  virtual void startJog(int32_t amount) = 0;
  virtual void stop() = 0;        // controlled deceleration
  virtual void halt() = 0;        // immediate
  virtual void idle() = 0;        // become inactive; may de-energize
  virtual void enable() = 0;      // maintenance override
  virtual void disable() = 0;
  virtual void applyConfig(const MotionConfig& config) = 0;

  virtual void poll() = 0;
  virtual bool isMoving() const = 0;
  virtual bool isEnabled() const = 0;
  virtual int32_t position() const = 0;
};
```

`StepperMotionController` implements it with `IStepperMotor` and
`MotionConfig`. It owns:

- the seek speed, acceleration and jog speed;
- the seek budget and the calibrated offset;
- the enable/disable policy:
  - any move clears the maintenance override and enables the driver;
  - `halt()` clears the override;
  - `idle()` de-energizes unless the override is set;
  - `enable()` / `disable()` are the explicit maintenance operations;
- `poll()`, which calls `IStepperMotor::run()` while enabled.

`ScannerController` depends only on `IMotionController&`. `CommandHandler`'s
`ENABLE`/`DISABLE` commands go through `ScannerController`, which delegates to
the same interface and emits `MOTOR_STATE`.

## Consequences

Positive:

- The state machine and the orchestrator speak in activities (`startSeek`,
  `applyOffset`, `startJog`, `stop`, `halt`, `idle`).
- Offset units, microsteps, speed, acceleration and enable polarity are confined
  to `StepperMotionController` and the HAL.
- `ScannerController` tests use a `FakeMotionController` and assert calls and
  ordering, not pulses.
- `StepperMotionController` is tested against `FakeStepperMotor`.
- A DC-motor variant is a second implementation; `docs/03`, the state machine
  and the serial protocol stay unchanged.

Negative:

- One more interface and one more implementation class.
- `IMotionController` has a wide surface; it is deliberately activity-oriented
  rather than a thin motor wrapper.

## Alternatives rejected

- **`ScannerController` talks to `IStepperMotor` directly.** Fewer types, but it
  pulls speeds, enable and units into the orchestrator.
- **Only a concrete `MotionController`.** Simplest, but the orchestrator depends
  on the stepper implementation and the tests need a simulated motor.
- **One interface per operation group (seek, jog, enable).** More types for no
  practical benefit; the operations belong to one collaborator.
