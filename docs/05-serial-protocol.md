# 05 — Serial protocol

## 1. Physical layer

| Setting | Value |
|---|---|
| Port | Arduino Uno USB (D0/D1, `Serial`) |
| Baud | 115200 |
| Data | 8 data bits, no parity, 1 stop bit (8N1) |
| Flow control | None |
| Encoding | ASCII |
| Line ending | `\n` (a preceding `\r` is accepted and stripped) |
| Max request length | 63 characters + NUL |
| Command case | Case-insensitive |

The host must read continuously; the firmware emits asynchronous `EVT` and `LOG`
lines between responses.

## 2. Framing

Requests are one line each:

```
<COMMAND> [<argument>] [<argument>]\n
```

Responses and asynchronous lines:

```
OK <COMMAND> [<details>]
ERR <CODE> <human readable message>
CFG <key>=<value>
STATUS <key>=<value> ...
EVT <NAME> [key=value ...]
LOG <LEVEL> <message>
```

Rules:

- The response to a command always comes before any events it triggers.
- Empty lines are ignored (no response).
- Leading/trailing whitespace is ignored; runs of spaces are collapsed.
- A line longer than 63 characters is discarded until the next `\n` and answered
  with `ERR BAD_COMMAND line too long`.
- Unknown commands → `ERR BAD_COMMAND unknown command`.
- Extra arguments to a command that takes none → `ERR BAD_COMMAND`.
- Commands that start long operations return `OK` immediately and report
  completion with `EVT` lines.

## 3. Command summary

| Command | Arguments | Allowed states | Purpose |
|---|---|---|---|
| `START` | — | `IDLE`, `FAULT` | Start a continuous run |
| `STOP` | — | any | Graceful stop during a run; immediate during manual moves |
| `ABORT` | — | any | Immediate stop in every state |
| `STATUS` | — | any | Print current status |
| `CONFIG` | — | any | Print the complete configuration |
| `GET` | `<key>` | any | Print one configuration value |
| `SET` | `<key> <value>` | `IDLE` | Change a value in RAM |
| `SAVE` | — | `IDLE` | Persist the current configuration to EEPROM |
| `RESET_CONFIG` | — | `IDLE` | Restore defaults and persist them |
| `JOG` | `<signed steps>` | `IDLE` | Manual move; no notch timeout |
| `STEP` | — | `IDLE` | Scan exactly one frame |
| `SHOOT` | — | `IDLE` | Trigger the camera without moving |
| `ENABLE` | — | `IDLE` | Enable the motor driver |
| `DISABLE` | — | `IDLE` | Disable the motor driver |
| `VERSION` | — | any | Print firmware and protocol version |
| `HELP` | — | any | List commands |
| `PING` | — | any | Liveness check |

## 4. Commands

### 4.1 `START`

Starts a continuous run. In `FAULT` it clears the error first.

```
> START
< OK START
< EVT RUN_STARTED
< EVT NOTCH pos=1290
< EVT FRAME_STARTED index=1
< EVT FRAME_DONE index=1 pos=1358
...
```

Errors:

| Condition | Response |
|---|---|
| Not in `IDLE`/`FAULT` | `ERR BUSY run already active` |
| Driver still reports a fault after clearing | `ERR DRIVER_FAULT <detail>`; state stays `FAULT` |

### 4.2 `STOP`

- `IDLE`: `OK STOP`, no event.
- `FAULT`: clears the error, `OK STOP`, state `IDLE`.
- `RUN`: `OK STOP`; the current frame finishes, then
  `EVT RUN_STOPPED reason=request frames=N`.
- `SINGLE`: motion halts immediately, camera aborts (completing an active
  shutter pulse), `OK STOP`, then `EVT STEP_STOPPED reason=request`.
- `JOG`: motion halts immediately, `OK STOP`, then
  `EVT JOG_STOPPED reason=request`.
- `SHOOT`: the camera sequence aborts (an active shutter pulse completes),
  `OK STOP`, then `EVT SHOT_STOPPED reason=request`.

### 4.3 `ABORT`

Immediate stop in every state. Motion is halted, a pending photo is cancelled,
an active shutter pulse is completed, the motor is disabled, and the state
returns to `IDLE`.

```
> ABORT
< OK ABORT
< EVT RUN_STOPPED reason=abort frames=2
```

In `IDLE` it ensures the motor is disabled and clears any `ENABLE` override. In
`FAULT` it clears the error and disables the motor.

### 4.4 `STATUS`

```
> STATUS
< STATUS state=IDLE mode=NONE stopRequested=0 motor=disabled camera=IDLE pos=0 frames=0 runFrames=0 lastError=NONE uptimeMs=12345
< OK STATUS
```

| Field | Values |
|---|---|
| `state` | `BOOT`, `IDLE`, `SEEK`, `STOPPING`, `OFFSET`, `CAMERA`, `JOG`, `FAULT` |
| `mode` | `NONE`, `RUN`, `SINGLE`, `SHOOT` |
| `stopRequested` | `0` or `1` |
| `motor` | `enabled` or `disabled` |
| `camera` | `IDLE`, `SETTLE`, `FOCUS`, `SHUTTER`, `POST`, `DONE` |
| `pos` | signed 32-bit microstep position |
| `frames` | total frames completed since boot (monotonic) |
| `runFrames` | frames completed in the current run, or in the most recently finished run |
| `lastError` | `ErrorCode` name |
| `uptimeMs` | milliseconds since boot |

`STATUS` never changes state and is safe to call at any time. When emitted
periodically (see `statusPeriodMs`) it has **no** trailing `OK STATUS`; the
`OK STATUS` line appears only in response to an explicit `STATUS` command.

### 4.5 `CONFIG`

Prints every key, including read-only values, then `OK CONFIG`.

```
> CONFIG
< CFG fw=0.1.0
< CFG proto=1
< CFG pin.notch=3
< CFG pin.control=6
< CFG pin.shutter=2
< CFG pin.focus=10
< CFG pin.step=7
< CFG pin.dir=8
< CFG pin.enable=9
< CFG pin.led=13
< CFG stepsPerMm=33.953
< CFG expectedFrameSteps=1290
< CFG microsteps=16
< CFG runCurrentMa=900
...
< CFG statusPeriodMs=0
< OK CONFIG
```

- Read-only keys (`fw`, `proto`, `pin.*`, `stepsPerMm`, `expectedFrameSteps`) are
  printed but rejected by `SET` with `ERR READ_ONLY`.
- Booleans are printed as `0`/`1`.
- Floats are printed with three decimals.

### 4.6 `GET <key>`

```
> GET offsetSteps
< CFG offsetSteps=0
< OK GET
```

Unknown key → `ERR UNKNOWN_KEY <key>`. Read-only keys are allowed.

### 4.7 `SET <key> <value>`

Changes RAM only. Use `SAVE` to persist. Allowed only in `IDLE`.

```
> SET offsetSteps 68
< OK SET offsetSteps=68
```

```
> SET seekSpeed 99999
< ERR VALUE_OUT_OF_RANGE seekSpeed must be 50..2000
```

```
> SET offsetSteps 10
< ERR BUSY configuration can only change in IDLE
```

Booleans accept `0`/`1`, `true`/`false`, `on`/`off` (case-insensitive).

Keys and ranges are defined in [06-configuration.md](06-configuration.md).
A `SET` that changes the driver configuration re-applies it immediately.
Changing `microsteps`, `seekSpeed` or `acceleration` invalidates the calibrated
`offsetSteps`; the firmware logs a warning but does not block the change.

### 4.8 `SAVE`

Persists the current RAM configuration to EEPROM.

```
> SAVE
< OK SAVE
< EVT CONFIG_SAVED
```

Only allowed in `IDLE`; otherwise `ERR BUSY`.

### 4.9 `RESET_CONFIG`

Restores factory defaults **and** saves them.

```
> RESET_CONFIG
< OK RESET_CONFIG
< EVT CONFIG_RESET
```

Only allowed in `IDLE`.

### 4.10 `JOG <signed steps>`

Manual move at `jogSpeed`. Positive = forward, negative = backward. No notch
timeout. Stoppable with `STOP`, `ABORT`, or the control input.

```
> JOG 100
< OK JOG
< EVT JOG_DONE steps=100 pos=100
```

```
> JOG -100
< OK JOG
< EVT JOG_DONE steps=-100 pos=0
```

Errors: `ERR VALUE_OUT_OF_RANGE` for `0`, `ERR BUSY` outside `IDLE`.

### 4.11 `STEP`

Scans exactly one frame: seek, align, settle, focus, shutter, post. Applies the
notch timeout; a timeout is reported as an event, not a fault.

```
> STEP
< OK STEP
< EVT NOTCH pos=1290
< EVT FRAME_STARTED index=1
< EVT FRAME_DONE index=1 pos=1358
< EVT STEP_DONE pos=1358
```

Timeout:

```
< EVT STEP_FAILED code=NOTCH_TIMEOUT
```

Only allowed in `IDLE`; otherwise `ERR BUSY`.

### 4.12 `SHOOT`

Runs the camera sequence once without moving.

```
> SHOOT
< OK SHOOT
< EVT SHOT_DONE
```

Only allowed in `IDLE`; otherwise `ERR BUSY`.

### 4.13 `ENABLE` / `DISABLE`

Manual motor-enable control for setup. Allowed only in `IDLE`.

```
> ENABLE
< OK ENABLE
< EVT MOTOR_STATE enabled=1
```

```
> DISABLE
< OK DISABLE
< EVT MOTOR_STATE enabled=0
```

`ENABLE` is a **maintenance override**: it energizes the driver while in `IDLE`,
where the normal rule is that the motor is disabled. The override ends on
`DISABLE`, any motion command (which enables the driver and then disables it
again on return to `IDLE`), `ABORT`, or entry into `FAULT`. Any motion command
enables the driver automatically regardless of the override.

### 4.14 `VERSION`

```
> VERSION
< OK VERSION fw=0.1.0 proto=1
```

`fw` comes from the build flag `VERSION`; `proto` is the protocol version (1).

### 4.15 `HELP`

```
> HELP
< HELP START - start a continuous run
< HELP STOP - graceful stop (finish current frame)
< HELP ABORT - immediate stop
...
< OK HELP
```

Keep the help text compact so it does not stall the loop.

### 4.16 `PING`

```
> PING
< OK PING
```

## 5. Events

Events are asynchronous and may appear at any time after the command response.

| Event | Fields | When |
|---|---|---|
| `EVT READY` | `fw=<version> proto=1` | After boot |
| `EVT RUN_STARTED` | — | Run starts |
| `EVT RUN_STOPPED` | `reason=<request\|abort\|notch_timeout\|motion_timeout\|error> frames=<runFrames>` | Run ends |
| `EVT NOTCH` | `pos=<int32>` | Notch closure detected |
| `EVT FRAME_STARTED` | `index=<n>` | Camera sequence starts |
| `EVT FRAME_DONE` | `index=<n> pos=<int32>` | Camera sequence ends |
| `EVT JOG_DONE` | `steps=<int32> pos=<int32>` | Jog completes |
| `EVT JOG_STOPPED` | `reason=<request\|abort\|error>` | Jog stopped early |
| `EVT STEP_DONE` | `pos=<int32>` | Single frame completes |
| `EVT STEP_FAILED` | `code=<NOTCH_TIMEOUT\|MOTION_TIMEOUT>` | Single frame failed |
| `EVT STEP_STOPPED` | `reason=<request\|abort\|error>` | Single frame stopped |
| `EVT SHOT_DONE` | — | `SHOOT` completes |
| `EVT SHOT_STOPPED` | `reason=<request\|abort\|error>` | `SHOOT` cancelled |
| `EVT FAULT` | `code=<DRIVER_FAULT\|INTERNAL>` | Fault entered |
| `EVT CONFIG_SAVED` | — | EEPROM write completed |
| `EVT CONFIG_RESET` | — | Defaults restored and saved |
| `EVT MOTOR_STATE` | `enabled=<0\|1>` | Driver enable changed |

Frame indices start at 1 and reset at the start of each run. `STEP`/`SHOOT` use
index 1. `frames=<runFrames>` in `RUN_STOPPED` is the number of frames completed
in that run (`runFrames` in `STATUS`), not the lifetime total.

## 6. Log lines

```
LOG ERROR <message>
LOG WARN  <message>
LOG INFO  <message>
LOG DEBUG <message>
```

Levels: `0 = ERROR`, `1 = WARN`, `2 = INFO`, `3 = DEBUG`. The default is `INFO`.
`LOG` is for humans; do not parse it for control.

Boot examples:

```
< LOG INFO config loaded
< LOG INFO tmc uart readback available
< EVT READY fw=0.1.0 proto=1
```

```
< LOG WARN eeprom invalid, defaults loaded
< LOG WARN tmc uart readback unavailable, write-only
< EVT READY fw=0.1.0 proto=1
```

## 7. Error codes

| Code | Meaning |
|---|---|
| `NONE` | Success |
| `BAD_COMMAND` | Unknown command, bad syntax, too many args, line too long |
| `UNKNOWN_KEY` | Configuration key does not exist |
| `READ_ONLY` | Configuration key cannot be set |
| `VALUE_OUT_OF_RANGE` | Value outside the allowed range |
| `BUSY` | Command not allowed in the current state |
| `NOTCH_TIMEOUT` | No notch before the deadline (event only) |
| `MOTION_TIMEOUT` | `maxSeekSteps` reached without a notch |
| `DRIVER_FAULT` | TMC2209 reported a fault (readback only) |
| `DRIVER_UART` | UART readback failed (warning, write-only continues) |
| `EEPROM_INVALID` | EEPROM empty/corrupt; defaults loaded |
| `CONFIG_INVALID` | Loaded config failed validation; defaults loaded |
| `INTERNAL` | Unexpected internal state |

`NOTCH_TIMEOUT` is a normal end-of-run condition and does not enter `FAULT`.
`DRIVER_FAULT` and `INTERNAL` do.

## 8. Examples

### 8.1 Boot and status

```
> PING
< OK PING
> VERSION
< OK VERSION fw=0.1.0 proto=1
> STATUS
< STATUS state=IDLE mode=NONE stopRequested=0 motor=disabled camera=IDLE pos=0 frames=0 runFrames=0 lastError=NONE uptimeMs=8421
< OK STATUS
```

### 8.2 Configure and save the offset

```
> SET offsetSteps 68
< OK SET offsetSteps=68
> SET seekSpeed 400
< OK SET seekSpeed=400
> SAVE
< OK SAVE
< EVT CONFIG_SAVED
```

### 8.3 Run with a graceful stop

```
> START
< OK START
< EVT RUN_STARTED
< EVT NOTCH pos=1290
< EVT FRAME_STARTED index=1
< EVT FRAME_DONE index=1 pos=1358
< EVT NOTCH pos=2580
< EVT FRAME_STARTED index=2
< EVT FRAME_DONE index=2 pos=2648
> STOP
< OK STOP
< EVT NOTCH pos=3870
< EVT FRAME_STARTED index=3
< EVT FRAME_DONE index=3 pos=3938
< EVT RUN_STOPPED reason=request frames=3
```

### 8.4 Notch timeout

```
> START
< OK START
< EVT RUN_STARTED
< EVT NOTCH pos=1290
< EVT FRAME_STARTED index=1
< EVT FRAME_DONE index=1 pos=1358
... 15 s without a notch ...
< EVT RUN_STOPPED reason=notch_timeout frames=1
```

### 8.5 Fault and recovery

```
< EVT FAULT code=DRIVER_FAULT
> STATUS
< STATUS state=FAULT mode=NONE stopRequested=0 motor=disabled camera=IDLE pos=1358 frames=1 runFrames=1 lastError=DRIVER_FAULT uptimeMs=60000
< OK STATUS
> START
< OK START
< EVT RUN_STARTED
```

`STOP` also clears a fault and returns to `IDLE`.

### 8.6 Jog

```
> JOG 200
< OK JOG
< EVT JOG_DONE steps=200 pos=200
> JOG -200
< OK JOG
< EVT JOG_DONE steps=-200 pos=0
```

## 9. Protocol versioning

- Protocol version: **1**.
- The version is reported by `VERSION` and `EVT READY`.
- Commands and key names are stable. New keys may be added; unknown keys are
  rejected.
- Event field order is stable, but parsers should treat lines as key/value sets.
- A breaking change increments `proto` and is documented here.
