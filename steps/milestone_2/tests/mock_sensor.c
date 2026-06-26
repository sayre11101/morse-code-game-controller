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
#include <ctype.h>
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

#define MAX_SEGMENTS 1024
static struct segment timeline[MAX_SEGMENTS];
static size_t timeline_count;

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

static const char *morse_for_char(char c)
{
    switch ((char)toupper((unsigned char)c)) {
    case 'A': return ".-";
    case 'B': return "-...";
    case 'C': return "-.-.";
    case 'D': return "-..";
    case 'E': return ".";
    case 'F': return "..-.";
    case 'G': return "--.";
    case 'H': return "....";
    case 'I': return "..";
    case 'J': return ".---";
    case 'K': return "-.-";
    case 'L': return ".-..";
    case 'M': return "--";
    case 'N': return "-.";
    case 'O': return "---";
    case 'P': return ".--.";
    case 'Q': return "--.-";
    case 'R': return ".-.";
    case 'S': return "...";
    case 'T': return "-";
    case 'U': return "..-";
    case 'V': return "...-";
    case 'W': return ".--";
    case 'X': return "-..-";
    case 'Y': return "-.--";
    case 'Z': return "--..";
    default: return NULL;
    }
}

static void append_segment(enum key_state state, int64_t duration_ms)
{
    if (timeline_count >= MAX_SEGMENTS || duration_ms <= 0) {
        return;
    }

    int64_t duration_us = duration_ms * 1000;
    int64_t start = (timeline_count == 0) ? 0 : timeline[timeline_count - 1].end_us;
    timeline[timeline_count].state = state;
    timeline[timeline_count].end_us = start + duration_us;
    timeline_count++;
}

static void build_timeline_from_word(const char *word)
{
    timeline_count = 0;

    /* Initial quiet window for baseline/noise adaptation. */
    append_segment(KS_UP, 3000);

    size_t len = strlen(word);
    for (size_t i = 0; i < len; i++) {
        const char *code = morse_for_char(word[i]);
        if (code == NULL) {
            continue;
        }

        size_t n = strlen(code);
        for (size_t j = 0; j < n; j++) {
            bool is_dot = (code[j] == '.');

            append_segment(KS_TRAVEL_DOWN, 80);
            append_segment(KS_DOWN, is_dot ? 120 : 360);
            append_segment(KS_TRAVEL_UP, 80);

            if (j + 1 < n) {
                /* Intra-letter symbol gap. */
                append_segment(KS_UP, 120);
            }
        }

        if (i + 1 < len) {
            /* Inter-letter gap. */
            append_segment(KS_UP, 360);
        }
    }

    /* Final hold to let decoder flush output and exit cleanly. */
    append_segment(KS_UP, 5000);
}

static enum key_state current_state(int64_t now_us)
{
    for (size_t i = 0; i < timeline_count; i++) {
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
        *raw_x = 220 + noise_x;
        *raw_y = noise_y;
        *raw_z = 240 + noise_z;
        break;
    case KS_DOWN:
        *raw_x = 90 + noise_x;
        *raw_y = noise_y;
        *raw_z = 250 + noise_z;
        break;
    case KS_TRAVEL_UP:
        *raw_x = -220 + noise_x;
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

    for (size_t off = 0; off + 5 < len; off += 6) {
        enum key_state st = current_state(sample_time_us);
        synth_sample(st, &x, &y, &z);
        encode_xyz(&buf[off], x, y, z);
        sample_time_us += period_us;
    }

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
        if (reg < sizeof(reg_file)) {
            reg_file[reg] = val;
        }
        printf("Write 0x%02X -> 0x%02X\n", reg, val);
    } else if (num_msgs == 2 && (msgs[0].flags & I2C_MSG_READ) == 0 && (msgs[1].flags & I2C_MSG_READ) == I2C_MSG_READ) {
        struct i2c_msg *msg1 = &msgs[1];

        if (reg == 0x00 && msg1->len == 1) {
            msg1->buf[0] = 0xE5;
        } else if (reg == 0x39 && msg1->len == 1) {
            printf("Read 0x39 (FIFO polled)\n");
            msg1->buf[0] = 0x8C;
        } else if (reg == 0x30 && msg1->len == 1) {
            printf("Read 0x30 (INT_SOURCE cleared)\n");
            enum key_state st = current_state(sample_time_us);
            msg1->buf[0] = (st == KS_TRAVEL_DOWN || st == KS_TRAVEL_UP) ? 0x10 : 0x00;
        } else if (reg == 0x32 && msg1->len > 0) {
            if (msg1->len != 6) {
                return -EIO;
            }
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

    const char *target_word = getenv("MORSE_TARGET_WORD");
    if (target_word == NULL || target_word[0] == '\0') {
        target_word = "SOS";
    }
    build_timeline_from_word(target_word);

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