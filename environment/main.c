#include <zephyr/kernel.h>
#include <zephyr/drivers/i2c.h>
#include <stdio.h>
#include <stdlib.h>
#include "headers/adxl345_setup.h"
#include "headers/adxl345_read.h"
#include "adxl_regs.h"

/* Standard Zephyr I2C device binding */
#define I2C_NODE DT_NODELABEL(i2c0)
#define ADXL345_ADDR 0x53

int main(void)
{
    printf("Starting ADXL345 Application...\n");

    /* Milestone 1: base configuration */
    adxl345_setup();

    /* Milestone 2: two trigger/read cycles */
    adxl345_read_samples();
    adxl345_read_samples();

    exit(0);
}