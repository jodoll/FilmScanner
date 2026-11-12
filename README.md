# Film Scanner

Firmware for an Arduino-based film-negative scanner.

A strip of four negatives sits in a holder. The holder has a toothed rack on the
bottom, driven by a stepper motor through a sprocket. Notches in the rack are
detected by a microswitch; each time the switch closes, the holder is aligned
with the camera. The camera is triggered through two opto-isolated outputs
(focus and shutter). A magazine can feed successive holders, so the machine can
scan a whole film without intervention.

```
12 V ──► TMC2209 ──► NEMA 17 ──► Z20 ──► Z30 ──► rack ──► holder
             ▲                                              │
             │ UART                                         │ notch
        Arduino Uno ◄────────────────────────────────────────┘
             │
             ├──► camera focus / shutter (opto-isolated)
             ├──► status LED (D13)
             └──► run button / cable (momentary, active-low)
```

## Status

**Specification complete — firmware not implemented yet.**

- `docs/` is the complete, implementation-ready specification.
- `src/` contains an early prototype from the first attempt and will be replaced.
- See [AGENTS.md](AGENTS.md) for the implementing agent's entry point.

## What the firmware does

- Moves the holder forward at a controlled, slow speed.
- Stops at each notch, applies a calibrated step offset, lets the mechanics
  settle, focuses the camera, fires the shutter, and repeats.
- Scans continuously; every notch is one photo. When no notch arrives for 15 s
  (configurable), the run ends and the machine waits.
- Stops gracefully with `STOP` / a short button press (finishes the current
  frame) or immediately with `ABORT` / a 2 s long press.
- Is controlled over USB serial with a small ASCII protocol, and configured
  over the same interface. Settings persist in EEPROM.
- Disables the motor in IDLE and FAULT (the holder does not back-drive).

## Hardware summary

| Item | Value |
|---|---|
| Controller | Arduino Uno |
| Stepper driver | BIGTREETECH TMC2209 V1.3 (12 V, UART) |
| Motor | Artillery 42-35 NEMA 17, 200 steps/rev |
| Gearing | Fischertechnik Z20 → Z30 → rack (m = 1.5 mm) |
| Frame pitch | 38 mm (1,290 microsteps at 1/16) |
| Notch | 5 mm wide, NO switch to GND |
| Camera | Focus + shutter, active-high optocoupler outputs |
| Control | Momentary button or cable, active-low |
| Status | Onboard LED on D13 |

Full pinout and wiring: [docs/02-hardware.md](docs/02-hardware.md).
Wiring diagram: [docs/wiring.png](docs/wiring.png) — editable source in
[docs/wiring.puml](docs/wiring.puml) (render with PlantUML).

## Quick start (once firmware exists)

```bash
pio run -e uno -t upload
pio device monitor -b 115200
```

```
> PING
< OK PING
> START
< OK START
< EVT RUN_STARTED
...
> STOP
< OK STOP
< EVT RUN_STOPPED reason=request frames=3
```

## Documentation

| Document | Contents |
|---|---|
| [docs/01-requirements.md](docs/01-requirements.md) | Functional and non-functional requirements, acceptance criteria |
| [docs/02-hardware.md](docs/02-hardware.md) | BOM, pinout, wiring, mechanics, safety |
| [docs/03-architecture.md](docs/03-architecture.md) | Layers, interfaces, state machine, timing model |
| [docs/04-code-style.md](docs/04-code-style.md) | Clean Code rules for this codebase |
| [docs/05-serial-protocol.md](docs/05-serial-protocol.md) | Command reference, events, error codes |
| [docs/06-configuration.md](docs/06-configuration.md) | Config keys, defaults, EEPROM layout |
| [docs/07-stepper-tmc2209-guide.md](docs/07-stepper-tmc2209-guide.md) | Plain-language TMC2209 and stepper guide |
| [docs/08-calibration.md](docs/08-calibration.md) | Commissioning and calibration procedure |
| [docs/09-testing.md](docs/09-testing.md) | Unit test plan and hardware acceptance checklist |
| [docs/10-troubleshooting.md](docs/10-troubleshooting.md) | Symptom → cause → fix |
| [docs/11-implementation-plan.md](docs/11-implementation-plan.md) | Ordered implementation milestones |
| [docs/12-glossary.md](docs/12-glossary.md) | Terms for non-experts |
| [docs/13-sandbox.md](docs/13-sandbox.md) | Sandboxed agent environment and daily workflow |
| [docs/adr/](docs/adr/) | Architecture decision records |

Commit conventions: [CONTRIBUTING.md](CONTRIBUTING.md).

## Documentation checks

After editing the docs, verify internal references with:

```bash
./scripts/check_docs.py
```

See [docs/README.md](docs/README.md#checking-documentation-references) for
details.

## Safety

- Use the 12 V supply for the motor; never power the motor from the Arduino 5 V.
- Do not connect or disconnect the motor while the driver is powered.
- Keep hands clear of the rack and holder while a run is active.
- Use `ABORT` (serial) or a 2 s long press for an immediate stop.
- The firmware enables a 2 s watchdog; a hang resets the board and disables the
  motor.
