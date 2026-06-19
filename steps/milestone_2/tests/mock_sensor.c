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

static uint8_t reg_power_ctl = 0;
static uint8_t reg_thresh_act = 0;
static uint8_t reg_act_inact = 0;
static uint8_t reg_fifo_ctl = 0;
static int current_stage = 1;
static int fifo_read_index = 0;
static int fifo_poll_count = 0;
static int active_fifo_size = 12;
static uint64_t trigger_ready_cycle = 0;
static bool require_trigger_settle = false;

static uint8_t current_fifo[12][6] = {0};

#define FINAL_RAW_X 400
#define FINAL_RAW_Y -188
#define FINAL_RAW_Z 300

static void generate_batch(int size, float target_g)
{
    ARG_UNUSED(target_g);

    fifo_read_index = 0;
    fifo_poll_count = 0;
    active_fifo_size = size;
    trigger_ready_cycle = 0;
    require_trigger_settle = false;

    for (int i = 0; i < size; i++) {
        int16_t raw_x;
        int16_t raw_y;
        int16_t raw_z;

        if (i < size - 1) {
            raw_x = (rand() % 25) - 12;
            raw_y = (rand() % 25) - 12;
            raw_z = (rand() % 25) - 12;
        } else {
            raw_x = FINAL_RAW_X;
            raw_y = FINAL_RAW_Y;
            raw_z = FINAL_RAW_Z;
        }

        current_fifo[i][0] = raw_x & 0xFF;
        current_fifo[i][1] = (raw_x >> 8) & 0xFF;
        current_fifo[i][2] = raw_y & 0xFF;
        current_fifo[i][3] = (raw_y >> 8) & 0xFF;
        current_fifo[i][4] = raw_z & 0xFF;
        current_fifo[i][5] = (raw_z >> 8) & 0xFF;
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
        printf("Write 0x%02X -> 0x%02X\n", reg, val);

        switch (reg) {
        case 0x2D:
            reg_power_ctl = val;
            break;
        case 0x24:
            reg_thresh_act = val;
            break;
        case ADXL345_REG_ACT_INACT_CTL:
            reg_act_inact = val;
            break;
        case 0x38:
            reg_fifo_ctl = val;
            break;
        default:
            break;
        }

        if (current_stage == 2 && reg_power_ctl == 0x08 && reg_thresh_act == 0x13 && reg_fifo_ctl == 0xC8) {
            current_stage = 3;
            generate_batch(8, 1.3f);
        }
    } else if (num_msgs == 2 && (msgs[0].flags & I2C_MSG_READ) == 0 && (msgs[1].flags & I2C_MSG_READ) == I2C_MSG_READ) {
        struct i2c_msg *msg1 = &msgs[1];

        if (reg == 0x00 && msg1->len == 1) {
            msg1->buf[0] = 0xE5;
        } else if (reg == 0x39 && msg1->len == 1) {
            printf("Read 0x39 (FIFO polled)\n");
            msg1->buf[0] = (fifo_poll_count >= 2 && reg_act_inact == 0x40) ? (uint8_t)(0x80 | active_fifo_size) : 0x00;
            if ((msg1->buf[0] & 0x80) != 0) {
                trigger_ready_cycle = k_cycle_get_64();
                require_trigger_settle = true;
            }
            fifo_poll_count++;
        } else if (reg == 0x30 && msg1->len == 1) {
            printf("Read 0x30 (INT_SOURCE cleared)\n");
            msg1->buf[0] = 0x80;
            if (current_stage == 1 && fifo_read_index >= active_fifo_size) {
                current_stage = 2;
                generate_batch(12, 1.6f);
            }
        } else if (reg == 0x32 && msg1->len > 0) {
            if (msg1->len != 6) {
                return -EIO;
            }

            if (require_trigger_settle) {
                uint64_t elapsed_us = k_cyc_to_us_floor64(k_cycle_get_64() - trigger_ready_cycle);
                if (elapsed_us < 5) {
                    return -EIO;
                }
                require_trigger_settle = false;
            }

            memset(msg1->buf, 0, msg1->len);

            if (fifo_read_index < active_fifo_size) {
                memcpy(msg1->buf, current_fifo[fifo_read_index], 6);
                fifo_read_index++;
            }
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
    srand(42);
    generate_batch(12, 1.6f);
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