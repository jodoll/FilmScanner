# ADR-0005 — Config slices and dependency direction

- **Status:** Accepted
- **Date:** 2026-11-07
- **Deciders:** Project owner, firmware architect

## Context

The firmware has one configuration object shared by motion, camera, inputs,
driver setup, logging and status reporting. The first draft implied that
`ScannerController` and the individual components would receive the
`ConfigManager`. That would turn `ConfigManager` into a service locator:

- every component would include `ConfigManager.h` and know about persistence,
  key names and serial concerns;
- unit tests would need a fake config manager even when a component only needs
  two integers;
- live `SET` changes would require callbacks or setters in every component;
- the dependency graph would be wider than the actual data flow.

Most components need only a small, coherent group of values:

- `NotchSensor` and `Button`: debounce and long-press values;
- `Camera`: the four camera timings;
- `MotionController`: seek speed, acceleration and jog speed;
- `ScannerController`: offset, notch timeout, seek budget and status period;
- `TmcStepperDriver`: microsteps and current settings.

## Decision

`Config` is a plain value aggregate split into narrow slices:

```cpp
struct InputConfig  { /* notch/button debounce, long press */ };
struct CameraConfig { /* settle, focus, shutter, post-shot */ };
struct MotionConfig { /* seek, acceleration, jog */ };
struct RunConfig    { /* offset, notch timeout, max seek, status period */ };
struct DriverConfig { /* microsteps, currents, stealthChop */ };

struct Config {
  InputConfig  input;
  CameraConfig camera;
  MotionConfig motion;
  RunConfig    run;
  DriverConfig driver;
  uint8_t      logLevel;
};
```

Dependency rules:

1. `ConfigManager` owns exactly one `Config` instance for the lifetime of the
   program. It handles load, save, reset, key lookup, validation and persistence.
2. Components receive only the slice they need, by `const&`:
   - `NotchSensor(IInput&, const InputConfig&)`
   - `Button(IInput&, const InputConfig&)`
   - `Camera(IOutput& focus, IOutput& shutter, const CameraConfig&)`
   - `MotionController(IStepperMotor&, const MotionConfig&)`
   - `ScannerController(..., const RunConfig&, ...)`
3. No component includes `ConfigManager.h` or receives a `ConfigManager&`.
4. Components read their slice when they need the value. Live `SET` changes are
   therefore visible on the next use without setters or callbacks.
5. `ConfigManager::set()` validates, mutates the existing `Config` in place and
   returns `ConfigSetResult { ErrorCode error; ConfigChange change; }`, where
   `change` is `NONE`, `DRIVER` or `LOG_LEVEL`.
6. `CommandHandler` re-applies the TMC configuration and the logger level for
   those two change types. All other changes need no wiring.
7. `ConfigManager::begin()` and `reset()` mutate the existing `config_`; they
   never replace the object, so the component references stay valid.
8. `ScannerController` may depend on `RunConfig` because it is the orchestrator;
   it still must not depend on `ConfigManager`.

Only the composition root (`main.cpp`) and the serial command handler know about
`ConfigManager`.

## Consequences

Positive:

- Components depend on the smallest type they actually use.
- Unit tests construct a plain `Config` and pass one slice; no fake manager.
- Live changes are automatic and consistent: every component reads the same
  shared values.
- Fewer includes and fewer constructor parameters.
- The persistence/serial boundary stays at the edge of the system.

Negative:

- There are more small structs and a key-to-slice mapping in `ConfigManager`.
- `CommandHandler` owns two explicit apply cases (TMC and logger).
- The `Config` object must never be replaced or copied while components hold
  references to its slices.

## Alternatives rejected

- **Inject `ConfigManager` everywhere.** Simple to write, but couples every
  component to persistence, key names and serial concerns.
- **Pass the whole `Config` to every component.** Better than a manager, but it
  still gives each class far more than it needs and makes tests noisier.
- **Copy values at construction.** Avoids references, but `SET` would update the
  stored config and not the running component. It would require a setter in
  every class.
- **One callback per live key.** Correct but verbose, and it duplicates the
  ownership model that the shared slices already provide.
