#include <zephyr/kernel.h>
#include <zephyr/drivers/i2c.h>
#include <stdio.h>
#include "headers/adxl345_setup.h"
#include "adxl_regs.h"

#define I2C_NODE DT_NODELABEL(i2c0)

void adxl345_setup(void) {
    const struct device *i2c_dev = DEVICE_DT_GET(I2C_NODE);
    if (!device_is_ready(i2c_dev)) {
        printf("I2C device not ready\n");
        return;
    }

    // 1. Offsets
    i2c_reg_write_byte(i2c_dev, ADXL345_ADDR, ADXL345_REG_OFSX, 0x05); // X: +5
    i2c_reg_write_byte(i2c_dev, ADXL345_ADDR, ADXL345_REG_OFSY, 0xFC); // Y: -4 (Two's complement)
    i2c_reg_write_byte(i2c_dev, ADXL345_ADDR, ADXL345_REG_OFSZ, 0x02); // Z: +2

    // 2. Threshold (1.5g / 0.0625g = 24 -> 0x18)
    i2c_reg_write_byte(i2c_dev, ADXL345_ADDR, ADXL345_REG_THRESH_ACT, 0x18);

    // 3. Activity detect on X axis only
    i2c_reg_write_byte(i2c_dev, ADXL345_ADDR, ADXL345_REG_ACT_INACT_CTL, 0x40);

    // 4. Bandwidth (400Hz = 0x0C)
    i2c_reg_write_byte(i2c_dev, ADXL345_ADDR, ADXL345_REG_BW_RATE, 0x0C);

    // 5. Data Format (+/- 2g, 10-bit -> 0x00)
    i2c_reg_write_byte(i2c_dev, ADXL345_ADDR, ADXL345_REG_DATA_FORMAT, 0x00);

    // 6. FIFO Control (Trigger mode, INT1, 12 samples -> 0xCC)
    i2c_reg_write_byte(i2c_dev, ADXL345_ADDR, ADXL345_REG_FIFO_CTL, 0xCC);

    // 7. Power Control (Measurement Mode -> 0x08)
    i2c_reg_write_byte(i2c_dev, ADXL345_ADDR, ADXL345_REG_POWER_CTL, 0x08);
}