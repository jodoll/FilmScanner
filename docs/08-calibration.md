# 08 — Calibration and commissioning

Perform this procedure on the real machine before the first real scan. Record
the values at the end; they are machine-specific.

## 0. Prerequisites

- Firmware flashed and `PING` answers at 115200 8N1.
- 12 V supply connected, motor free to move, hands clear.
- Notch switch and control input wired (active-low, `INPUT_PULLUP`).
- Camera connected through the optocouplers.
- A test strip or a holder with a clearly visible frame gate.
- A ruler or calipers.
- A way to read the serial monitor (`pio device monitor -b 115200`).

## 1. Power-up checks

1. Power the Arduino over USB with the 12 V supply off.
2. Confirm the status LED blinks at 1 Hz (`IDLE`).
3. Send `PING` → `OK PING`.
4. Send `VERSION` → `OK VERSION fw=... proto=1`.
5. Send `CONFIG`; confirm the defaults, especially:
   - `microsteps=16`
   - `runCurrentMa=900`
   - `seekSpeed=400`
   - `acceleration=2000`
   - `offsetSteps=0`
6. Power the 12 V supply. The motor should remain disabled (`STATUS` →
   `motor=disabled`).
7. Send `ENABLE`; the motor should energize and hold.
8. Send `DISABLE`; the motor should release.

If `LOG WARN tmc uart readback unavailable` appears, the machine is in
write-only mode. That is fine; continue. See
[10-troubleshooting.md](10-troubleshooting.md) if configuration has no effect.

## 2. Direction check

1. Send `ENABLE`.
2. Send `JOG 100`.
3. The holder must move **forward** (toward the next frame / magazine).
4. Send `JOG -100`; it must move back to the starting position.
5. If forward is reversed:
   - Power down, swap one motor coil pair (A1/A2), or
   - invert the DIR adapter in firmware, then rebuild.

Do not change `JOG` sign semantics; positive is always forward.

## 3. Notch sensor check

1. Send `STATUS`; note the position.
2. Manually move the holder (motor disabled) so the switch drops into a notch.
3. `STATUS` does not show the notch; use `STEP` for a sensor-driven test.
4. Send `STEP`. The holder should:
   - Seek forward,
   - Stop at the next notch,
   - Apply the offset,
   - Run the camera sequence,
   - Return to `IDLE` with `EVT STEP_DONE`.
5. If it runs past the notch:
   - Check the switch wiring (NO to GND, D3).
   - Check that the lever drops fully into the 5 mm notch.
   - Reduce `notchDebounceMs` from 10 to 5 and retry.
6. If it stops immediately at the current notch (already closed), that is
   expected: the firmware first moves out of the closed notch, then to the next
   one.

## 4. Speed and step-loss check

Defaults: `seekSpeed=400`, `acceleration=2000`. A 38 mm frame takes about
3.4 s; the camera sequence adds about 0.95 s, so a frame is about 4.4 s.

1. Time 10 frames with `START` and `STOP`. Expect about 44 s plus the final
   frame.
2. If a frame takes much longer, the motor may be stalling or the loop is
   blocked. Check `STATUS` / logs.
3. Run 36 frames. If the offset drifts or frames are missed, the motor is losing
   steps:
   - Lower `seekSpeed` (e.g. 300).
   - Lower `acceleration` (e.g. 1500).
   - Raise `runCurrentMa` in small steps (900 → 1000).
   - Check for mechanical binding.
4. Do not raise speed for its own sake; the machine is designed for 3–5 s per
   frame and a 36-frame film in under 10 minutes.

## 5. Offset calibration

The offset is applied **after** the controlled stop. It compensates for the
stopping distance and the mechanical alignment of the film gate. It depends on
`microsteps`, `seekSpeed` and `acceleration`; recalibrate if any of those change.

### 5.1 Procedure

1. Set the offset to zero:
   ```
   > SET offsetSteps 0
   < OK SET offsetSteps=0
   ```
2. Insert a test strip and move to a known frame (or use the current one).
3. Send `STEP` to take one photo at the current offset.
4. Inspect the photo or the physical frame gate. Measure the error in
   millimetres, positive if the holder must move **further forward**.
5. Convert to microsteps:
   ```
   deltaSteps = errorMm × 33.953
   ```
   Example: 2.0 mm forward → 2.0 × 33.953 ≈ 68 steps.
6. Update:
   ```
   > SET offsetSteps 68
   < OK SET offsetSteps=68
   > SAVE
   < OK SAVE
   < EVT CONFIG_SAVED
   ```
7. Send `STEP` again and re-measure. Repeat until the error is within
   ±0.5 microstep (about ±0.015 mm).
8. Test three consecutive frames; the offset must be repeatable.

### 5.2 Notes

- The useful range is typically ±2 mm (about ±68 steps). `offsetSteps` accepts
  −5000..5000 for setup.
- If you change `microsteps`, scale the offset:
  `newOffset = oldOffset × newMicrosteps / oldMicrosteps`.
- If you change `seekSpeed` or `acceleration`, the stopping distance changes.
  Recalibrate from step 1.
- Do not use the offset to compensate for a slipping gear or a loose set screw.
  Fix the mechanics first.

## 6. Camera timing

1. Send `SHOOT` a few times:
   ```
   > SHOOT
   < OK SHOOT
   < EVT SHOT_DONE
   ```
2. If the camera does not focus:
   - Increase `focusMs` (try 500, 800).
   - Check the focus optocoupler polarity and wiring.
3. If the shutter does not fire:
   - Increase `shutterMs` (try 200, 300).
   - Check the shutter optocoupler and the camera's remote mode.
4. If the image is blurry from motion:
   - Increase `settleMs` (try 300, 500).
5. If the next frame starts before the camera has saved the image:
   - Increase `postShotMs` (try 500, 800).
6. Verify the timing with `STATUS` / timestamps; a frame should be about 4.4 s at
   defaults.

Camera-specific notes:

- Some cameras need the focus line held during the shutter pulse; the firmware
  does that.
- Some cameras only need a shutter pulse and ignore focus; leaving focus
  connected is harmless.
- If the camera has a "half-press" threshold, `focusMs` is the time the contact
  is closed, not the autofocus time of the camera. Tune it until the camera
  locks focus.

## 7. Control input check

1. Short press (< 2 s):
   - In `IDLE`: starts a run.
   - During a run: graceful stop after the current frame.
   - During `JOG`/`STEP`: immediate stop.
2. Long press (≥ 2 s):
   - In `IDLE`: starts a run at the 2 s mark.
   - During a run/JOG/STEP: aborts immediately at the 2 s mark.
   - Release after a long press does nothing.
3. Hold the cable/button closed, power-cycle the board:
   - No motion occurs.
   - Release, then press again; it works.
4. Confirm the cable behaves like a button: connect = press, disconnect =
   release. A permanently connected cable is not a run-enable input.

## 8. Timeout check

1. Remove the film/holder so no notch can be detected.
2. Send `START`.
3. After about 15 s the run must stop with:
   ```
   < EVT RUN_STOPPED reason=notch_timeout frames=0
   ```
4. The state must be `IDLE`, the motor disabled, and the machine ready for
   `START`.
5. `JOG` must **not** time out. Send `JOG 1000` and confirm it completes (or
   stop it with `STOP`).

## 9. Full-run acceptance

1. Load a 36-frame film (or four 4-frame holders with a gap).
2. Send `START` (or press the button).
3. Confirm:
   - One photo per notch.
   - The holder gap (47–57 mm) does not cause a timeout.
   - The run continues into the next holder.
   - `STOP` finishes the current frame and stops.
   - `ABORT` stops immediately.
   - The total time is under 10 minutes.
4. After the last frame, wait 15 s; the run ends with
   `reason=notch_timeout`.
5. Check the images for alignment and focus.

## 10. Record the values

Copy the output of `CONFIG` and record it here or in a separate machine log:

```
> CONFIG
...
```

Machine-specific values to record:

| Key | Value | Date | Notes |
|---|---|---|---|
| `microsteps` | | | |
| `runCurrentMa` | | | |
| `holdCurrentPercent` | | | |
| `stealthChop` | | | |
| `seekSpeed` | | | |
| `acceleration` | | | |
| `jogSpeed` | | | |
| `offsetSteps` | | | |
| `notchTimeoutMs` | | | |
| `settleMs` | | | |
| `focusMs` | | | |
| `shutterMs` | | | |
| `postShotMs` | | | |

## 11. Recalibration triggers

Recalibrate the offset (and possibly speed/current) when:

- `microsteps` changes.
- `seekSpeed` or `acceleration` changes.
- The motor, driver, gear, sprocket, or rack is replaced.
- The camera or lens changes (timings).
- The notch switch or its mounting changes.
- Frames become consistently misaligned.
- The machine starts skipping steps.

## 12. Troubleshooting during calibration

See [10-troubleshooting.md](10-troubleshooting.md). The most common issues are:

- Motor does not move → EN, 12 V, STEP/DIR, UART config.
- Motor moves the wrong way → swap one coil pair.
- Notch missed → switch mounting, debounce, speed.
- Camera does not fire → optocoupler polarity, `focusMs`/`shutterMs`.
- Offset drifts → speed/accel changed, mechanical slip, current too low.
