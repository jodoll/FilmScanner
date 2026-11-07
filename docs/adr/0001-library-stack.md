# ADR-0001 — Library stack

- **Status:** Accepted
- **Date:** 2026-11-07
- **Deciders:** Project owner, firmware architect

## Context

The firmware must configure a TMC2209 over UART, generate STEP/DIR pulses with
acceleration and exact position tracking, and run logic tests on a PC. The target
is an Arduino Uno (16 MHz, 32 KB flash, 2 KB RAM).

The existing prototype used `janelia-arduino/TMC2209@10.1.1`. That library only
writes `VACTUAL` for motion and does not generate STEP pulses, so it cannot
support the post-notch offset.

Options considered:

1. **Janelia TMC2209 + custom step timer** — configuration is limited;
   step generation would have to be written from scratch.
2. **TMCStepper + AccelStepper** — mature libraries with the right split:
   configuration vs. pulse generation.
3. **TMCStepper + FastAccelStepper** — faster step generation, but more complex
   and more flash/RAM; the machine only needs 400 µsteps/s.
4. **Unity vs. GoogleTest** — Unity is lighter and Arduino-friendly; GoogleTest
   is heavier and unnecessary for this size.

## Decision

Use:

- **TMCStepper 0.7.3** (`teemuatlut/TMCStepper`) for TMC2209 UART configuration
  and diagnostics.
- **AccelStepper 1.64** (`waspinator/AccelStepper`) for STEP/DIR pulse
  generation, wrapped behind `IStepperMotor`.
- **Unity** on PlatformIO's `native` platform for unit tests.

## Consequences

Positive:

- The configuration and motion concerns are separated cleanly.
- TMCStepper provides mA-based current setting, microsteps, stealthChop,
  `test_connection()` and fault readback.
- AccelStepper is well understood, non-blocking, and fast enough
  (~4,000 steps/s on a 16 MHz Uno; the machine uses 400).
- The `IStepperMotor` abstraction allows replacing AccelStepper later without
  touching the state machine.

Negative:

- TMCStepper 0.7.3 is old (2021) but stable and widely used; no newer release is
  required for the TMC2209.
- AccelStepper has no hardware timer; it is polled. The loop must stay fast. At
  400 µsteps/s this is easy (2.5 ms per step).
- SoftwareSerial at 115200 is marginal for readback; one-wire TX-only is the
  required fallback.
- The two libraries add flash usage; the build must be measured.

## Alternatives rejected

- **Janelia TMC2209 alone:** cannot generate steps; the offset feature is
  impossible without a second mechanism.
- **VACTUAL velocity mode:** no position tracking, no exact stop; see ADR-0002.
- **FastAccelStepper:** unnecessary speed for this machine, more complexity and
  resource use.
- **GoogleTest:** heavier than needed for a 2 KB-RAM target's logic tests.
