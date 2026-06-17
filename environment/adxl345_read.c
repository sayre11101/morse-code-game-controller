#include <zephyr/kernel.h>
#include <zephyr/drivers/i2c.h>
#include <stdio.h>
#include <stdint.h>
#include "headers/adxl345_read.h"
#include "adxl_regs.h"

/* Standard Zephyr I2C device binding */
#define I2C_NODE DT_NODELABEL(i2c0)
#define ADXL345_ADDR 0x53

void adxl345_read_samples(void) {
    /* * TODO: MILESTONE 2
     * 1. Poll the FIFO_STATUS register until the watermark trigger is reached.
     * 2. Download the samples from the data registers.
     * 3. Convert the raw bytes to floating point 'g' values.
     * 4. Print the final sample formatted exactly as: X: [val] g, Y: [val] g, Z: [val] g
     * 5. Clear the interrupts and reset the trigger mode.
     */

}