#include <zephyr/kernel.h>
#include <zephyr/drivers/i2c.h>
#include <stdio.h>
#include "headers/adxl345_setup.h"
#include "adxl_regs.h"

#define I2C_NODE DT_NODELABEL(i2c0)
#define ADXL345_ADDR 0x53

void adxl345_setup(void) {
    const struct device *i2c_dev = DEVICE_DT_GET(I2C_NODE);
    if (!device_is_ready(i2c_dev)) {
        printf("I2C device not ready\n");
        return;
    }

    // 1. Offsets
    i2c_reg_write_byte(i2c_dev, ADXL345_ADDR, 0x1E, 0x05); // X: +5
    i2c_reg_write_byte(i2c_dev, ADXL345_ADDR, 0x1F, 0xFC); // Y: -4 (Two's complement)
    i2c_reg_write_byte(i2c_dev, ADXL345_ADDR, 0x20, 0x02); // Z: +2

    // 2. Threshold (1.5g / 0.0625g = 24 -> 0x18)
    i2c_reg_write_byte(i2c_dev, ADXL345_ADDR, 0x24, 0x18);

    // 3. Bandwidth (400Hz = 0x0C)
    i2c_reg_write_byte(i2c_dev, ADXL345_ADDR, 0x2C, 0x0C);

    // 4. Data Format (+/- 2g, 10-bit -> 0x00)
    i2c_reg_write_byte(i2c_dev, ADXL345_ADDR, 0x31, 0x00);

    // 5. FIFO Control (Trigger mode, INT1, 12 samples -> 0xCC)
    i2c_reg_write_byte(i2c_dev, ADXL345_ADDR, 0x38, 0xCC);

    // 6. Power Control (Measurement Mode -> 0x08)
    i2c_reg_write_byte(i2c_dev, ADXL345_ADDR, 0x2D, 0x08);
}