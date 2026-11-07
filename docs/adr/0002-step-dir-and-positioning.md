# ADR-0002 — STEP/DIR and sensor-referenced positioning

- **Status:** Accepted
- **Date:** 2026-11-07
- **Deciders:** Project owner, firmware architect

## Context

The holder must stop at each notch, apply a signed offset, and take a photo. The
notch microswitch is the only position sensor; there is no encoder, home switch,
or end-of-travel switch. The rack has 38 mm between notch starts and a 5 mm
notch.

The TMC2209 offers two ways to move:

- **`VACTUAL`** — the driver generates its own velocity internally. The Arduino
  writes a velocity and the driver ramps.
- **STEP/DIR** — the Arduino generates one pulse per microstep; the driver
  follows.

`VACTUAL` has no position counter and its stop behavior is not tied to an exact
step position. STEP/DIR lets the firmware know exactly where it is and stop at a
computed target.

## Decision

Use **STEP/DIR** with AccelStepper. Positioning is **sensor-first**:

1. Move forward at `seekSpeed` toward a far target.
2. On the notch closure, command a controlled deceleration (`stop()`).
3. After the stop, move `offsetSteps` (signed) and wait.
4. Settle, then run the camera sequence.
5. Repeat; each frame is re-referenced by the notch sensor.

The offset is applied after the controlled stop. It therefore absorbs the
stopping distance and the mechanical gate alignment. It depends on `seekSpeed`,
`acceleration` and `microsteps`; changing any of them requires recalibration.

## Consequences

Positive:

- Exact, repeatable stop position after calibration.
- Step counting is available for diagnostics and for the offset.
- Step loss during travel does not accumulate: the next notch re-references the
  holder. Only the short post-notch offset move is open-loop.
- No encoder or home switch is needed.

Negative:

- The offset is speed/acceleration dependent; a speed change invalidates it.
- The stop distance must fit inside the 5 mm notch (1.18 mm at defaults).
- The firmware depends on AccelStepper's position tracking; if steps are lost
  after the notch (during the offset move), the frame is misaligned. The move is
  slow and short, so this is unlikely.
- `maxSeekSteps` is a safety net only; the 15 s timeout is the primary stop
  criterion.

## Alternatives rejected

- **VACTUAL velocity mode:** no exact stop; cannot apply a step offset reliably.
- **Fixed steps per frame:** mechanically the sensor is the truth; a fixed count
  would drift with belt/gear tolerances and step loss.
- **Encoder/home switch:** unnecessary hardware for this mechanism.
