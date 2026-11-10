# AGENTS.md — Film Scanner Firmware

This repository contains the **specification** for the firmware of a film-negative
scanner. The firmware runs on an Arduino Uno, moves a negative holder with a
stepper motor, detects frame notches with a microswitch, and triggers a camera
once per negative.

**Your job as an implementing agent:** turn this specification into working,
tested firmware. Read this file first, then follow the read order below.

---

## Status

- The specification is complete. No production firmware exists yet. The
  declarations in `lib/scanner_core/include/scanner_core/` are in place as the
  contract: interfaces, vocabulary and config slices, with no logic.
- `src/main.cpp` is an early prototype from the first attempt. **Replace it.** Do
  not extend it.
- `platformio.ini` is up to date: TMCStepper, AccelStepper, and a `native` test
  environment.
- **Documentation is the source of truth for behavior.** The wire format, the
  config keys and ranges, the persistence layout and the acceptance criteria are
  defined in `docs/`. If code and docs disagree about behavior, the docs win
  until the docs are deliberately updated.
- **The headers are the source of truth for declarations.** Signatures, struct
  fields and enum members live in `lib/scanner_core/include/scanner_core/`. Do
  not copy them into the docs; change them freely while the tests pass.

---

## Read order

1. [docs/01-requirements.md](docs/01-requirements.md) — what the machine must do
2. [docs/02-hardware.md](docs/02-hardware.md) — pins, wiring, mechanics
3. [docs/03-architecture.md](docs/03-architecture.md) — layers, interfaces, state machine
4. [docs/04-code-style.md](docs/04-code-style.md) — how the code must look
5. [docs/05-serial-protocol.md](docs/05-serial-protocol.md) — host interface
6. [docs/06-configuration.md](docs/06-configuration.md) — config keys and EEPROM
7. [docs/07-stepper-tmc2209-guide.md](docs/07-stepper-tmc2209-guide.md) — plain-language driver guide
8. [docs/08-calibration.md](docs/08-calibration.md) — commissioning procedure
9. [docs/09-testing.md](docs/09-testing.md) — unit tests and hardware acceptance
10. [docs/11-implementation-plan.md](docs/11-implementation-plan.md) — ordered work plan

Use [docs/12-glossary.md](docs/12-glossary.md) when a term is unclear.

---

## Documentation changes

The docs are the source of truth for behavior and the host contract, and they are
changed deliberately.

- Do not edit anything under `docs/` or this file without explicit approval.
- Do not paste interfaces, structs or enums into `docs/`. Link to the header
  instead. The docs state behavior, names, ownership and requirements.
- If a rule, name or behavior is missing, ambiguous, or contradicts the code,
  stop and report:
  1. the exact gap or contradiction,
  2. the options you see,
  3. the specific doc change you recommend (file and section).
- Wait for approval before changing the doc and before implementing the
  affected behavior.
- Never encode an unapproved decision in code, a comment, a test name or a
  prompt. Do not invent a local variant.
- Once approved, update the doc first, then the code, in the same change or a
  preceding commit.
- You may continue with unrelated, unambiguous work while waiting.

---

## Non-negotiable design decisions

These were agreed with the project owner. Do not change them without updating
the docs and the decision records in `docs/adr/`.

1. **Target:** Arduino Uno (ATmega328P, 16 MHz, 2 KB RAM, 32 KB flash).
2. **Driver:** BIGTREETECH TMC2209 V1.3, UART configuration.
3. **Motor:** Artillery 42-35 NEMA 17 (200 full steps/rev).
4. **Libraries:** `teemuatlut/TMCStepper@0.7.3` for TMC2209 configuration,
   `waspinator/AccelStepper@1.64` for STEP/DIR pulse generation,
   GoogleTest for native unit tests.
5. **STEP/DIR, never `VACTUAL`.** The TMCStepper library configures the driver
   only; AccelStepper generates the pulses. `VACTUAL` has no position tracking
   and cannot stop at an exact offset.
6. **Pure core:** all logic lives in `lib/scanner_core/` and must not include
   `Arduino.h`. Hardware is reached only through the interfaces listed in
   `docs/03-architecture.md`.
7. **Non-blocking:** no `delay()` in core logic. Everything is driven by
   `tick(uint32_t nowMs)` and deadline comparisons.
8. **No heap, no exceptions, no RTTI, no STL containers, no Arduino `String`.**
   Fixed buffers and plain C++11 only.
9. **Positioning is sensor-first:** the notch switch is the reference. Steps are
   counted for diagnostics and for the post-notch offset, but the next frame is
   always re-referenced by the sensor.
10. **Offset is applied after the controlled stop**, not at the instant the
    switch closes. Changing seek speed, acceleration, or microsteps invalidates
    the calibrated offset.
11. **The control input is a momentary active-low contact only.** A button or a
    cable used like a button. There is no switch/level mode and no auto-start at
    boot. Short press acts on release; a 2 s long press fires while held.
12. **Graceful stop vs. abort:** `STOP` / short press finishes the current frame
    and photo during a run, but stops immediately during manual moves. `ABORT` /
    long press stops immediately in every state.
13. **Motor is disabled in IDLE and FAULT.** The holder does not back-drive when
    de-energized, so holding current is not needed while idle. `StepperMotionController`
    owns this policy; the orchestrator only calls `idle()`. The explicit
    `ENABLE` maintenance command is the only exception: it keeps the driver
    energized in `IDLE` until `DISABLE`, any motion command, `ABORT`, or a fault.
14. **Explicit persistence:** `SET` changes RAM only; `SAVE` writes EEPROM.
15. **Config ownership and push:** each consumer defines and stores its own
    config slice (`NotchConfig`, `ButtonConfig`, `CameraConfig`, `MotionConfig`,
    `RunConfig`, `DriverConfig`, `LogConfig`). `ConfigManager` owns the aggregate
    and pushes changes through `applyConfig`. Never inject `ConfigManager` into a
    component. See [ADR-0007](docs/adr/0007-config-ownership-and-push.md).
16. **Pure activity state machine:** `StateMachine` has no project dependencies,
    states are activities (`SEEK`, `STOPPING`, `OFFSET`, `CAMERA`, `JOG`), one
    method per event, and the result is `{state, error, reason, announcements}`.
    Entry/exit actions live in `ScannerController`. See
    [ADR-0008](docs/adr/0008-pure-state-machine-and-orchestrator.md).
17. **Motion seam:** `ScannerController` talks to `IMotionController` in
    activities (`startSeek`, `applyOffset`, `startJog`, `stop`, `halt`, `idle`);
    `StepperMotionController` owns enable polarity, units and pulses. See
    [ADR-0009](docs/adr/0009-motion-controller-interface.md).

---

## Build and test commands

```bash
# Compile for the Uno
pio run -e uno

# Run all native unit tests (no hardware needed)
pio test -e native

# Upload
pio run -e uno -t upload

# Serial monitor (115200 8N1)
pio device monitor -b 115200

# Check Markdown links, anchors and code fences
./scripts/check_docs.py
```

Native tests must pass on a clean checkout before any hardware testing.
There is no CI workflow; run the commands locally.

---

## Repository layout

```
.
├── AGENTS.md                  ← you are here
├── README.md                  ← human overview
├── platformio.ini             ← build environments
├── scripts/
│   └── check_docs.py          ← documentation reference checker
├── docs/                      ← the specification (behavior source of truth)
│   ├── adr/                   ← architecture decision records
│   └── ...
├── lib/
│   └── scanner_core/          ← pure C++ logic (no Arduino.h)
│       ├── include/scanner_core/
│       └── src/
├── src/                       ← Arduino-only code
│   ├── main.cpp               ← composition root
│   └── hal/                   ← Arduino implementations of the interfaces
└── test/
    ├── test_native/           ← GoogleTest tests
    ├── fakes/                 ← test doubles
    └── ...
```

---

## Inspecting files

Look things up with the dedicated tools, not with shell text-processing
pipelines:

- Use `read` with `offset` / `limit` to get specific lines of a file. Do not
  reach for `sed -n`, `head`, `tail`, `cat` or `awk` for that.
- Use `grep` to search file contents and `glob` to find paths. Do not pipe
  their output through `sed` to indent or otherwise decorate it.
- Use `read` on a directory to list its entries, or `glob` to match paths. Do
  not shell out to `ls`, `find` or `tree`: the dedicated tools carry the path
  and permission rules, and shelling out either prompts or bypasses them.
- `sed` is a last resort and may only print. Never change files from the shell:
  no `sed -i`, no `sed 'w …'`, no `>` redirects. Every file change goes through
  `edit` / `write`, so it is visible as a diff and can be reviewed.

---

## Coding rules (summary)

- Clean Code in the Uncle Bob sense: small functions, one level of abstraction,
  intention-revealing names, no boolean flag parameters, guard clauses,
  dependency inversion, command/query separation.
- **TDD:** write a failing native test first for every behavior in
  `scanner_core`; every public method and every error path needs a direct unit
  test. Do not batch tests at the end. HAL/Arduino integration is covered by the
  manual hardware acceptance checklist, not by host unit tests.
- **Test conventions** (one case per `TEST`/`TEST_F`/`TEST_P`, parameterized
  values, assertion choice, fakes) are defined in
  [docs/09-testing.md §1](docs/09-testing.md#1-strategy). Follow that document;
  do not invent a local variant.
- Every public class has a single responsibility and is testable in isolation;
  use fakes only where an interface exists.
- **Depend on the narrowest type.** Do not pass a manager or facade when a value
  or small config struct is enough. Components define and store their own slice;
  they must not include `ConfigManager.h`.
- **States are activities, not effects.** Entry/exit actions belong to the
  orchestrator; the pure machine returns facts, never action lists.
- Errors are values (`ErrorCode` / `Status`), never exceptions.
- The core may use `int32_t`, `uint32_t`, `uint16_t`, `uint8_t`, `enum class`,
  fixed arrays, and simple structs. Nothing else from Arduino.
- The HAL may use Arduino APIs, but must be thin and dumb.
- The main loop must stay fast; `motion.poll()` is called every iteration.
- Use `F()` for constant strings in the HAL. Core logging uses fixed buffers.

Full rules: [docs/04-code-style.md](docs/04-code-style.md).

---

## Commits

Never commit without asking. A commit is due when the user asks for one, or
when the active workflow or skill (for example the Core TDD skill) says the
current unit is ready.

1. Finish the current logical unit and run the checks that apply to it.
2. Inspect `git status --short` and `git diff` (and `git diff --cached` when
   something is already staged).
3. Ask with the `question` tool for the scope and the message. Propose the
   changed files as the default scope and one Conventional Commit message in
   the style of
   [docs/11-implementation-plan.md §15](docs/11-implementation-plan.md#15-suggested-commit-sequence).
   Skip the question only when the user already supplied both the exact files
   and the exact message.
4. Run `git add <chosen files>` and `git commit -m "<chosen message>"`. The
   `ask` rule on `git commit *` is the final approval gate.
5. Never push, never use `--no-verify`, and never edit the permission rules to
   bypass the gate. If the user rejects, stop and report.

"Commit before we continue" means: ask and commit in the same run, then
continue. Do not end the turn to wait for a new prompt.

`question` is denied for the `general` subagent, so keep commit handling in
the main session.

---

## Definition of done

- `pio run -e uno` compiles without warnings from project code.
- `pio test -e native` passes.
- `./scripts/check_docs.py` passes after documentation changes.
- All commands in `docs/05-serial-protocol.md` behave as specified, including
  error codes and events.
- The hardware acceptance checklist in [docs/09-testing.md](docs/09-testing.md)
  passes on the real machine.
- The calibration procedure in [docs/08-calibration.md](docs/08-calibration.md)
  has been executed and the resulting values are recorded.
- The docs are updated if any behavior changed during implementation.

---

## Gotchas / things that commonly go wrong

- **TMCStepper does not generate steps.** It only writes registers. Without
  AccelStepper (or another STEP/DIR generator) the motor will not move.
- **`EN` is active-low** on the TMC2209. `digitalWrite(EN, LOW)` enables.
- **Both switches are active-low** with `INPUT_PULLUP`: notch and control input.
- **A cable used as a button is not a run-enable input.** It is a momentary
  contact. Leaving it permanently connected is a held button; it will trigger
  the 2 s action after boot and nothing else until it is released and pressed
  again.
- **No auto-start at boot.** A contact held at power-on is ignored until it is
  released.
- **The 15 s notch timeout applies only in `SEEK` for RUN and SINGLE.** `JOG`
  never times out.
- **The camera pulse is never cut mid-pulse.** Abort during a shutter pulse
  completes the pulse, then stops.
- **Changing `microsteps` invalidates `offsetSteps`.** Recalibrate.
- **`SAVE` is required** after `SET`; otherwise the value is RAM-only.
- **Do not power the motor from 5 V.** Use the 12 V supply.
- **Do not disconnect the motor while the driver is powered.**
- **Watchdog:** 2 s WDT; kick it once per loop. Clear `MCUSR` on boot to avoid a
  reset loop on some bootloaders.
- **Do not inject `ConfigManager` into `NotchSensor`, `Button`, `Camera`,
  `StepperMotionController` or `ScannerController`.** Each defines and stores
  its own slice; `ConfigManager` pushes through `applyConfig`. Only `main.cpp`
  and `CommandHandler` know `ConfigManager`.
- **Do not put hardware effects in the state machine.** It has no enable, start,
  stop or camera call. `ScannerController` runs the entry/exit actions.
- **Do not put transition logic in `ScannerController`.** It translates inputs,
  runs entry/exit actions and formats events; the pure machine owns the
  sequence.
- **Enabling the motor is inside `StepperMotionController`.** `ScannerController`
  never touches `EN`, microsteps or current. The maintenance `ENABLE`/`DISABLE`
  commands go through the controller to the motion interface.
- **`SET` already applied the change.** `ConfigManager::set` pushes the new value
  before it returns; `CommandHandler` must not re-apply it.
- **Do not copy declarations into the docs.** Signatures, struct fields and enum
  members live in the headers under `lib/scanner_core/include/`. If the docs and
  a header disagree about a declaration, the header wins; if they disagree about
  behavior, the docs win.
- **There is no `ALIGN` state.** The stop and the offset are `STOPPING` and
  `OFFSET`.
- **Do not hide test cases in a loop.** A `TEST` with `for (value : values)` is
  one reported case and stops at the first fatal failure. Use `TEST_P` so every
  value is its own case (see
  [docs/09-testing.md §1.2](docs/09-testing.md#12-one-test-case-per-test)).
- **Do not paste ad hoc Python reference checkers into the shell.** Run
  `./scripts/check_docs.py`; it is the committed, documented checker for docs.

---

## Where to start

Follow [docs/11-implementation-plan.md](docs/11-implementation-plan.md) in order.
It is deliberately **core-first and class by class**: setup, then the pure state
machine, then one collaborator at a time (motion, notch, camera, button), then
the orchestrator, config, parser, LED and HAL. Work one failing test at a time.
Do not skip the native tests; they are the fastest way to validate the logic
without the machine.
