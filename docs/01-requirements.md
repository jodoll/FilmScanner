# 01 — Requirements

## 1. Purpose

Define what the film-scanner firmware must do, independent of how it is
implemented. Every requirement has an ID so tests and commits can refer to it.

## 2. System context

The machine scans a strip of film negatives held in a holder. A stepper motor
drives a toothed rack through a sprocket. A microswitch detects notches in the
rack; each notch marks a frame position. The firmware moves the holder from
notch to notch, triggers a camera once per notch, and reports progress over USB
serial. A magazine can feed successive holders, so the run continues until no
notch is seen for a configurable timeout.

```
Host (optional) ──USB──► Arduino Uno ──UART──► TMC2209 ──► NEMA 17
                              │                              │
                              ├── notch microswitch ◄────────┤ holder/rack
                              ├── camera focus + shutter
                              ├── status LED
                              └── run button / cable
```

## 3. Definitions

| Term | Meaning |
|---|---|
| Frame | One negative image |
| Holder | Carrier for a strip of four negatives |
| Magazine | Feeder that supplies successive holders |
| Notch | Recess in the rack detected by the microswitch |
| Notch closed | Switch actuated (pin LOW) |
| Seek | Move forward until the next notch closes |
| Align | Controlled stop plus signed step offset after a notch |
| Run | Automatic continuous scanning until stopped or timed out |
| Single | One frame triggered by the `STEP` command |
| Jog | Manual forward/backward move by a step count |
| Control input | Momentary button or cable on D6 |
| Graceful stop | Finish the current frame and photo, then stop |
| Abort | Stop motion immediately; cancel a pending photo |

## 4. Functional requirements

### 4.1 Run control

| ID | Requirement |
|---|---|
| FR-01 | `START` (serial) or a control-input event in `IDLE` or `FAULT` MUST start a run. `START` in `FAULT` clears the fault. |
| FR-02 | A run MUST scan continuously: each notch closure produces exactly one photo. The firmware MUST NOT count four frames or stop at the end of a holder. |
| FR-03 | At the start of a run the holder may be anywhere. If the notch is already closed, the firmware MUST move forward until it opens, then continue to the next closure before taking the first photo. If it is open, the first photo is at the next closure. |
| FR-04 | A run MUST end when: `STOP` is received and the current frame completes; `ABORT` is received; the notch timeout expires; or an error occurs. |
| FR-05 | The firmware MUST NOT auto-start a run at power-on when the control input is held. |
| FR-06 | The control input is a momentary active-low contact. A short press acts on release; a long press (2 s) fires while held. |
| FR-07 | Short press in `IDLE`/`FAULT` starts a run. Short press during a run requests a graceful stop. Short press during `JOG`, `SINGLE` or `SHOOT` stops immediately. Short press while a graceful stop is pending is ignored. |
| FR-08 | Long press in `IDLE`/`FAULT` starts a run. Long press during a run, `JOG`, `SINGLE` or `SHOOT` aborts immediately. Release after a long press does nothing. |
| FR-09 | `STOP` during a run MUST be graceful: the current frame is completed (seek, align, settle, focus, shutter, post) and then the run ends. `STOP` during `JOG`, `SINGLE` or `SHOOT` MUST stop immediately; during `SHOOT` the camera sequence is aborted (an active shutter pulse completes). |
| FR-10 | `ABORT` MUST stop motion immediately in every state, cancel a pending photo, and return to `IDLE`. If a shutter pulse is already active, the pulse MUST be completed before releasing focus. |

### 4.2 Motion and positioning

| ID | Requirement |
|---|---|
| FR-11 | Scanning motion is forward only. `JOG` may be signed for setup and unjamming. |
| FR-12 | The notch microswitch is the position reference. Each frame MUST be re-referenced by the sensor; step loss during travel must not accumulate. |
| FR-13 | On a notch closure the firmware MUST command a controlled deceleration to a stop, then move the signed `offsetSteps`, then settle before the photo. |
| FR-14 | The notch timeout MUST be 15,000 ms by default and MUST apply while seeking in `RUN` and `SINGLE`. It MUST NOT apply to `JOG`. |
| FR-15 | On timeout in `RUN`, the firmware MUST stop motion, disable the motor, emit `EVT RUN_STOPPED reason=notch_timeout`, and return to `IDLE`. |
| FR-16 | On timeout in `SINGLE`, the firmware MUST stop motion, disable the motor, emit `EVT STEP_FAILED code=NOTCH_TIMEOUT`, and return to `IDLE` (no fault). |
| FR-17 | `JOG <steps>` MUST move by the given signed number of microsteps at the configured jog speed and MUST be stoppable by `STOP`, `ABORT`, or the control input. |
| FR-18 | `STEP` MUST execute exactly one seek/align/settle/focus/shutter/post sequence and then return to `IDLE`. |
| FR-19 | The motor driver MUST be enabled for any motion and MUST be disabled in `IDLE` and `FAULT`. The sole exception is the explicit `ENABLE` maintenance override, which keeps the driver enabled in `IDLE` until `DISABLE`, any motion command, `ABORT`, or entry into `FAULT`. |
| FR-20 | The firmware MUST track and report the current position in microsteps, the total number of frames completed since boot, the number completed in the current/last run, and the last error. |

### 4.3 Camera

| ID | Requirement |
|---|---|
| FR-21 | The camera MUST be triggered through two independent active-high outputs: focus (D10) and shutter (D2). |
| FR-22 | The frame sequence MUST be: settle delay → focus high → focus delay → shutter high → shutter pulse → shutter low → focus low → post-shot delay. |
| FR-23 | Default timings MUST be settle 200 ms, focus 300 ms, shutter 150 ms, post-shot 300 ms; all configurable. |
| FR-24 | `SHOOT` MUST run the camera sequence without moving the holder. |
| FR-25 | An abort during a shutter pulse MUST NOT truncate the pulse. If the shutter has not fired yet, it MUST NOT fire. |

### 4.4 Configuration and persistence

| ID | Requirement |
|---|---|
| FR-26 | All settings listed in `06-configuration.md` MUST be readable and writable over serial, validated against their ranges, and persisted in EEPROM on `SAVE`. |
| FR-27 | `SET` MUST change RAM only. `SAVE` MUST persist. `RESET_CONFIG` MUST restore defaults and persist them. |
| FR-28 | On boot, invalid EEPROM magic, version, length or CRC MUST cause defaults to be loaded with a warning; the firmware MUST NOT fail to boot. |
| FR-29 | `CONFIG` MUST print every key, including read-only fields such as firmware version, protocol version, pin map and derived values. |
| FR-30 | `SET`, `SAVE` and `RESET_CONFIG` MUST be rejected outside `IDLE` with `ERR BUSY`. `GET`, `CONFIG`, `STATUS` MUST work in every state. |

### 4.5 Reporting and safety

| ID | Requirement |
|---|---|
| FR-31 | The firmware MUST implement the ASCII protocol in `05-serial-protocol.md`, including `OK`/`ERR` responses, `EVT` events and `LOG` lines. |
| FR-32 | A 2 s watchdog MUST be enabled and kicked once per main-loop iteration. |
| FR-33 | The status LED MUST show: 1 Hz blink in `IDLE`, solid while active, 5 Hz blink in `FAULT`. |
| FR-34 | All hardware access MUST go through the interfaces in `03-architecture.md`; core logic MUST be testable without hardware. |
| FR-35 | The firmware MUST not allocate memory after `setup()`, use exceptions, RTTI, STL containers, or Arduino `String`. |
| FR-36 | The main loop MUST remain non-blocking; no `delay()` is allowed in core logic. |

## 5. Non-functional requirements

| ID | Requirement |
|---|---|
| NFR-01 | A 36-frame film SHOULD complete in under 10 minutes; the target is 3–5 s per frame (about 2.6 min for 36 frames at defaults). |
| NFR-02 | Motion defaults MUST be conservative: 400 µsteps/s and 2,000 µsteps/s², well below the AccelStepper limit and the motor's torque limit. |
| NFR-03 | The firmware MUST tolerate a 47–57 mm gap between holders without a false timeout (15 s covers ~177 mm at 400 µsteps/s). |
| NFR-04 | Command response latency SHOULD be under 100 ms while idle and MUST not block step generation. |
| NFR-05 | The firmware MUST fit the Uno's 32 KB flash and 2 KB RAM with margin. |
| NFR-06 | All logic classes MUST have native unit tests; the hardware acceptance checklist MUST pass before release. |
| NFR-07 | Timing MUST be deterministic: all delays are deadline-based on `millis()`, not loop-count based. |
| NFR-08 | The design MUST keep the motor disabled at rest so the driver and motor stay cool. |
| NFR-09 | Configuration MUST be changeable without recompiling, except for the physical pin map. |
| NFR-10 | A TMC UART readback failure MUST degrade to write-only operation with a warning, not a fatal error. |

## 6. Constraints

- Arduino Uno, 16 MHz, 5 V logic.
- TMC2209 UART may be TX-only (one wire) or two-wire; firmware MUST work with
  either.
- 12 V motor supply, 900 mA RMS default.
- Camera interface is two opto-isolated active-high contacts.
- No home or end-of-travel switch; the notch switch is the only sensor.
- The holder does not back-drive when the motor is disabled.
- USB serial is the only host interface; no display, no SD card, no WiFi.

## 7. Out of scope

- Host-side image capture, file naming, or image processing.
- Camera configuration (exposure, focus mode) — the camera is set up manually.
- Automatic film loading/unloading.
- Reverse scanning.
- Multiple motors or axes.
- Network connectivity.
- OTA firmware update.
- A physical enclosure/UI beyond the status LED and one control input.

## 8. Assumptions

- The notch switch is normally open, wired to GND, with the internal pull-up.
- The control input is normally open, wired to GND, with the internal pull-up.
- The camera accepts a dry-contact closure for focus and shutter.
- The rack moves the holder forward when the motor turns in the configured
  positive direction; direction can be inverted by swapping one motor coil pair
  or by the `DIR` adapter.
- Notches are 5 mm wide and 38 mm apart (start to start).
- The holder gap is 47–57 mm.
- The holder stays in place when the motor is de-energized.

## 9. Acceptance criteria

The firmware is accepted when all of the following pass on the real machine.

| ID | Test | Pass condition |
|---|---|---|
| AC-01 | Compile | `pio run -e uno` succeeds with no project warnings |
| AC-02 | Unit tests | `pio test -e native` passes |
| AC-03 | Boot | `EVT READY` appears at 115200 8N1; LED blinks at 1 Hz |
| AC-04 | Notch detection | `STATUS` and `EVT NOTCH` report each closure; debounce ignores bounce |
| AC-05 | Single frame | `STEP` takes exactly one photo at the next notch and returns to `IDLE` |
| AC-06 | Continuous run | `START` takes one photo per notch for a 4-frame holder and continues if another holder follows |
| AC-07 | Timeout | With no notch, a run stops after 15 s with `reason=notch_timeout` |
| AC-08 | JOG | `JOG 100` moves forward; `JOG -100` moves back; no timeout; `STOP` halts it |
| AC-09 | Graceful stop | `STOP` during a run completes the current frame and then stops |
| AC-10 | Abort | `ABORT` during a run stops within ~1.2 mm and takes no further photo |
| AC-11 | Button | Short press starts/stops; 2 s long press aborts; release after long press does nothing |
| AC-12 | Boot with contact held | A held cable at power-on causes no motion until released |
| AC-13 | Config | `SET`/`SAVE` survive a power cycle; `RESET_CONFIG` restores defaults |
| AC-14 | Config validation | Out-of-range values return `ERR VALUE_OUT_OF_RANGE` and do not change RAM |
| AC-15 | LED | IDLE blinks, active solid, FAULT fast blink |
| AC-16 | Watchdog | A deliberately blocked loop resets the board within ~2 s and the motor is disabled |
| AC-17 | Thermal | Motor and driver stay below 70 °C after 10 minutes of continuous scanning |
| AC-18 | Offset repeatability | Three consecutive frames land within ±1 microstep of the calibrated offset |
| AC-19 | 36-frame run | A 36-frame film completes in under 10 minutes with no lost frames |
| AC-20 | Error recovery | After a fault, `START` and a button press both restart; `STOP` returns to `IDLE` |
