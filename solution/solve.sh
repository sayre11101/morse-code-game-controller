#!/bin/bash

# Navigate to the correct directory (if not already there)
cd /app

# Generate the main.c file with the new 3-stage state machine logic
cat << 'EOF' > main.c
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <stdio.h>

#define ADXL345_ADDR 0x53

// Helper function: Polls the FIFO_STATUS register for the Watermark trigger
void wait_for_trigger(const struct device *i2c_dev) {
    uint8_t status = 0;
    while ((status & 0x80) == 0) {
        i2c_reg_read_byte(i2c_dev, ADXL345_ADDR, 0x39, &status);
        k_msleep(10);
    }
}

// Helper function: Reads INT_SOURCE to clear the hardware interrupt flag
void clear_interrupt(const struct device *i2c_dev) {
    uint8_t int_source = 0;
    i2c_reg_read_byte(i2c_dev, ADXL345_ADDR, 0x30, &int_source);
}

int main(void) {
    const struct device *i2c_dev = DEVICE_DT_GET(DT_NODELABEL(i2c0));
    if (!device_is_ready(i2c_dev)) {
        printf("I2C device not ready\n");
        return -1;
    }

    // --- INITIAL BOOT CONFIGURATION ---
    i2c_reg_write_byte(i2c_dev, ADXL345_ADDR, 0x1E, 0x05); // OFSX: +5
    i2c_reg_write_byte(i2c_dev, ADXL345_ADDR, 0x1F, 0xFC); // OFSY: -4
    i2c_reg_write_byte(i2c_dev, ADXL345_ADDR, 0x20, 0x02); // OFSZ: +2
    i2c_reg_write_byte(i2c_dev, ADXL345_ADDR, 0x31, 0x00); // Data format: +/- 2g
    i2c_reg_write_byte(i2c_dev, ADXL345_ADDR, 0x2C, 0x0C); // BW Rate
    i2c_reg_write_byte(i2c_dev, ADXL345_ADDR, 0x38, 0xCC); // FIFO: 12 Samples
    i2c_reg_write_byte(i2c_dev, ADXL345_ADDR, 0x24, 0x18); // Thresh: 1.5g
    i2c_reg_write_byte(i2c_dev, ADXL345_ADDR, 0x27, 0x70);
    i2c_reg_write_byte(i2c_dev, ADXL345_ADDR, 0x2E, 0x12); // INT_ENABLE
    i2c_reg_write_byte(i2c_dev, ADXL345_ADDR, 0x2D, 0x08); // POWER_CTL: Measure Mode

    uint8_t data[6];
    float scale = 0.00390625f; 
    int16_t x_raw, y_raw, z_raw;

    // --- STAGE 1: First Batch (12 Samples) ---
    wait_for_trigger(i2c_dev);
    for (int i = 0; i < 12; i++) {
        i2c_burst_read(i2c_dev, ADXL345_ADDR, 0x32, data, 6);
    }
    // Print ONLY the 12th sample
    x_raw = (int16_t)((data[1] << 8) | data[0]);
    y_raw = (int16_t)((data[3] << 8) | data[2]);
    z_raw = (int16_t)((data[5] << 8) | data[4]);
    printf("X: %.2f g, Y: %.2f g, Z: %.2f g\n", (double)(x_raw * scale), (double)(y_raw * scale), (double)(z_raw * scale));
    
    clear_interrupt(i2c_dev);

    // --- STAGE 2: Second Batch (12 Samples) ---
    wait_for_trigger(i2c_dev);
    for (int i = 0; i < 12; i++) {
        i2c_burst_read(i2c_dev, ADXL345_ADDR, 0x32, data, 6);
    }
    // Print ONLY the 12th sample
    x_raw = (int16_t)((data[1] << 8) | data[0]);
    y_raw = (int16_t)((data[3] << 8) | data[2]);
    z_raw = (int16_t)((data[5] << 8) | data[4]);
    printf("X: %.2f g, Y: %.2f g, Z: %.2f g\n", (double)(x_raw * scale), (double)(y_raw * scale), (double)(z_raw * scale));
    
    clear_interrupt(i2c_dev);

    // --- STAGE 3: Reconfigure & Dump Array ---
    i2c_reg_write_byte(i2c_dev, ADXL345_ADDR, 0x2D, 0x00); // Standby
    i2c_reg_write_byte(i2c_dev, ADXL345_ADDR, 0x24, 0x13); // New Thresh
    i2c_reg_write_byte(i2c_dev, ADXL345_ADDR, 0x38, 0xC8); // New FIFO
    i2c_reg_write_byte(i2c_dev, ADXL345_ADDR, 0x2D, 0x08); // Measure

    wait_for_trigger(i2c_dev);
    for (int i = 0; i < 8; i++) {
        i2c_burst_read(i2c_dev, ADXL345_ADDR, 0x32, data, 6);
        
        x_raw = (int16_t)((data[1] << 8) | data[0]);
        y_raw = (int16_t)((data[3] << 8) | data[2]);
        z_raw = (int16_t)((data[5] << 8) | data[4]);
        
        // Print ALL 8 samples
        printf("X: %.2f g, Y: %.2f g, Z: %.2f g\n", (double)(x_raw * scale), (double)(y_raw * scale), (double)(z_raw * scale));
    }

    return 0;
}
EOF

echo "Oracle successfully generated /app/main.c"