# 10 — Troubleshooting

## 1. First checks

1. Is the firmware flashed and the board powered?
2. Is the serial monitor at **115200 8N1**, line ending `\n`?
3. Does `PING` answer?
4. Does `STATUS` show `state=IDLE`, `motor=disabled`?
5. Does `CONFIG` show the expected values?
6. Is the 12 V supply on and the common ground connected?
7. Is the motor connector fully seated? **Never** unplug it while powered.

Useful commands:

```
PING
VERSION
STATUS
CONFIG
```

## 2. Serial / USB

| Symptom | Cause | Action |
|---|---|---|
| No response at all | Wrong baud or port | 115200 8N1; select the Uno's port |
| Garbage characters | Wrong baud | Set 115200 |
| Commands ignored | Missing `\n`; monitor sends `\r\n` only | Enable newline; `\r\n` is accepted |
| `ERR BAD_COMMAND` for a valid command | Extra argument or typo | Check `HELP`; commands are case-insensitive |
| Response arrives but events are missing | Host not reading continuously | Read until timeout; events are asynchronous |
| `STATUS` works but `CONFIG` is truncated | Host buffer too small | Increase the read buffer; CONFIG is multi-line |
| Board resets when the motor starts | 12 V droop or ground bounce | Check supply, common ground, decoupling |
| Board resets randomly | Watchdog from a blocked loop | See §8 |

## 3. Motor and driver

| Symptom | Cause | Action |
|---|---|---|
| Motor does not move, no noise | `EN` high (disabled); no 12 V; no STEP pulses | `ENABLE`; check `EN` wiring (active-low); check VM; verify AccelStepper is generating steps |
| Motor hums but does not turn | One coil open/miswired; current too low | Check A1/A2/B1/B2; raise `runCurrentMa` |
| Motor turns the wrong way | DIR polarity | Swap one coil pair or invert DIR |
| Motor vibrates and stalls | Speed/accel too high; current too low; binding | Lower `seekSpeed`/`acceleration`; raise current; check mechanics |
| Motor loses steps over time | Current too low; speed too high; mechanical drag | Raise current in small steps; lower speed; check rack/gear |
| Motor very hot | Current too high; hold current too high | Lower `runCurrentMa`/`holdCurrentPercent` |
| Driver very hot / shuts down | Poor cooling; high current | Add heatsink/fan; lower current; check VM |
| Motor does not hold between frames | Driver disabled in IDLE | Expected; it holds only while enabled during a run (unless `ENABLE` is active) |
| Motor moves in `JOG` but not `START` | Notch already closed? No notch? | `STATUS`; check the switch; `STEP` to test |
| UART readback fails | One-wire wiring; baud; resistor | `LOG` says write-only; lower baud; check 1 kΩ |
| `SET runCurrentMa` has no effect | UART not connected/configured | Check D5 → 1 kΩ → PDN_UART; `LOG` for readback; `CONFIG` |
| Driver faults reported | Overcurrent/overtemperature/short | Check wiring, current, cooling; `ABORT`; power-cycle |

### 3.1 UART-specific

| Symptom | Cause | Action |
|---|---|---|
| `LOG WARN tmc uart readback unavailable` | One-wire, or RX not connected | Normal if one-wire; otherwise check D4 |
| `LOG ERROR tmc uart` | Wrong wiring/baud/address | Check PDN_UART, 1 kΩ, MS1/MS2 = 0, baud |
| Current/microsteps do not change | Values written but not applied | `SET` then `SAVE`; check `LOG`; power-cycle |
| Two TMC drivers on one bus | Address collision | Set different MS1/MS2 addresses (not used here) |

### 3.2 Why the motor does not move at all

The TMCStepper library only writes registers. It does **not** generate STEP
pulses. Confirm that:

- `AccelStepperMotor` is constructed in `DRIVER` mode with the STEP/DIR pins.
- `motion.poll()` is called every loop.
- `EN` is LOW during motion.
- The motor has 12 V at VM.
- `VACTUAL` is 0 (STEP/DIR mode).

## 4. Notch sensor

| Symptom | Cause | Action |
|---|---|---|
| No `EVT NOTCH` | Switch not closing; wrong pin; no pull-up | Check D3, NO-to-GND wiring, `INPUT_PULLUP` |
| Stops too early/late | Switch mounting; lever not centered | Adjust the switch so it drops into the 5 mm notch |
| Multiple triggers per notch | Bounce | Increase `notchDebounceMs` (10 → 20) |
| Missed notch at speed | Debounce too long or speed too high | Lower `notchDebounceMs`; lower `seekSpeed` |
| Always reports closed | Wiring shorted to GND; NC switch | Check continuity; use a NO switch |
| Always reports open | Broken wire; wrong pin | Check continuity and pin map |
| `STEP` runs past the notch | Sensor not seen; firmware already latched | Check the switch; lower speed; verify debounce |

## 5. Camera

| Symptom | Cause | Action |
|---|---|---|
| No focus | Optocoupler polarity; `focusMs` too short; camera mode | Check LED/phototransistor; increase `focusMs`; set camera to remote/autofocus |
| No shutter | Optocoupler polarity; `shutterMs` too short | Check wiring; increase `shutterMs` |
| Blurry image | Motion during exposure; settle too short | Increase `settleMs`; check the holder is stable |
| Next frame starts before save | `postShotMs` too short | Increase `postShotMs` |
| Shutter fires twice | Long pulse or camera in continuous mode | Set camera to single; check `shutterMs` |
| Camera not isolated | Common ground/no opto | Use optocouplers; do not tie camera ground to 12 V |
| `SHOOT` works but `STEP` does not | Motion/notch issue, not camera | Check `STEP` events; see §4 |

## 6. Control input (button/cable)

| Symptom | Cause | Action |
|---|---|---|
| Button does nothing | Wiring; not to GND; not `INPUT_PULLUP` | Check D6 to GND; `INPUT_PULLUP` |
| Starts on release instead of press | Expected for short presses | Short press acts on release |
| Long press does not abort | Held < 2 s; debounce | Hold ≥ 2 s; check `longPressMs` |
| Starts immediately at boot | Contact held at boot is ignored, not started | If it starts, check for a stuck input or old firmware |
| A cable left connected does nothing | It is a held button | Disconnect and reconnect like a press |
| Double trigger | Bounce | Increase `buttonDebounceMs` |
| Short press during a run does not stop immediately | Graceful stop is intended | Use `ABORT` or a 2 s long press |

## 7. Configuration / EEPROM

| Symptom | Cause | Action |
|---|---|---|
| Values lost after power-cycle | `SET` without `SAVE` | `SAVE` after `SET` |
| `ERR VALUE_OUT_OF_RANGE` | Value outside range | Check `06-configuration.md` |
| `ERR READ_ONLY` | Tried to set `fw`, `pin.*`, derived values | Those are compile-time |
| `ERR BUSY` | `SET`/`SAVE` outside `IDLE` | `STOP`/`ABORT` first |
| `LOG WARN eeprom invalid` | First boot, corrupt EEPROM, version change | Defaults loaded; `SAVE` to store them |
| `LOG ERROR eeprom write failed` | EEPROM wear/failure | Retry; the Uno EEPROM is rated ~100k writes |
| Offset changed after microsteps/speed | Calibration invalid | Recalibrate |
| Config seems ignored | Driver not re-applied | Check `LOG`; power-cycle; check UART |

## 8. Watchdog / resets

| Symptom | Cause | Action |
|---|---|---|
| Board resets every ~2 s | Main loop blocked > 2 s | Find blocking code; serial reads must be non-blocking; no `delay()` |
| Resets when USB is unplugged | Power/ground issue | Power the Arduino properly; common ground |
| Boot loop after flashing | Watchdog not cleared / bootloader | Clear `MCUSR`; `wdt_disable()` early in `setup()` |
| Motor disabled after reset | Expected safety behavior | Re-enable/start |

## 9. State and command behavior

| Symptom | Cause | Action |
|---|---|---|
| `START` → `ERR BUSY` | Already running or in JOG/SINGLE | `STOP`/`ABORT`; check `STATUS` |
| `JOG` → `ERR BUSY` | Not in IDLE | Stop first |
| `STEP` times out | No notch within 15 s | Check sensor; `JOG` to a notch; `START` |
| Run stops after 15 s | No notch (end of film or jam) | Load the next holder; check the rack |
| Run stops at a holder gap | Timeout too short for the gap | Increase `notchTimeoutMs` (default 15 s covers 177 mm) |
| `STOP` does not stop immediately | Graceful stop is intended during a run | Use `ABORT` or a long press |
| `ABORT` takes a moment | Controlled deceleration / shutter pulse | Normal; ≤ ~1.2 mm + pulse |
| State stuck in FAULT | `START`/button needed | `START` or `STOP` clears it |

## 10. Mechanical

| Symptom | Cause | Action |
|---|---|---|
| Holder binds | Rack/gear misalignment; debris | Clean, align, check backlash |
| Gear slips | Loose set screw; worn gear | Tighten/replace |
| Holder drifts when disabled | Back-drive (contrary to assumption) | Keep the driver enabled or add a brake; update the docs |
| Notch spacing wrong | Wrong holder/rack | Verify 38 mm pitch |
| Frame alignment varies | Loose mechanics; current too low | Fix mechanics; raise current; recalibrate |
| Loud clicking | Current too high; microsteps too low | Lower current; use 1/16 |
| Motor stalls at the same place | Binding or cable drag | Find and fix the mechanical obstruction |

## 11. Diagnostic recipes

### 11.1 Is the driver configured?

```
> CONFIG
< CFG microsteps=16
< CFG runCurrentMa=900
...
> STATUS
< STATUS ... motor=disabled ...
```

Check `LOG WARN` for UART readback. If write-only, the values are still applied;
verify by feel/temperature.

### 11.2 Is the notch sensor working?

- Manually close the switch and send `STATUS`; it does not show the notch.
- Use `STEP` and watch for `EVT NOTCH`.
- If no event, check the wiring and pin with a multimeter.

### 11.3 Is the camera working?

```
> SHOOT
< OK SHOOT
< EVT SHOT_DONE
```

Watch the optocoupler LEDs and the camera. If nothing, measure the output pin.

### 11.4 Is the loop blocking?

- Remove periodic status (`SET statusPeriodMs 0`).
- Lower `logLevel` to 1.
- Check that no `delay()` or long `Serial.print` is in the loop.
- The watchdog resetting is the strongest signal.

## 12. Error code quick reference

| Code | Meaning | Typical action |
|---|---|---|
| `BAD_COMMAND` | Syntax/unknown command | `HELP` |
| `UNKNOWN_KEY` | Config key typo | `CONFIG` |
| `READ_ONLY` | Tried to set a read-only key | Use the writable key |
| `VALUE_OUT_OF_RANGE` | Value outside range | See `06-configuration.md` |
| `BUSY` | Command not allowed now | `STOP`/`ABORT` |
| `NOTCH_TIMEOUT` | No notch in 15 s | Load film; check sensor |
| `MOTION_TIMEOUT` | `maxSeekSteps` reached | Increase/disable; check sensor |
| `DRIVER_FAULT` | TMC fault (readback) | Check wiring/current/cooling |
| `DRIVER_UART` | Readback failed | Check UART; write-only is OK |
| `EEPROM_INVALID` | EEPROM empty/corrupt | Defaults loaded; `SAVE` |
| `CONFIG_INVALID` | Loaded config invalid | Defaults loaded; re-check |
| `INTERNAL` | Unexpected state | Reboot; report with logs |
