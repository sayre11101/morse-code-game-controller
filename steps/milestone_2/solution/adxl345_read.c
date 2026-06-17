#include <zephyr/kernel.h>
#include <zephyr/drivers/i2c.h>
#include <stdio.h>
#include <stdint.h>
#include "headers/adxl345_read.h"
#include "adxl_regs.h"

#define I2C_NODE DT_NODELABEL(i2c0)
#define ADXL345_ADDR 0x53

#define SCALE_FACTOR 0.004f // 4mg/LSB

void adxl345_read_samples(void) {
    const struct device *i2c_dev = DEVICE_DT_GET(I2C_NODE);
    if (!device_is_ready(i2c_dev)) return;

    uint8_t status = 0;
    
    // 1. Poll FIFO_STATUS (0x39) until the trigger/watermark is reached (bit 7 is set)
    do {
        i2c_reg_read_byte(i2c_dev, ADXL345_ADDR, 0x39, &status);
        k_sleep(K_MSEC(10)); // Yield to prevent watchdog timeout
    } while ((status & 0x80) == 0);

    // 2. Burst read 12 samples (6 bytes per sample * 12 = 72 bytes)
    uint8_t buffer[72];
    i2c_burst_read(i2c_dev, ADXL345_ADDR, 0x32, buffer, 72);

    // 3. Extract the 12th sample (bytes 66 through 71)
    int16_t raw_x = (int16_t)((buffer[67] << 8) | buffer[66]);
    int16_t raw_y = (int16_t)((buffer[69] << 8) | buffer[68]);
    int16_t raw_z = (int16_t)((buffer[71] << 8) | buffer[70]);

    // 4. Convert to gravity floats
    float x_g = raw_x * SCALE_FACTOR;
    float y_g = raw_y * SCALE_FACTOR;
    float z_g = raw_z * SCALE_FACTOR;

    // 5. Print formatting
    printf("X: %.2f g, Y: %.2f g, Z: %.2f g\n", x_g, y_g, z_g);

    // 6. Clear interrupts by reading INT_SOURCE (0x30)
    uint8_t dummy;
    i2c_reg_read_byte(i2c_dev, ADXL345_ADDR, 0x30, &dummy);
}