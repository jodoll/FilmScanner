# 06 — Configuration

All values are changeable over the serial protocol and persisted in EEPROM with
`SAVE`. The pin map is compile-time only.

## 1. Keys

### 1.1 Writable keys

| Key | Type | Unit | Default | Range | Live behavior |
|---|---|---|---|---|---|
| `microsteps` | uint16 | µsteps/full step | `16` | `1,2,4,8,16,32,64` | Immediately pushed to the TMC driver |
| `runCurrentMa` | uint16 | mA RMS | `900` | `100..2000` | Immediately pushed to the TMC driver |
| `holdCurrentPercent` | uint8 | % of run current | `50` | `0..100` | Immediately pushed to the TMC driver |
| `stealthChop` | bool | — | `1` | `0,1` | Immediately pushed to the TMC driver |
| `seekSpeed` | uint16 | µsteps/s | `400` | `50..2000` | Next move |
| `acceleration` | uint16 | µsteps/s² | `2000` | `100..20000` | Next move |
| `jogSpeed` | uint16 | µsteps/s | `200` | `50..1000` | Next jog |
| `offsetSteps` | int32 | µsteps | `0` | `-5000..5000` | Next frame |
| `notchDebounceMs` | uint16 | ms | `10` | `2..100` | Pushed; applies to the next `update()` |
| `buttonDebounceMs` | uint16 | ms | `20` | `5..100` | Pushed; applies to the next `update()` |
| `longPressMs` | uint16 | ms | `2000` | `500..10000` | Pushed; applies to the next `update()` |
| `notchTimeoutMs` | uint16 | ms | `15000` | `1000..60000` | Pushed; next seek |
| `maxSeekSteps` | uint32 | µsteps | `0` | `0..1000000` | Pushed; next seek |
| `settleMs` | uint16 | ms | `200` | `0..5000` | Pushed; next camera sequence |
| `focusMs` | uint16 | ms | `300` | `0..5000` | Pushed; next camera sequence |
| `shutterMs` | uint16 | ms | `150` | `10..2000` | Pushed; next camera sequence |
| `postShotMs` | uint16 | ms | `300` | `0..5000` | Pushed; next camera sequence |
| `logLevel` | uint8 | — | `2` | `0..3` | Immediately pushed to the logger |
| `statusPeriodMs` | uint16 | ms | `0` | `0..60000` | Pushed; next `tick()` |

`maxSeekSteps = 0` disables the step-budget safety net; the notch timeout is
then the only stop criterion. This is the default.

`logLevel`: `0 = ERROR`, `1 = WARN`, `2 = INFO`, `3 = DEBUG`.
`statusPeriodMs = 0` disables periodic status lines.

At runtime `ConfigManager` owns one aggregate `Config` — the authoritative copy.
Each consumer defines its own slice, stores the applied values, and exposes
`applyConfig`. A change is validated against the aggregate and then **pushed** to
the owning consumer, which stores it and may react. Components never receive
`ConfigManager`, never read a shared object, and never copy values at
construction. See [§5](#5-live-updates-and-ownership).

### 1.2 Read-only keys

| Key | Example | Meaning |
|---|---|---|
| `fw` | `0.1.0` | Firmware version from the build flag |
| `proto` | `1` | Protocol version |
| `pin.notch` | `3` | Notch switch pin |
| `pin.control` | `6` | Button/cable pin |
| `pin.shutter` | `2` | Camera shutter pin |
| `pin.focus` | `10` | Camera focus pin |
| `pin.step` | `7` | TMC STEP pin |
| `pin.dir` | `8` | TMC DIR pin |
| `pin.enable` | `9` | TMC EN pin |
| `pin.led` | `13` | Status LED pin |
| `pin.tmcTx` | `5` | TMC UART TX pin |
| `pin.tmcRx` | `4` | TMC UART RX pin (optional) |
| `stepsPerMm` | `33.953` | Derived from the gearing and `microsteps` |
| `expectedFrameSteps` | `1290` | `round(38 mm × stepsPerMm)` |

`stepsPerMm` and `expectedFrameSteps` update automatically when `microsteps`
changes. They are diagnostics only; the motor is driven by the notch sensor.

## 2. Defaults rationale

| Key | Why this default |
|---|---|
| `microsteps = 16` | 0.029 mm resolution, smooth low-speed motion, 1,290 µsteps/frame — far below the Uno/AccelStepper limit |
| `runCurrentMa = 900` | BTT V1.3 factory equivalent (~0.9 A RMS); conservative for a 42-35 motor at 12 V |
| `holdCurrentPercent = 50` | Enough holding torque between frames without excessive heat |
| `stealthChop = 1` | Quietest mode at 7.5 rpm; plenty of torque at this speed |
| `seekSpeed = 400` | 11.8 mm/s → 3.4 s per 38 mm frame; large torque margin, 424 ms notch window |
| `acceleration = 2000` | 0.2 s ramp, 1.18 mm; smooth, no jerk, not limited by motor torque |
| `jogSpeed = 200` | Slow, precise manual positioning |
| `offsetSteps = 0` | Calibrated on the real machine; depends on the mechanical stop position |
| `notchDebounceMs = 10` | Ignores contact bounce; notch window is 424 ms at default speed |
| `buttonDebounceMs = 20` | Reliable short/long classification |
| `longPressMs = 2000` | Chosen by the operator; long enough to avoid accidental aborts |
| `notchTimeoutMs = 15000` | 15 s covers ~177 mm at 400 µsteps/s; holder gap is 47–57 mm |
| `maxSeekSteps = 0` | The timeout is the intended stop criterion; a step budget could false-trigger |
| `settleMs = 200` | Lets the mechanics stop vibrating before the photo |
| `focusMs = 300` | Typical camera autofocus/half-press time |
| `shutterMs = 150` | Typical remote-shutter pulse |
| `postShotMs = 300` | Lets the camera finish before moving |
| `logLevel = 2` | Informational logs without flooding the serial port |
| `statusPeriodMs = 0` | No periodic traffic unless requested |

## 3. Validation

- Numeric keys must be within their range; otherwise
  `ERR VALUE_OUT_OF_RANGE <key> must be <min>..<max>`.
- `microsteps` must be one of `1,2,4,8,16,32,64` (power of two).
- Booleans accept `0/1`, `true/false`, `on/off` (case-insensitive).
- A non-numeric value → `ERR VALUE_OUT_OF_RANGE <key> expects an integer`.
- `SET` is only allowed in `IDLE`; `CommandHandler` enforces this and returns
  `ERR BUSY` otherwise. `ConfigManager::set()` has no state dependency.
- Unknown keys → `ERR UNKNOWN_KEY`.
- Read-only keys → `ERR READ_ONLY`.
- Cross-field checks:
  - `longPressMs` must be greater than `buttonDebounceMs`.
  - `maxSeekSteps` must be `0` or at least `100`.
- Invalid values never change RAM.

On boot, a config that fails validation is replaced by defaults and logged with
`LOG WARN config invalid, defaults loaded`.

## 4. EEPROM layout

The Uno has 1,024 bytes of EEPROM. The configuration is stored once at address 0
as a packed struct:

```cpp
struct PersistedConfig {
  uint16_t magic;               // 0x4653 ("FS")
  uint8_t  version;             // 1
  uint8_t  reserved;            // 0
  uint16_t length;              // sizeof(PersistedConfig) = 47
  NotchConfig  notch;           // 2 bytes
  ButtonConfig button;          // 4 bytes
  CameraConfig camera;          // 8 bytes
  MotionConfig motion;          // 14 bytes
  RunConfig    run;             // 4 bytes
  DriverConfig driver;          // 6 bytes
  LogConfig    log;             // 1 byte
  uint16_t crc;                 // 2 bytes; CRC16-CCITT over the preceding bytes
} __attribute__((packed));
```

The nested slices are the same ones in
[03-architecture.md §5](03-architecture.md#5-types), defined next to their
consumer. `DriverConfig` uses `uint8_t` for `stealthChop`, not `bool`, to keep
the packed layout explicit. Load and save copy field by field between
`PersistedConfig` and the aggregate `Config`; the slices are never serialized
separately.

- `magic`: `0x4653` ("FS").
- `version`: `1`. Bump only with a migration path; otherwise defaults are used.
- `length`: `sizeof(PersistedConfig)`.
- `crc`: CRC16-CCITT, polynomial `0x1021`, initial value `0xFFFF`, no final XOR,
  computed over bytes `[0, offsetof(PersistedConfig, crc))`.
- `reserved`: write 0.

### 4.1 Load algorithm

1. `EEPROM.get(0, persisted)`.
2. If `magic`, `version`, `length` or `crc` is wrong → return `false`.
3. Copy the slices into the aggregate `Config`.
4. Validate. If invalid → return `false`.
5. Return `true`.

`ConfigManager::begin()` on `false`: load defaults into the aggregate, log
`LOG WARN eeprom invalid, defaults loaded`, then push every slice into its
consumer. On success it also pushes every slice once, so the components start
from the persisted values rather than from their construction defaults.

### 4.2 Save algorithm

1. Copy `Config` into `PersistedConfig`.
2. Fill magic/version/length/reserved.
3. Compute CRC.
4. `EEPROM.put(0, persisted)`.
5. Read back and verify the CRC.
6. On mismatch: log `LOG ERROR eeprom write failed` and return `false`.

`SAVE` is explicit. The firmware never auto-saves, so EEPROM wear is bounded by
operator actions.

### 4.3 Reset

`RESET_CONFIG` loads the factory defaults into the aggregate, pushes every slice
to its consumer, writes the aggregate to EEPROM, and returns success. The
command handler emits `EVT CONFIG_RESET`. It is allowed only in `IDLE`.

## 5. Live updates and ownership

`ConfigManager` owns the aggregate `Config` — the single source of truth. Each
consumer defines its slice, owns a copy of the applied values, and exposes
`applyConfig`:

| Slice | Consumer | Pushed on | Reaction |
|---|---|---|---|
| `NotchConfig` | `NotchSensor` | `notchDebounceMs` | restart the debounce window |
| `ButtonConfig` | `Button` | `buttonDebounceMs`, `longPressMs` | next `update()` |
| `CameraConfig` | `Camera` | `settleMs`, `focusMs`, `shutterMs`, `postShotMs` | next sequence |
| `MotionConfig` | `IMotionController` (`StepperMotionController`) | `seekSpeed`, `acceleration`, `jogSpeed`, `offsetSteps`, `maxSeekSteps` | next move |
| `RunConfig` | `ScannerController` | `notchTimeoutMs`, `statusPeriodMs` | next seek / status check |
| `DriverConfig` | `ITmcDriver` | `microsteps`, `runCurrentMa`, `holdCurrentPercent`, `stealthChop` | rewrite the registers immediately |
| `LogConfig` | `Logger` | `logLevel` | change the level immediately |

`ConfigManager::set()` validates against the aggregate, mutates it, and calls
`applyConfig` on the owning consumer. It returns `ErrorCode`; there is no change
descriptor and no second application step. A rejected set changes nothing and
pushes nothing.

Rules:

- The aggregate is authoritative. Values are never read back from components.
- `applyConfig` accepts the validated value as-is; a component must not clamp or
  normalise, or the copy and the truth diverge.
- `begin()` and `reset()` push every slice.
- `SET` is only allowed in `IDLE`, so no activity observes a half-applied change.

Changing `microsteps`, `seekSpeed` or `acceleration` changes the physical
stopping position. The manager logs:

```
LOG WARN offsetSteps must be recalibrated after this change
```

and continues. Recalibrate with [08-calibration.md](08-calibration.md).

## 6. Examples

```
> SET offsetSteps 68
< OK SET offsetSteps=68
> SET stealthChop 1
< OK SET stealthChop=1
> SET notchTimeoutMs 15000
< OK SET notchTimeoutMs=15000
> SAVE
< OK SAVE
< EVT CONFIG_SAVED
```

```
> GET expectedFrameSteps
< CFG expectedFrameSteps=1290
< OK GET
```

```
> SET pin.notch 5
< ERR READ_ONLY pin.notch
```

```
> SET microsteps 3
< ERR VALUE_OUT_OF_RANGE microsteps must be one of 1,2,4,8,16,32,64
```

## 7. Factory defaults (reset table)

```
microsteps=16
runCurrentMa=900
holdCurrentPercent=50
stealthChop=1
seekSpeed=400
acceleration=2000
jogSpeed=200
offsetSteps=0
notchDebounceMs=10
buttonDebounceMs=20
longPressMs=2000
notchTimeoutMs=15000
maxSeekSteps=0
settleMs=200
focusMs=300
shutterMs=150
postShotMs=300
logLevel=2
statusPeriodMs=0
```

## 8. Notes

- The pin map is intentionally not persisted. Changing pins requires a rebuild.
- There is no `controlMode` key. The control input is always a momentary button
  (or a cable used as one). There is no level/switch mode and no auto-start at
  boot.
- `offsetSteps` is in microsteps and is applied after the controlled stop. If
  you change `microsteps`, recalculate: the same physical offset is
  `offsetSteps × (newMicrosteps / oldMicrosteps)`.
- `runCurrentMa` is RMS, not peak. See
  [07-stepper-tmc2209-guide.md](07-stepper-tmc2209-guide.md).
- `holdCurrentPercent` is a percentage of `runCurrentMa`.
- `notchTimeoutMs` is measured from entering `SEEK`. It resets for every frame.
  It does not apply to `JOG`.
