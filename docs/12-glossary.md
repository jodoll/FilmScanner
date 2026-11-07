# 12 — Glossary

Terms used throughout the documentation, in plain language.

| Term | Meaning |
|---|---|
| **Abort** | Stop immediately in every state. Cancels a pending photo; an active shutter pulse is completed. |
| **AccelStepper** | Arduino library that generates STEP/DIR pulses with acceleration and position tracking. Used for all motion. |
| **Active-low** | A signal that is "on" when the pin reads LOW. The notch switch, control input and TMC `EN` are active-low. |
| **Back-EMF** | The voltage a spinning motor generates. StallGuard uses it to estimate load. |
| **Baud** | Serial speed in bits per second. The USB port and TMC UART use 115200 by default. |
| **Blanking time** | A short period after a chopper switching edge during which the driver ignores current measurements, to avoid noise. Set by the library; not a config key. |
| **Chopper** | The part of the driver that switches the coil current on and off rapidly to regulate it. |
| **CoolStep** | A TMC2209 feature that lowers current when the load is low. Disabled here for repeatable framing. |
| **Config slice** | A narrow group of related config values (`InputConfig`, `CameraConfig`, `MotionConfig`, `RunConfig`, `DriverConfig`) passed to the one component that needs it. Components never receive `ConfigManager`. |
| **CRC** | Cyclic Redundancy Check. A checksum used to detect corrupted EEPROM data. |
| **DIR** | Direction signal to the stepper driver. |
| **EEPROM** | Non-volatile memory inside the Uno. Stores the configuration across power cycles. |
| **EN** | Enable pin of the TMC2209. Active-low: LOW = driver enabled. |
| **Frame** | One negative image. |
| **Full step** | One of the 200 magnetic positions per motor revolution (1.8°). |
| **Graceful stop** | Finish the current frame and photo, then stop. Triggered by `STOP` or a short button press during a run. |
| **Holder** | The carrier for a strip of four negatives. |
| **Jog** | Manual move by a signed step count, used for setup and unjamming. No notch timeout. |
| **Long press** | Holding the control input for 2 s. Fires at the 2 s mark. Aborts in active states, starts in IDLE/FAULT. |
| **Magazine** | The feeder that supplies successive holders. |
| **MicroPlyer** | TMC2209 interpolator that smooths 1/16 input to 1/256 internally. |
| **Microstep** | An electrical intermediate position between two full steps. 1/16 = 16 microsteps per full step. |
| **Module (gear)** | Gear size convention. Fischertechnik uses m = 1.5 mm; pitch diameter = teeth × 1.5 mm. |
| **Notch** | A 5 mm recess in the rack; the microswitch closes when the lever drops into it. |
| **Offset (`offsetSteps`)** | Signed number of microsteps applied after the controlled stop to center the frame. Calibrated on the machine. |
| **PDN_UART** | The single UART pin on the TMC2209 module. |
| **Peak current** | The maximum coil current, √2 times the RMS value. |
| **Rack** | The toothed strip on the holder that the Z30 drives. |
| **RMS current** | The effective (heating) current per phase. Motor ratings and `runCurrentMa` are RMS. |
| **Run** | Automatic continuous scanning: one photo per notch until stopped or timed out. |
| **Settle** | A short pause after motion stops so the mechanics stop vibrating before the photo. |
| **Shutter** | The camera output that fires the exposure. |
| **Single** | The `STEP` command: exactly one frame. |
| **Sprocket** | The gear (Z30) that engages the rack. |
| **StallGuard** | TMC2209 sensorless load/stall detection. Not used here; the notch switch is the reference. |
| **StealthChop** | TMC2209 quiet voltage-mode chopper. Default on. |
| **STEP** | Step-pulse signal to the driver; one pulse = one microstep. |
| **Stop** | Graceful stop during a run; immediate stop during manual moves. |
| **SpreadCycle** | TMC2209 current-mode chopper with more torque at speed but more noise. |
| **Standstill mode** | What the driver does when the motor stops: hold, freewheel, brake. The firmware uses `NORMAL` (hold) while enabled. |
| **TMC2209** | The stepper driver IC/module. Configured over UART; moves via STEP/DIR. |
| **TMCStepper** | Arduino library for configuring Trinamic drivers over UART/SPI. It does not generate steps. |
| **TPOWERDOWN** | Delay before the driver reduces to hold current after the last step. |
| **UART** | Asynchronous serial interface used to configure the TMC2209. |
| **VACTUAL** | TMC2209 internal velocity mode. Not used: no position tracking, no exact offset stop. |
| **Watchdog** | A hardware timer that resets the board if the main loop stops kicking it. 2 s here. |
| **Z20 / Z30** | Fischertechnik gears with 20 and 30 teeth. The Z20 is on the motor; the Z30 drives the rack. |
| **µstep** | Shorthand for microstep. |
