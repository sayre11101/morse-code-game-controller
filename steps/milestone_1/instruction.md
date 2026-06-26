# Milestone 1

Implement this milestone in /app/main.c.

Build the key-state timeline for a Morse key that moves between an upper stop and a lower stop. Your program must classify motion into exactly four states named TRAVEL_DOWN, DOWN, TRAVEL_UP, and UP. TRAVEL_DOWN is movement toward the bottom stop, DOWN is resting at the bottom stop, TRAVEL_UP is movement toward the upper stop, and UP is resting at the upper stop.

The key includes an ADXL345 accelerometer. This is a Zephyr project that communicates with the sensor over I2C. The ADXL345 datasheet and programming reference are available at /app/adxl345.pdf. The hardware wiring and device configuration used by this project are defined in /app/app.overlay.

Use Zephyr I2C register and burst APIs to communicate with the sensor. The expected interface is register-based I2C access through functions such as i2c_reg_write_byte, i2c_reg_read_byte, and i2c_burst_read.

Note on register constants: `adxl_regs.h` provides register addresses, but may not provide every symbolic bit-field macro. It is acceptable to configure ADXL345 registers using explicit hex values (for example `DATA_FORMAT = 0x0B`, `POWER_CTL = 0x08`) when symbolic macros are unavailable.

**Do NOT use FIFO trigger mode or LINK configuration.** Configure the ADXL345 in simple measurement mode with direct register reads. Because the travel pulse is brief, either sample very quickly or use a simple interrupt/status-based approach without FIFO.

Set ADXL345 `BW_RATE` high enough to observe brief transit pulses. A practical target is at least 800 Hz (`0x0D`), and 1600-3200 Hz (`0x0E`-`0x0F`) is preferred for this task.

**Critical timing approach:** Use sample-count-based timing, not host uptime. On each DATAX read, increment a counter and compute elapsed time as `elapsed_us = sample_count * (1000000 / ODR_hz)`. This avoids millisecond quantization errors that hide sub-millisecond transit pulses. For example, at 1600 Hz (625 µs/sample), a 4 ms transit spans about 6-7 samples; millisecond timing would report 0 ms duration. Do NOT use `k_uptime_get()` for state duration measurements.

The key gap for this task is 2.0 mm from upper stop to lower stop. A normal travel event is extremely brief compared with hold intervals. Transit time is expected to be about 4-8 ms in either direction.

During travel, acceleration magnitude is large relative to noise. Expect roughly 6-12 g during upward or downward transit, and potentially more than 12 g at stop impact. Noise is small compared with these motion pulses, so you should choose fixed thresholds that align with the expected travel acceleration rather than trying to estimate a noise floor.

## State Transition Logic

Implement a state detector that:

- Reads acceleration quickly enough to catch 4-8 ms transit pulses, or uses a simple non-FIFO interrupt/status mechanism to avoid missing them.
- Configures sensor output data rate accordingly (for example by writing `BW_RATE` to a high ODR setting).
- Uses fixed acceleration thresholds derived from the expected travel pulse magnitude. The important distinction is between large signed travel acceleration and the much quieter resting states.
- Classifies positive travel as TRAVEL_DOWN and negative travel as TRAVEL_UP.
- Treats the key as settled when acceleration returns to a small quiet band, for example less than about 1 g in magnitude on the axis you are monitoring.
- Uses the most recent travel direction to decide the settled state: after TRAVEL_DOWN and then quiet, the state is DOWN; after TRAVEL_UP and then quiet, the state is UP.
- Alternates between TRAVEL_DOWN, DOWN, TRAVEL_UP, and UP as the key moves and then settles.
- **Exits with `exit(0)` after printing END** — do NOT use an infinite loop. When UP has been sustained for 5.0 seconds continuously, print the final UP duration, then print END, then call `exit(0)`.

You do not need to detect the exact hard-stop impact pulse to classify the state correctly. It is sufficient to detect the signed transit pulse and then observe that acceleration has returned to the quiet band.

Print one output line each time the state changes. Each state line must be written as STATE duration_seconds, where STATE is one of the four required names and duration_seconds is the measured time spent in that state. The final line must be END with no duration.

The run ends when UP has been sustained for 5.0 seconds. State order must match the simulated key session, and printed durations must match expected timing within normal simulation tolerance.
