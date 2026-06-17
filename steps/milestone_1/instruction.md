# Milestone 1

Implement the ADXL345 initialization sequence in `adxl345_setup.c`.

Program the device over I2C to establish the baseline configuration:

- Set offsets to `OFSX=+5`, `OFSY=-4`, `OFSZ=+2`
- Set activity threshold to `1.5g` (`THRESH_ACT = 0x18`)
- Set output data rate to `400 Hz` (`BW_RATE = 0x0C`)
- Set data format to `+/-2g` / 10-bit behavior (`DATA_FORMAT = 0x00`)
- Set FIFO trigger mode with `12` samples on `INT1` (`FIFO_CTL = 0xCC`)
- Put the sensor into measurement mode (`POWER_CTL = 0x08`)

Your implementation should be callable from `main.c` as `adxl345_setup()`.
