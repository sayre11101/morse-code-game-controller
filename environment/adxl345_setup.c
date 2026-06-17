#include <zephyr/kernel.h>
#include <zephyr/drivers/i2c.h>
#include <stdio.h>
#include "headers/adxl345_setup.h"
#include "adxl_regs.h" /* Assuming your register definitions are here */

/* Standard Zephyr I2C device binding */
#define I2C_NODE DT_NODELABEL(i2c0)
#define ADXL345_ADDR 0x53

void adxl345_setup(void)
{
    /* * TODO: MILESTONE 1
     * Implement the ADXL345 initialization sequence here.
     * Ensure you follow the datasheet for register addresses and formatting.
     */
}