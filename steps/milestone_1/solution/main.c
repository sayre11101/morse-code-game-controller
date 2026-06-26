#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>

#include "adxl_regs.h"

#define I2C_NODE DT_NODELABEL(i2c0)

#define TRAVEL_ACCEL_THRESHOLD_RAW 1200
#define SETTLED_UP_THRESHOLD_RAW 80
#define SETTLED_DOWN_THRESHOLD_RAW 400
#define FINAL_UP_HOLD_MS 5000

enum key_state {
    ST_TRAVEL_DOWN = 0,
    ST_DOWN,
    ST_TRAVEL_UP,
    ST_UP,
};

static const char *state_name(enum key_state st)
{
    switch (st) {
    case ST_TRAVEL_DOWN:
        return "TRAVEL_DOWN";
    case ST_DOWN:
        return "DOWN";
    case ST_TRAVEL_UP:
        return "TRAVEL_UP";
    case ST_UP:
    default:
        return "UP";
    }
}

static int16_t read_x_raw(const struct device *i2c_dev)
{
    uint8_t sample[6] = {0};
    int rc = i2c_burst_read(i2c_dev, ADXL345_ADDR, ADXL345_REG_DATAX0, sample, sizeof(sample));
    if (rc != 0) {
        return 0;
    }

    return (int16_t)((sample[1] << 8) | sample[0]);
}

static int64_t bw_rate_to_period_us(uint8_t rate_code)
{
    switch (rate_code & 0x0F) {
    case 0x0F: return 313;   /* 3200 Hz */
    case 0x0E: return 625;   /* 1600 Hz */
    case 0x0D: return 1250;  /* 800 Hz */
    case 0x0C: return 2500;  /* 400 Hz */
    case 0x0B: return 5000;  /* 200 Hz */
    case 0x0A: return 10000; /* 100 Hz */
    case 0x09: return 20000; /* 50 Hz */
    case 0x08: return 40000; /* 25 Hz */
    default:
        return 10000;
    }
}

static enum key_state classify_state(int16_t x_raw, enum key_state current, enum key_state last_motion)
{
    ARG_UNUSED(last_motion);

    if (x_raw >= TRAVEL_ACCEL_THRESHOLD_RAW) {
        return ST_TRAVEL_DOWN;
    }

    if (x_raw <= -TRAVEL_ACCEL_THRESHOLD_RAW) {
        return ST_TRAVEL_UP;
    }

    if (x_raw >= -SETTLED_UP_THRESHOLD_RAW && x_raw <= SETTLED_UP_THRESHOLD_RAW) {
        return ST_UP;
    }

    if (x_raw >= -SETTLED_DOWN_THRESHOLD_RAW && x_raw <= SETTLED_DOWN_THRESHOLD_RAW) {
        return ST_DOWN;
    }

    return current;
}

int main(void)
{
    const struct device *i2c_dev = DEVICE_DT_GET(I2C_NODE);
    const uint8_t bw_rate_cfg = 0x0F;
    const int64_t sample_period_us = bw_rate_to_period_us(bw_rate_cfg);
    enum key_state current = ST_UP;
    enum key_state last_motion = ST_TRAVEL_UP;
    int64_t state_start_us;
    int64_t now_us;
    int64_t sample_count = 0;

    if (!device_is_ready(i2c_dev)) {
        printf("I2C device not ready\n");
        return -1;
    }

    /* Non-FIFO, non-Link configuration path (primary oracle variant). */
    i2c_reg_write_byte(i2c_dev, ADXL345_ADDR, ADXL345_REG_BW_RATE, bw_rate_cfg);
    i2c_reg_write_byte(i2c_dev, ADXL345_ADDR, ADXL345_REG_DATA_FORMAT, 0x0B);
    i2c_reg_write_byte(i2c_dev, ADXL345_ADDR, ADXL345_REG_THRESH_ACT, 0x18);
    i2c_reg_write_byte(i2c_dev, ADXL345_ADDR, ADXL345_REG_ACT_INACT_CTL, 0x70);
    i2c_reg_write_byte(i2c_dev, ADXL345_ADDR, ADXL345_REG_POWER_CTL, 0x08);

    state_start_us = 0;

    while (1) {
        enum key_state sensed;
        int16_t x_raw;
        int64_t duration_us;

        x_raw = read_x_raw(i2c_dev);
        sample_count++;
        now_us = sample_count * sample_period_us;

        sensed = classify_state(x_raw, current, last_motion);

        if (sensed != current) {
            duration_us = now_us - state_start_us;
            printf("%s %.3f\n", state_name(current), (double)duration_us / 1000000.0);
            current = sensed;
            state_start_us = now_us;
            if (current == ST_TRAVEL_DOWN || current == ST_TRAVEL_UP) {
                last_motion = current;
            }
        }

        if (current == ST_UP) {
            duration_us = now_us - state_start_us;
            if (duration_us >= (int64_t)FINAL_UP_HOLD_MS * 1000) {
                printf("UP %.3f\n", (double)duration_us / 1000000.0);
                printf("END\n");
                fflush(stdout);
                exit(0);
            }
        }
    }

    return 0;
}

