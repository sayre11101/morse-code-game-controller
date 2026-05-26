#!/bin/bash

# Navigate to the correct directory (if not already there)
cd /app

# Generate the main.c file with the updated 12-loop logic
cat << 'EOF' > main.c
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <stdio.h>

#define ADXL345_ADDR 0x53

int main(void) {
    const struct device *i2c_dev = DEVICE_DT_GET(DT_NODELABEL(i2c0));
    if (!device_is_ready(i2c_dev)) {
        printf("I2C device not ready\n");
        return -1;
    }

    i2c_reg_write_byte(i2c_dev, ADXL345_ADDR, 0x1E, 0x05);
    i2c_reg_write_byte(i2c_dev, ADXL345_ADDR, 0x1F, 0xFC);
    i2c_reg_write_byte(i2c_dev, ADXL345_ADDR, 0x20, 0x02);
    i2c_reg_write_byte(i2c_dev, ADXL345_ADDR, 0x2C, 0x0C);
    i2c_reg_write_byte(i2c_dev, ADXL345_ADDR, 0x31, 0x02);
    i2c_reg_write_byte(i2c_dev, ADXL345_ADDR, 0x38, 0xCC);
    i2c_reg_write_byte(i2c_dev, ADXL345_ADDR, 0x24, 0x18);
    i2c_reg_write_byte(i2c_dev, ADXL345_ADDR, 0x2E, 0x10);
    i2c_reg_write_byte(i2c_dev, ADXL345_ADDR, 0x2D, 0x08);

    uint8_t fifo_status = 0;
    while ((fifo_status & 0x80) == 0) {
        i2c_reg_read_byte(i2c_dev, ADXL345_ADDR, 0x39, &fifo_status);
        k_msleep(10);
    }

    uint8_t data[6];
    
    // The Fix: Loop 12 times to traverse the realistic FIFO queue
    for (int i = 0; i < 12; i++) {
        i2c_burst_read(i2c_dev, ADXL345_ADDR, 0x32, data, 6);
    }

    int16_t x_raw = (int16_t)((data[1] << 8) | data[0]);
    int16_t y_raw = (int16_t)((data[3] << 8) | data[2]);
    int16_t z_raw = (int16_t)((data[5] << 8) | data[4]);

    float scale = 0.00390625f; 
    
    // Explicitly cast to double to prevent Zephyr warnings
    printf("X: %.2f g, Y: %.2f g, Z: %.2f g\n", (double)(x_raw * scale), (double)(y_raw * scale), (double)(z_raw * scale));

    return 0;
}
EOF

# Provide feedback that the Oracle script successfully generated the file
echo "Oracle successfully generated /app/main.c"