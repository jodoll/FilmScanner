# ADR-0003 — Configuration persistence

- **Status:** Accepted
- **Date:** 2026-11-07
- **Deciders:** Project owner, firmware architect

## Context

The machine must be tunable without recompiling: current, speed, acceleration,
offset, timings, debounce, timeout, log level. The Uno has 1,024 bytes of
EEPROM, rated for about 100,000 write cycles per byte. A power loss during a
write must not leave the firmware unable to boot.

Options:

1. **Compile-time constants only** — simple, but requires a rebuild for every
   calibration change.
2. **Single EEPROM record with magic/version/length/CRC** — simple, robust
   against corruption, not atomic.
3. **Two alternating records with sequence numbers** — atomic against power loss,
   more code and EEPROM use.
4. **External FRAM/SD** — unnecessary hardware.

## Decision

Use a **single packed record at EEPROM address 0** with:

- magic `0x4653` ("FS"),
- version `1`,
- length,
- CRC16-CCITT over the preceding bytes,
- all writable config fields.

Persistence is **explicit**: `SET` changes RAM; `SAVE` writes EEPROM;
`RESET_CONFIG` restores defaults and writes. There is no auto-save.

On load, any invalid field (magic, version, length, CRC, range) causes defaults
to be loaded with `LOG WARN`; the firmware always boots.

## Consequences

Positive:

- Calibration is persistent and survives power cycles.
- Corruption is detected by CRC.
- EEPROM wear is bounded by operator `SAVE` actions.
- A version bump can either migrate or fall back to defaults; the current
  version falls back.
- The record is small (47 bytes), leaving the rest of EEPROM free.

Negative:

- A power loss during `SAVE` can corrupt the record; the next boot falls back to
  defaults. The operator must re-enter values. This is acceptable for a
  single-machine tool.
- There is no automatic backup slot.
- Changing `microsteps` changes the meaning of `offsetSteps`; the firmware warns
  but does not rescale.

## Alternatives rejected

- **Compile-time only:** too slow to calibrate.
- **Dual-slot journal:** more code and RAM for a failure mode that is rare and
  recoverable (defaults + re-enter).
- **Auto-save on every `SET`:** unnecessary EEPROM wear and risk during a run.
