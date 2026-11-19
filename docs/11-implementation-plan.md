# 11 — Implementation plan

The order is MVP first: three milestones, and the firmware is the MVP
(version 1.0) when all three pass on the machine. Within a milestone, implement
one unit at a time, pure core before hardware adapters. A milestone is done when
every capability below passes on hardware.

## How to work

- One unit at a time: failing native test → minimum implementation → commit.
  Never batch tests at the end.
- Core logic has no Arduino dependencies and no real time. Hardware adapters are
  written only in the last step of a milestone.
- The concept docs define behavior; declarations live in headers. Choose the
  decomposition freely; do not treat this plan as a class or method list.

## Milestone 1 — Motor moves under button control

1. The motor can be commanded to move continuously, decelerate to a stop, stop
   immediately, and idle de-energized; a maintenance override can keep it
   energized. Tested on the host against a fake motor.
2. The control input is debounced: short press acts on release, a 2 s press
   fires while held, and a contact held at boot is ignored until released.
3. Start, stop and errors are reported as log lines on serial.
4. Boot configures the driver over UART, the watchdog runs, the main loop stays
   non-blocking.
5. Acceptance: short press starts continuous motion, short press again stops,
   long press aborts, the motor is de-energized at rest.

*Until the notch input exists (milestone 2), a short press starts an open-ended
manual move instead of a run. This is the only planned deviation from FR-07 and
is removed in milestone 2.*

## Milestone 2 — Motion follows the notch sensor

1. The notch input is debounced and produces exactly one event per closure.
2. A short press starts a run per FR-07: seek forward, controlled stop at the
   next notch, then the post-notch offset.
3. A run that sees no notch in time stops and returns to idle with a reported
   reason.
4. Acceptance: every real closure produces a controlled stop, the stop position
   is repeatable, the timeout fires with no strip. The offset stays 0 until
   calibration.

## Milestone 3 — Camera completes the frame

1. The focus and shutter outputs run the settle → focus → shutter → post
   sequence with configurable timings. Abort never truncates an active shutter
   pulse and prevents a pending one.
2. A run takes exactly one photo per notch.
3. Acceptance: a strip scans end to end using only the button. **This completes
   the MVP (version 1.0).**

## After the MVP — reassess

Re-plan on the bench. Candidates in rough order:

- Run bookkeeping and status reporting (counters, snapshot, event stream).
- Serial commands and the full protocol surface.
- Configuration: validation, persistence, pushes to consumers, log levels.
- Calibration ([08](08-calibration.md)), the full acceptance checklist
  ([09 §8](09-testing.md#8-hardware-acceptance-checklist)), a 36-frame run.
- Status LED.
