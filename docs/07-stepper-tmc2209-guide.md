# 07 — Stepper motor and TMC2209 guide

This document explains the motor, the driver, and every value the firmware sets.
It assumes no prior knowledge of stepper drivers. Read it before changing
`runCurrentMa`, `microsteps`, `stealthChop`, `seekSpeed` or `acceleration`.

## 1. The motor in one paragraph

The Artillery 42-35 is a **bipolar stepper motor** with 200 full steps per
revolution (1.8° per step). Its shaft does not spin continuously like a DC
motor; it moves in discrete steps and holds its position when energized. That is
ideal for a scanner: the holder can be moved a precise distance and stopped
without an encoder.

## 2. Full steps and microsteps

| Term | Meaning |
|---|---|
| Full step | One of the 200 magnetic positions per revolution |
| Microstep | An electrical intermediate position between two full steps |
| 1/16 microstepping | 16 microsteps per full step → 3,200 microsteps/rev |
| MicroPlyer | The TMC2209 internally interpolates 1/16 input to 1/256 for smooth motion |

At 1/16 microstepping:

- 3,200 microsteps per motor revolution.
- 94.2478 mm of rack travel per motor revolution.
- **33.953 microsteps per mm**.
- 0.02945 mm per microstep.
- 1,290 microsteps for a 38 mm frame.

Microstepping improves smoothness and resolution, not raw torque. The firmware
uses 1/16 because it gives sub-0.03 mm resolution with a comfortable step rate
and flash footprint. More microsteps would increase step counts and CPU load
without a practical benefit.

> **If you change `microsteps`, the offset calibration changes.** The same
> physical offset is `offsetSteps × (newMicrosteps / oldMicrosteps)`.

## 3. What the TMC2209 does

The TMC2209 is a **stepper driver**: it takes low-voltage STEP/DIR signals from
the Arduino and switches the motor coils with the 12 V supply. It also contains
a chopper that regulates the coil current, plus optional "quiet" and
"load-sensing" features.

Two interfaces matter:

| Interface | Purpose |
|---|---|
| STEP/DIR | One pulse = one microstep; DIR sets direction. This is how the motor moves. |
| UART | Configuration and diagnostics: current, microsteps, chopper mode, fault readback. |

**The firmware uses both.** TMCStepper writes the UART registers; AccelStepper
generates STEP pulses. The TMC2209's own `VACTUAL` velocity mode is **not** used
because it has no position counter and cannot stop at an exact offset.

## 4. Current: RMS vs peak

Motor current is usually quoted as **RMS** (root-mean-square) per phase. The
TMC2209's peak current is √2 times the RMS value.

Current is set by the driver's sense resistors and the internal current-scale
register. The BIGTREETECH TMC2209 V1.3 uses **110 mΩ** sense resistors.

The datasheet formula for analog (potentiometer) scaling is:

```
I_RMS = (325 mV / (R_sense + 20 mΩ)) × (1/√2) × (V_VREF / 2.5 V)
```

With `R_sense = 0.11 Ω`, this simplifies to:

```
I_RMS ≈ V_VREF / √2
I_PEAK = I_RMS × √2 = V_VREF
```

Examples:

| Vref | I_RMS | I_PEAK |
|---|---|---|
| 1.0 V | 0.71 A | 1.0 A |
| 1.2 V | 0.85 A | 1.2 A |
| 1.4 V | 0.99 A | 1.4 A |

The factory setting is about **1.2 V → 0.85 A RMS**, and the BTT board is often
quoted as "about 0.9 A RMS" out of the box.

### 4.1 UART current setting

In UART mode the firmware calls:

```cpp
driver.rms_current(900, 0.5);   // 900 mA RMS run, 50 % hold
```

TMCStepper converts this to the correct current-scale register using the
`Rsense` value passed to its constructor (`0.11`). The potentiometer is not the
control path; the firmware also sets `I_scale_analog(false)`.

### 4.2 What the default 900 mA means

- 900 mA RMS ≈ **1.27 A peak**.
- The 42-35 motor is a 42 mm NEMA 17; its rated current is typically around
  1.5 A/phase, but verify the label. 900 mA is a conservative ~60 % and leaves
  thermal headroom.
- If the motor is weak or skips steps under load, raise `runCurrentMa` in small
  steps (e.g. 900 → 1000 → 1100) and check temperature.
- If the motor or driver is hot, lower it.
- `holdCurrentPercent = 50` means 450 mA RMS while holding between frames.

### 4.3 Thermal check

After 10 minutes of continuous scanning:

- Motor case: below ~70 °C is acceptable; below 60 °C is comfortable.
- Driver: warm but not painful to touch; a small heatsink is recommended.
- If either is too hot, reduce `runCurrentMa` before adding cooling.

The holder does not back-drive when disabled, so the firmware disables the
driver in `IDLE` and `FAULT`. No holding current is used at rest.

## 5. Chopper modes: stealthChop vs spreadCycle

| Mode | How it works | Strengths | Weaknesses |
|---|---|---|---|
| **stealthChop** | Voltage-mode PWM; the driver shapes the coil voltage to follow a sine | Very quiet, smooth at low/medium speed | Less torque at high speed; can lose steps if overloaded |
| **spreadCycle** | Current-mode chopper with fast decay | More torque and dynamics at high speed | Audible switching noise |

The firmware default is **stealthChop on** (`stealthChop = 1`). At 400 µsteps/s
the motor turns at only 7.5 rpm, where stealthChop has plenty of torque and is
essentially silent. If the mechanism binds and steps are lost, try
`SET stealthChop 0` before raising the current.

```cpp
driver.en_spreadCycle(!config.stealthChop);
driver.pwm_autoscale(true);   // required for stealthChop
```

## 6. MicroPlyer

MicroPlyer is the TMC2209's internal interpolator. It takes each 1/16 microstep
pulse and smoothly interpolates it to 1/256 internally. The Arduino still sends
one pulse per 1/16 microstep, but the motor current waveform is much smoother.

- Leave it enabled (default).
- It does not change the step count or position.
- It is the reason 1/16 input feels smooth.

## 7. CoolStep

CoolStep reduces coil current automatically when the load is low and raises it
when more torque is needed.

- Saves energy and heat.
- Changes torque dynamically, which can make microstep positioning slightly less
  repeatable.
- **Disabled in this firmware** for predictable framing. At 7.5 rpm and 900 mA
  the heat is already modest.

You do not need to configure CoolStep. If you later enable it, test the frame
offset repeatability before trusting it.

## 8. StallGuard4

StallGuard4 estimates load from the motor's back-EMF and can flag a stall
without an encoder.

- **Not used in this firmware.**
- At very low speed the signal is weak and the threshold is unreliable.
- The notch microswitch is the position reference; a missed notch is caught by
  the 15 s timeout.
- If readback is wired, a driver fault can still be reported via
  `driver_error()`.

## 9. Standstill and hold current

When the motor stops, the driver can hold the shaft with current or release it.

| Standstill mode | Behavior |
|---|---|
| `NORMAL` | Hold current is applied (default) |
| `FREEWHEELING` | Coils are de-energized; the shaft can move |
| `STRONG_BRAKING` | Both coils shorted; strong braking |
| `BRAKING` | Both coils shorted; braking |

The firmware uses `NORMAL` while the driver is enabled, with
`holdCurrentPercent = 50`. Between frames the driver stays enabled and holds the
holder still during the photo. In `IDLE` and `FAULT` the **EN pin is driven
high**, which disables the driver entirely; the holder does not move because the
mechanism does not back-drive. The `ENABLE` command is a maintenance override
that keeps the driver energized in `IDLE` (see `05-serial-protocol.md`).

`TPOWERDOWN` sets how long after the last step the driver waits before reducing
to hold current. The default (20) is fine; it is not exposed as a configuration
key.

## 10. UART details

### 10.1 Wiring

- `PDN_UART` is the single UART pin on the BTT V1.3.
- One-wire (TX only): Arduino D5 → 1 kΩ → `PDN_UART`.
- Two-wire (readback): the same, plus `PDN_UART` → Arduino D4.
- `MS1`/`MS2` to GND selects address 0.
- `CLK` to GND selects the internal clock.

### 10.2 Baud rate

The TMC2209 performs automatic baud-rate detection. The firmware uses 115200.
SoftwareSerial on a 16 MHz Uno is reliable for transmission at this rate;
readback may be less reliable. If `test_connection()` fails:

1. Lower the UART baud to 57600 or 19200 (compile-time constant).
2. Check the 1 kΩ resistor and the `PDN_UART` connection.
3. Fall back to one-wire; the machine still runs, just without diagnostics.

### 10.3 Address

The default address is 0 (both MS pins low). The firmware constructs
`TMC2209Stepper(&softSerial, 0.11, 0)`. If you change the address straps,
update the constructor and the docs.

### 10.4 What the firmware writes

```cpp
driver.begin();
driver.toff(3);                  // chopper on-time; library default
driver.blank_time(24);           // blanking time; library default
driver.I_scale_analog(false);    // UART current, ignore the pot
driver.internal_Rsense(false);   // external 110 mΩ sense resistors
driver.microsteps(config.microsteps);
driver.rms_current(config.runCurrentMa, config.holdCurrentPercent / 100.0f);
driver.en_spreadCycle(!config.stealthChop);
driver.pwm_autoscale(true);
driver.VACTUAL(0);               // ensure STEP/DIR mode
driver.TPOWERDOWN(20);
```

`toff`, `blank_time` and `TPOWERDOWN` are not exposed as config keys. They are
chopper timing details; change them only if you understand the effect on the
current waveform.

## 11. Recommended settings for this machine

| Setting | Value | Why |
|---|---|---|
| `microsteps` | 16 | 0.029 mm resolution, smooth, 1,290 µsteps/frame |
| `runCurrentMa` | 900 | Conservative for a 42-35 at 12 V; ~1.27 A peak |
| `holdCurrentPercent` | 50 | Holds position between frames with modest heat |
| `stealthChop` | 1 | Quiet and smooth at 7.5 rpm |
| `seekSpeed` | 400 µsteps/s | 11.8 mm/s, 3.4 s/frame, 424 ms notch window |
| `acceleration` | 2000 µsteps/s² | 0.2 s ramp, 1.18 mm; no jerk |
| `jogSpeed` | 200 µsteps/s | Slow, precise manual moves |
| `offsetSteps` | 0 | Calibrate on the machine |
| `notchTimeoutMs` | 15000 | Covers a 47–57 mm holder gap with ~10× margin |
| `maxSeekSteps` | 0 | Timeout is the intended stop criterion |
| CoolStep | off | Predictable framing |
| StallGuard | off | Unreliable at low speed; switch is the reference |
| MicroPlyer | on | Smooth motion |
| Standstill | `NORMAL` | Hold between frames |

## 12. What goes wrong if a value is wrong

| Symptom | Likely cause | Fix |
|---|---|---|
| Motor hums but does not turn | Wrong coil wiring; current too low; EN not low | Check A1/A2/B1/B2; raise current; check EN |
| Motor turns the wrong way | DIR polarity | Swap one coil pair or invert DIR |
| Skipped steps / lost position | Speed or acceleration too high; current too low; binding | Lower `seekSpeed`/`acceleration`, raise current, check mechanics |
| Motor very hot | Current too high; hold current too high | Lower `runCurrentMa`/`holdCurrentPercent` |
| Driver very hot | Poor cooling; high current | Add heatsink/fan; lower current |
| Whining/noise at low speed | spreadCycle, or high current | Enable stealthChop; lower current |
| Notch missed | Speed too high; debounce too long; switch not closing | Lower speed, reduce `notchDebounceMs`, check switch |
| Frame offset drifts | Microsteps/speed/accel changed after calibration | Recalibrate `offsetSteps` |
| UART readback fails | One-wire wiring; baud; resistor | Lower baud; check wiring; run write-only |
| Motor does not hold between frames | Driver disabled or hold current 0 | Normal in `IDLE`; during a run check `holdCurrentPercent` |

## 13. Safety notes

- Never connect or disconnect the motor while the driver is powered; the
  inductive kick can destroy the driver.
- Use a 12 V supply rated for at least 2 A. Add a 100 µF capacitor across VM/GND.
- Keep the Arduino ground and the 12 V ground connected.
- Do not power the motor from the Arduino 5 V rail.
- Do not exceed 2 A RMS; the driver is rated 2 A RMS continuous with cooling.
- The firmware watchdog disables the motor after a hang, but do not rely on it
  as the only safety measure.
- Use `ABORT` or a 2 s long press for an immediate stop.

## 14. Glossary

See [12-glossary.md](12-glossary.md) for terms such as RMS, chopper, microstep,
back-EMF, and stall.
