#include <zephyr/kernel.h>
#include <zephyr/drivers/i2c.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include "headers/adxl345_setup.h"
#include "headers/adxl345_read.h"
#include "adxl_regs.h"

#define I2C_NODE DT_NODELABEL(i2c0)
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
    i2c_reg_write_byte(i2c_dev, ADXL345_ADDR, ADXL345_REG_POWER_CTL, 0x00);

    // 2. Reconfigure threshold to 1.2g (1.2 / 0.0625 = 19.2 -> 19 -> 0x13)
    i2c_reg_write_byte(i2c_dev, ADXL345_ADDR, ADXL345_REG_THRESH_ACT, 0x13);

    // 3. Change FIFO watermark to 8 samples, keep trigger mode (0xC0 + 0x08 = 0xC8)
    i2c_reg_write_byte(i2c_dev, ADXL345_ADDR, ADXL345_REG_FIFO_CTL, 0xC8);

    // 4. Resume Active Measurement (Set Measure bit D3)
    i2c_reg_write_byte(i2c_dev, ADXL345_ADDR, ADXL345_REG_POWER_CTL, 0x08);

    // 5. Wait for new trigger
    uint8_t status = 0;
    do {
        i2c_reg_read_byte(i2c_dev, ADXL345_ADDR, ADXL345_REG_FIFO_STATUS, &status);
        k_sleep(K_MSEC(10));
    } while ((status & 0x80) == 0);

    k_usleep(5);

    // 6. Read and print 8 FIFO entries as complete 6-byte X/Y/Z samples.
    uint8_t sample[6];

    // 7. Print all 8 samples
    for (int i = 0; i < 8; i++) {
        i2c_burst_read(i2c_dev, ADXL345_ADDR, ADXL345_REG_DATAX0, sample, sizeof(sample));
        int16_t raw_x = (int16_t)((sample[1] << 8) | sample[0]);
        int16_t raw_y = (int16_t)((sample[3] << 8) | sample[2]);
        int16_t raw_z = (int16_t)((sample[5] << 8) | sample[4]);

        float x_g = raw_x * SCALE_FACTOR;
        float y_g = raw_y * SCALE_FACTOR;
        float z_g = raw_z * SCALE_FACTOR;

        printf("X: %.2f g, Y: %.2f g, Z: %.2f g\n", x_g, y_g, z_g);
    }

    exit(0);
}