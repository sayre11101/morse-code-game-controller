# Milestone 1

Implement the ADXL345 initialization sequence in `adxl345_setup.c`.

Use the ADXL345 datasheet at `/app/adxl345.pdf` to determine the required register names, register addresses, bit configurations, and encoded values needed to produce the configuration below.

Program the device over I2C to establish the baseline configuration:

- Set offsets for X to +5, Y to -4 and Z to +2
- Set activity threshold to 1.5g
- Enable activity detection on the X axis only
- Set output data rate to 400 Hz
- Set data format to +/-2g / 10-bit behavior
- Set FIFO trigger mode with 12 samples
- Put the sensor into measurement mode

Your implementation should be callable from `main.c` as `adxl345_setup()`.

## Implementation Note

The verifier monitors I2C register writes via the mock's log format: `Write 0xXX -> 0xXX` (debug output printed when transfers complete). Ensure your register write operations produce this output. The mock validates that `ACT_INACT_CTL` is set to the correct value here, as subsequent milestones depend on this configuration to proceed.
