# ADR-0007 — Config ownership, aggregation and push

- **Status:** Accepted
- **Date:** 2026-11-08
- **Supersedes:** [ADR-0005](0005-config-slices-and-dependency-direction.md)
- **Deciders:** Project owner, firmware architect

## Context

ADR-0005 made `ConfigManager` own a single aggregate `Config` and let
components read narrow slices through references into that object. That kept
components away from the manager, but it had two consequences the project no
longer wants:

- a component's configuration was defined centrally, in `Config.h`, rather than
  by the class that understands the values;
- live changes were implicit: a component picked up a new value whenever it
  next read its slice, with no explicit application point.

The project wants each class to own the definition and the runtime values of its
own configuration, and wants a change to be applied explicitly so the class can
react to it.

## Decision

### Slice per consumer

Each configuration slice is defined next to the class that uses it and has
exactly one consumer:

| Slice | Consumer | Fields |
|---|---|---|
| `NotchConfig` | `NotchSensor` | notch debounce |
| `ButtonConfig` | `Button` | button debounce, long-press time |
| `CameraConfig` | `Camera` | settle, focus, shutter, post-shot |
| `MotionConfig` | `IMotionController` | seek speed, acceleration, jog speed, offset, seek budget |
| `RunConfig` | `ScannerController` | notch timeout, status period |
| `DriverConfig` | `ITmcDriver` | microsteps, currents, stealthChop |
| `LogConfig` | `Logger` | log level |

Offsets and the seek budget live in `MotionConfig` because they are expressed in
motion-layer units. The protocol keys (`offsetSteps`, `maxSeekSteps`) do not
change.

### Ownership and push

1. `ConfigManager` owns the aggregate `Config` — the single authoritative copy —
   and the key-to-slice mapping.
2. Each component owns its own stored values and exposes
   `void applyConfig(const XConfig&)`.
3. `ConfigManager::set(key, value)` validates, mutates the aggregate, and calls
   exactly one `applyConfig(...)` on the owning component. A rejected set
   changes nothing and pushes nothing.
4. `begin()` loads EEPROM into the aggregate, validates, then pushes every slice
   once. `reset()` writes defaults into the aggregate, pushes everything, then
   saves.
5. `save()` serialises the aggregate, never component state.
6. `get()` and `CONFIG` read the aggregate.
7. Values are never read back from components. The aggregate is the source of
   truth; components hold caches fed by pushes.
8. `applyConfig` must accept the value as validated. Components must not clamp
   or normalise, or the cache and the truth diverge.
9. `ConfigChange` and `ConfigSetResult` are removed. `set()` returns
   `ErrorCode`; the push *is* the application.

### Reactions

`applyConfig` stores the values and may react immediately:

- `NotchSensor` / `Button`: apply to the next update; `SET` is only allowed in
  `IDLE`, so no press is in progress.
- `Camera`: applies to the next sequence, never the active one.
- `IMotionController`: applies to the next move.
- `ScannerController`: applies to the next seek, status check or frame.
- `ITmcDriver`: rewrites the driver registers immediately.
- `Logger`: changes the level immediately.
- `ConfigManager` logs the offset-recalibration warning for `microsteps`,
  `seekSpeed` and `acceleration`.

### Dependency direction

`ConfigManager → components`. Components never include `ConfigManager.h` and
never receive it. Only `main.cpp` constructs the manager, so the wide edge stays
at the composition boundary. The manager includes the component headers it
pushes into; the aggregate `Config` is assembled from the slice headers and does
not require the component headers.

### Persistence

The persisted record is a separate representation, not assembled from the slice
types; see [ADR-0010](0010-persistence-record-is-a-separate-representation.md).

## Consequences

Positive:

- Each class owns the data it understands and the reaction to a change.
- Changes are explicit and observable; there is no hidden read-later coupling.
- The manager is the only place that knows keys, validation and persistence.
- Components remain testable with a plain slice value and an `applyConfig` call.
- A component can be swapped (for example a different motion implementation)
  without moving its configuration into a central blob.

Negative:

- The aggregate and the component caches can diverge if `applyConfig` is
  skipped or if a component clamps values. Both are forbidden by the rules
  above.
- `ConfigManager` has a wide constructor and includes every consumer header.
  This is accepted because only the composition root constructs it.
- There are more small files (one slice definition per consumer).

## Alternatives rejected

- **Manager owns the slices and components read references (ADR-0005).** Simple,
  but the definition is central and changes are implicit.
- **Components are the only source of truth and the manager reads back on save.**
  Avoids duplication, but makes `SAVE` depend on every component's getter and
  blurs who validates.
- **A callback per key.** Correct but verbose, and it duplicates the ownership
  that the slice plus `applyConfig` already provides.
