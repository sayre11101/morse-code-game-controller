#include <zephyr/kernel.h>
#include <zephyr/drivers/i2c.h>
#include <stdio.h>
#include <stdint.h>
#include "headers/adxl345_read.h"
#include "adxl_regs.h"

#define I2C_NODE DT_NODELABEL(i2c0)

#define SCALE_FACTOR 0.004f // 4mg/LSB

void adxl345_read_samples(void) {
    const struct device *i2c_dev = DEVICE_DT_GET(I2C_NODE);
    if (!device_is_ready(i2c_dev)) return;

    uint8_t status = 0;
    
    // 1. Poll FIFO_STATUS until the trigger/watermark is reached (bit 7 is set)
    do {
        i2c_reg_read_byte(i2c_dev, ADXL345_ADDR, ADXL345_REG_FIFO_STATUS, &status);
        k_sleep(K_MSEC(10)); // Yield to prevent watchdog timeout
    } while ((status & 0x80) == 0);

    k_usleep(5);

    // 2. Read 12 FIFO entries as complete 6-byte X/Y/Z samples.
    uint8_t sample[6] = {0};
    int16_t raw_x = 0;
    int16_t raw_y = 0;
    int16_t raw_z = 0;

    for (int i = 0; i < 12; i++) {
        i2c_burst_read(i2c_dev, ADXL345_ADDR, ADXL345_REG_DATAX0, sample, sizeof(sample));
        raw_x = (int16_t)((sample[1] << 8) | sample[0]);
        raw_y = (int16_t)((sample[3] << 8) | sample[2]);
        raw_z = (int16_t)((sample[5] << 8) | sample[4]);
    }

    // 4. Convert to gravity floats
    float x_g = raw_x * SCALE_FACTOR;
    float y_g = raw_y * SCALE_FACTOR;
    float z_g = raw_z * SCALE_FACTOR;

    // 5. Print formatting
    printf("X: %.2f g, Y: %.2f g, Z: %.2f g\n", x_g, y_g, z_g);

    // 6. Clear interrupts by reading INT_SOURCE
    uint8_t dummy;
    i2c_reg_read_byte(i2c_dev, ADXL345_ADDR, ADXL345_REG_INT_SOURCE, &dummy);

    // Datasheet trigger reset: switch FIFO to bypass, then back to trigger.
    i2c_reg_write_byte(i2c_dev, ADXL345_ADDR, ADXL345_REG_FIFO_CTL, 0x00);
    i2c_reg_write_byte(i2c_dev, ADXL345_ADDR, ADXL345_REG_FIFO_CTL, 0xCC);
}