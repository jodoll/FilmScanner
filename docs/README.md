# Documentation index

This directory is the implementation-ready specification for the film-scanner
firmware. Read in this order:

| # | Document | Purpose |
|---|---|---|
| 01 | [Requirements](01-requirements.md) | Scope, functional/non-functional requirements, acceptance criteria |
| 02 | [Hardware](02-hardware.md) | Components, pinout, wiring, mechanics, power, safety |
| 03 | [Architecture](03-architecture.md) | Layering, interfaces, state machine, timing, memory |
| 04 | [Code style](04-code-style.md) | Clean Code and testing rules |
| 05 | [Serial protocol](05-serial-protocol.md) | Host command reference and event/error formats |
| 06 | [Configuration](06-configuration.md) | Config keys, defaults, ranges, EEPROM layout |
| 07 | [Stepper & TMC2209 guide](07-stepper-tmc2209-guide.md) | Plain-language explanation of every driver setting |
| 08 | [Calibration](08-calibration.md) | Step-by-step commissioning on the real machine |
| 09 | [Testing](09-testing.md) | Native unit tests, fakes, hardware acceptance checklist |
| 10 | [Troubleshooting](10-troubleshooting.md) | Symptom → cause → action |
| 11 | [Implementation plan](11-implementation-plan.md) | Core-first, class-by-class milestones |
| 12 | [Glossary](12-glossary.md) | Terms used in the docs |

Wiring diagram:

- [wiring.png](wiring.png) / [wiring.svg](wiring.svg) — rendered diagram.
- [wiring.puml](wiring.puml) — editable PlantUML source covering Arduino,
  TMC2209, motor, switches, camera interface and power distribution.

Architecture decision records:

- [ADR-0001 — Library stack](adr/0001-library-stack.md)
- [ADR-0002 — STEP/DIR and sensor-referenced positioning](adr/0002-step-dir-and-positioning.md)
- [ADR-0003 — Configuration persistence](adr/0003-config-persistence.md)
- [ADR-0004 — ASCII serial protocol](adr/0004-serial-protocol.md)
- [ADR-0005 — Config slices and dependency direction](adr/0005-config-slices-and-dependency-direction.md)
  (superseded by ADR-0007)
- [ADR-0006 — GoogleTest for native unit tests](adr/0006-googletest-for-native-tests.md)
- [ADR-0007 — Config ownership and push](adr/0007-config-ownership-and-push.md)
- [ADR-0008 — Pure activity state machine and orchestrator](adr/0008-pure-state-machine-and-orchestrator.md)
- [ADR-0009 — Motion controller interface](adr/0009-motion-controller-interface.md)

The top-level [AGENTS.md](../AGENTS.md) is the entry point for implementing
agents. [README.md](../README.md) is the human overview.

## Conventions used in these docs

- **MUST / MUST NOT** = required for correctness or safety.
- **SHOULD** = expected; deviate only with a documented reason.
- Pin names are Arduino pin numbers (`D2` = digital pin 2).
- Times are milliseconds unless explicitly marked otherwise.
- Motor distances are in microsteps at the configured microstep setting.
- `µsteps` is shorthand for microsteps.
- "RUN" is an automatic continuous run; "SINGLE" is the `STEP` command;
  "JOG" is a manual move.
- "Frame" is one negative; "holder" holds four frames; "magazine" feeds
  successive holders.
