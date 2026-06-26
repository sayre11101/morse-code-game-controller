#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/i2c_emul.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>

#include "adxl_regs.h"

static uint8_t reg_file[0x40];
static uint32_t prng = 1;
static int64_t sample_time_us;

enum key_state {
    KS_UP = 0,
    KS_TRAVEL_DOWN,
    KS_DOWN,
    KS_TRAVEL_UP,
};

struct segment {
    enum key_state state;
    int64_t end_us;
};

/*
 * Deterministic key session for milestone 1.
 * Short high-acceleration travel pulses model a 2 mm key gap with
 * approximately 4-8 ms transit time, followed by rest at each stop.
 */
static const struct segment timeline[] = {
    { KS_UP,          500000 },
    { KS_TRAVEL_DOWN, 506000 },
    { KS_DOWN,        826000 },
    { KS_TRAVEL_UP,   833000 },
    { KS_UP,         1333000 },
    { KS_TRAVEL_DOWN, 1338000 },
    { KS_DOWN,       1988000 },
    { KS_TRAVEL_UP,  1995000 },
    { KS_UP,         6995000 },
};

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

static uint32_t next_rand(void)
{
    prng = prng * 1664525u + 1013904223u;
    return prng;
}

static enum key_state current_state(int64_t now_us)
{
    size_t n = sizeof(timeline) / sizeof(timeline[0]);

    for (size_t i = 0; i < n; i++) {
        if (now_us < timeline[i].end_us) {
            return timeline[i].state;
        }
    }

    return KS_UP;
}

static void synth_sample(enum key_state st, int16_t *raw_x, int16_t *raw_y, int16_t *raw_z)
{
    int16_t noise_x = (int16_t)((next_rand() % 9) - 4);
    int16_t noise_y = (int16_t)((next_rand() % 9) - 4);
    int16_t noise_z = (int16_t)((next_rand() % 9) - 4);

    switch (st) {
    case KS_TRAVEL_DOWN:
        *raw_x = 2000 + noise_x;
        *raw_y = noise_y;
        *raw_z = 240 + noise_z;
        break;
    case KS_DOWN:
        *raw_x = 150 + noise_x;
        *raw_y = noise_y;
        *raw_z = 250 + noise_z;
        break;
    case KS_TRAVEL_UP:
        *raw_x = -2000 + noise_x;
        *raw_y = noise_y;
        *raw_z = 240 + noise_z;
        break;
    case KS_UP:
    default:
        *raw_x = noise_x;
        *raw_y = noise_y;
        *raw_z = 250 + noise_z;
        break;
    }
}

static void encode_xyz(uint8_t *buf, int16_t raw_x, int16_t raw_y, int16_t raw_z)
{
    buf[0] = (uint8_t)(raw_x & 0xFF);
    buf[1] = (uint8_t)((raw_x >> 8) & 0xFF);
    buf[2] = (uint8_t)(raw_y & 0xFF);
    buf[3] = (uint8_t)((raw_y >> 8) & 0xFF);
    buf[4] = (uint8_t)(raw_z & 0xFF);
    buf[5] = (uint8_t)((raw_z >> 8) & 0xFF);
}

static void fill_data_window(uint8_t *buf, size_t len)
{
    int16_t x, y, z;
    int64_t period_us = bw_rate_to_period_us(reg_file[ADXL345_REG_BW_RATE]);

    /*
     * For clients that read contiguous bytes from DATAX0, return one or more
     * packed XYZ samples sequentially.
     */
    for (size_t off = 0; off + 5 < len; off += 6) {
        enum key_state st = current_state(sample_time_us);
        synth_sample(st, &x, &y, &z);
        encode_xyz(&buf[off], x, y, z);
        sample_time_us += period_us;
    }

    /* Fill trailing odd bytes with zeros if caller requested non-multiple of 6. */
    for (size_t off = (len / 6) * 6; off < len; off++) {
        buf[off] = 0;
    }
}

static int adxl345_emul_transfer_i2c(const struct emul *target,
                                     struct i2c_msg *msgs,
                                     int num_msgs,
                                     int addr)
{
    ARG_UNUSED(target);
    ARG_UNUSED(addr);

    if (num_msgs < 1) {
        return 0;
    }

    struct i2c_msg *msg0 = &msgs[0];
    uint8_t reg = msg0->buf[0];

    if (num_msgs == 1 && (msg0->flags & I2C_MSG_READ) == 0 && msg0->len == 2) {
        uint8_t val = msg0->buf[1];

        /* Milestone 1 forbids FIFO mode usage and POWER_CTL.LINK. */
        if (reg == ADXL345_REG_FIFO_CTL && val != 0x00) {
            printf("ERROR: FIFO_CTL must remain 0x00 in milestone 1\n");
            return -EIO;
        }
        if (reg == ADXL345_REG_POWER_CTL && (val & 0x20) != 0) {
            printf("ERROR: POWER_CTL.LINK is forbidden in milestone 1\n");
            return -EIO;
        }

        if (reg < sizeof(reg_file)) {
            reg_file[reg] = val;
        }
        printf("Write 0x%02X -> 0x%02X\n", reg, val);
    } else if (num_msgs == 2 && (msgs[0].flags & I2C_MSG_READ) == 0 && (msgs[1].flags & I2C_MSG_READ) == I2C_MSG_READ) {
        struct i2c_msg *msg1 = &msgs[1];

        if (reg == ADXL345_REG_DEVID && msg1->len == 1) {
            msg1->buf[0] = 0xE5;
        } else if (reg == ADXL345_REG_FIFO_STATUS && msg1->len == 1) {
            /* Trigger-ready bit set; lower nibble advertises available samples. */
            msg1->buf[0] = 0x8C;
            printf("Read 0x39\n");
        } else if (reg == ADXL345_REG_INT_SOURCE && msg1->len == 1) {
            enum key_state st = current_state(sample_time_us);
            if (st == KS_TRAVEL_DOWN || st == KS_TRAVEL_UP) {
                msg1->buf[0] = 0x10;
            } else {
                msg1->buf[0] = 0x00;
            }
            printf("Read 0x30\n");
        } else if (reg == ADXL345_REG_DATAX0 && msg1->len > 0) {
            fill_data_window(msg1->buf, msg1->len);
            printf("Read 0x32\n");
        } else if (msg1->len == 1 && reg < sizeof(reg_file)) {
            msg1->buf[0] = reg_file[reg];
        } else {
            memset(msg1->buf, 0, msg1->len);
        }
    }

    return 0;
}

static const struct i2c_emul_api adxl345_emul_api_i2c = {
    .transfer = adxl345_emul_transfer_i2c,
};

static int dummy_device_init(const struct device *dev)
{
    ARG_UNUSED(dev);
    memset(reg_file, 0, sizeof(reg_file));
    reg_file[ADXL345_REG_BW_RATE] = 0x0A;
    reg_file[ADXL345_REG_DEVID] = 0xE5;
    prng = 42;
    sample_time_us = 0;
    return 0;
}

DEVICE_DT_DEFINE(DT_NODELABEL(adxl345), dummy_device_init, NULL, NULL, NULL, POST_KERNEL, 99, NULL);

static int adxl345_emul_init(const struct emul *target, const struct device *parent)
{
    ARG_UNUSED(target);
    ARG_UNUSED(parent);
    return 0;
}

#define ADXL345_EMUL_DEFINE(inst) \
    EMUL_DT_DEFINE(DT_NODELABEL(adxl345), adxl345_emul_init, NULL, NULL, &adxl345_emul_api_i2c, NULL)

ADXL345_EMUL_DEFINE(0);