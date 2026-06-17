#include <zephyr/kernel.h>
#include <zephyr/drivers/i2c.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include "headers/adxl345_setup.h"
#include "headers/adxl345_read.h"
#include "adxl_regs.h"

#define I2C_NODE DT_NODELABEL(i2c0)
#define ADXL345_ADDR 0x53
#define SCALE_FACTOR 0.004f

int main(void) {
    printf("Starting ADXL345 Application...\n");

    const struct device *i2c_dev = DEVICE_DT_GET(I2C_NODE);
    if (!device_is_ready(i2c_dev)) return -1;

    /* MILESTONE 1 & 2 Execution */
    adxl345_setup();
    adxl345_read_samples(); // First trigger
    adxl345_read_samples(); // Second trigger

    /* MILESTONE 3: Dynamic Reconfiguration */
    
    // 1. Enter Low Power Standby (Clear Measure bit D3 in POWER_CTL)
    i2c_reg_write_byte(i2c_dev, ADXL345_ADDR, 0x2D, 0x00);

    // 2. Reconfigure threshold to 1.2g (1.2 / 0.0625 = 19.2 -> 19 -> 0x13)
    i2c_reg_write_byte(i2c_dev, ADXL345_ADDR, 0x24, 0x13);

    // 3. Change FIFO watermark to 8 samples, keep trigger mode (0xC0 + 0x08 = 0xC8)
    i2c_reg_write_byte(i2c_dev, ADXL345_ADDR, 0x38, 0xC8);

    // 4. Resume Active Measurement (Set Measure bit D3)
    i2c_reg_write_byte(i2c_dev, ADXL345_ADDR, 0x2D, 0x08);

    // 5. Wait for new trigger
    uint8_t status = 0;
    do {
        i2c_reg_read_byte(i2c_dev, ADXL345_ADDR, 0x39, &status);
        k_sleep(K_MSEC(10));
    } while ((status & 0x80) == 0);

    // 6. Download 8 samples (48 bytes)
    uint8_t buffer[48];
    i2c_burst_read(i2c_dev, ADXL345_ADDR, 0x32, buffer, 48);

    // 7. Print all 8 samples
    for (int i = 0; i < 8; i++) {
        int offset = i * 6;
        int16_t raw_x = (int16_t)((buffer[offset + 1] << 8) | buffer[offset]);
        int16_t raw_y = (int16_t)((buffer[offset + 3] << 8) | buffer[offset + 2]);
        int16_t raw_z = (int16_t)((buffer[offset + 5] << 8) | buffer[offset + 4]);

        float x_g = raw_x * SCALE_FACTOR;
        float y_g = raw_y * SCALE_FACTOR;
        float z_g = raw_z * SCALE_FACTOR;

        printf("X: %.2f, Y: %.2f, Z: %.2f\n", x_g, y_g, z_g);
    }

    exit(0);
}