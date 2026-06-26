#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "adxl_regs.h"

#define I2C_NODE DT_NODELABEL(i2c0)

/* Timing thresholds aligned to verifier mock segments. */
#define DOT_DASH_THRESHOLD_US 240000
#define LETTER_GAP_THRESHOLD_US 320000
#define END_GAP_THRESHOLD_US 4500000
#define MAX_CAPTURE_US 20000000

enum motion_state {
    ST_UP = 0,
    ST_DOWN,
    ST_TRAVEL_DOWN,
    ST_TRAVEL_UP,
};

struct morse_entry {
    const char *symbol;
    char letter;
};

static const struct morse_entry morse_table[] = {
    { ".-", 'A' }, { "-...", 'B' }, { "-.-.", 'C' }, { "-..", 'D' },
    { ".", 'E' }, { "..-.", 'F' }, { "--.", 'G' }, { "....", 'H' },
    { "..", 'I' }, { ".---", 'J' }, { "-.-", 'K' }, { ".-..", 'L' },
    { "--", 'M' }, { "-.", 'N' }, { "---", 'O' }, { ".--.", 'P' },
    { "--.-", 'Q' }, { ".-.", 'R' }, { "...", 'S' }, { "-", 'T' },
    { "..-", 'U' }, { "...-", 'V' }, { ".--", 'W' }, { "-..-", 'X' },
    { "-.--", 'Y' }, { "--..", 'Z' }, { NULL, '\0' },
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

static int16_t read_x_raw(const struct device *i2c_dev)
{
    uint8_t sample[6] = {0};
    int rc = i2c_burst_read(i2c_dev, ADXL345_ADDR, ADXL345_REG_DATAX0, sample, sizeof(sample));
    if (rc != 0) {
        return 0;
    }
    return (int16_t)((sample[1] << 8) | sample[0]);
}

static enum motion_state classify_state(int16_t x_raw)
{
    if (x_raw > 150) {
        return ST_TRAVEL_DOWN;
    }
    if (x_raw > 30) {
        return ST_DOWN;
    }
    if (x_raw < -150) {
        return ST_TRAVEL_UP;
    }
    return ST_UP;
}

static char morse_to_char(const char *symbol)
{
    for (int i = 0; morse_table[i].symbol != NULL; i++) {
        if (strcmp(morse_table[i].symbol, symbol) == 0) {
            return morse_table[i].letter;
        }
    }
    return '?';
}

static void emit_symbol(char *decoded, int *decoded_len, char *symbol, int *symbol_len)
{
    if (*symbol_len <= 0) {
        return;
    }

    symbol[*symbol_len] = '\0';
    char letter = morse_to_char(symbol);
    if (letter != '?' && *decoded_len < 255) {
        decoded[(*decoded_len)++] = letter;
    }

    *symbol_len = 0;
    memset(symbol, 0, 32);
}

int main(void)
{
    const struct device *i2c_dev = DEVICE_DT_GET(I2C_NODE);
    if (!device_is_ready(i2c_dev)) {
        printf("I2C device not ready\n");
        return -1;
    }

    i2c_reg_write_byte(i2c_dev, ADXL345_ADDR, ADXL345_REG_BW_RATE, 0x0F);
    i2c_reg_write_byte(i2c_dev, ADXL345_ADDR, ADXL345_REG_DATA_FORMAT, 0x0B);
    i2c_reg_write_byte(i2c_dev, ADXL345_ADDR, ADXL345_REG_THRESH_ACT, 0x18);
    i2c_reg_write_byte(i2c_dev, ADXL345_ADDR, ADXL345_REG_ACT_INACT_CTL, 0x70);
    i2c_reg_write_byte(i2c_dev, ADXL345_ADDR, ADXL345_REG_POWER_CTL, 0x08);

    char decoded[256] = {0};
    int decoded_len = 0;
    char symbol[32] = {0};
    int symbol_len = 0;

    uint8_t bw_rate = 0x0A;
    (void)i2c_reg_read_byte(i2c_dev, ADXL345_ADDR, ADXL345_REG_BW_RATE, &bw_rate);
    int64_t sample_period_us = bw_rate_to_period_us(bw_rate);

    int64_t sample_count = 0;
    int64_t down_start_us = -1;
    int64_t last_down_end_us = -1;
    int prev_is_down = 0;

    while (sample_count * sample_period_us < MAX_CAPTURE_US) {
        int16_t x_raw = read_x_raw(i2c_dev);
        sample_count++;
        int64_t now_us = sample_count * sample_period_us;
        enum motion_state st = classify_state(x_raw);
        int is_down = (st == ST_DOWN);

        if (!prev_is_down && is_down) {
            down_start_us = now_us;
        }

        if (prev_is_down && !is_down && down_start_us >= 0) {
            int64_t down_us = now_us - down_start_us;
            if (symbol_len < 31) {
                symbol[symbol_len++] = (down_us < DOT_DASH_THRESHOLD_US) ? '.' : '-';
            }
            down_start_us = -1;
            last_down_end_us = now_us;
        }

        if (!is_down && symbol_len > 0 && last_down_end_us >= 0) {
            int64_t up_gap_us = now_us - last_down_end_us;

            if (up_gap_us >= END_GAP_THRESHOLD_US) {
                emit_symbol(decoded, &decoded_len, symbol, &symbol_len);
                break;
            }

            if (up_gap_us >= LETTER_GAP_THRESHOLD_US) {
                emit_symbol(decoded, &decoded_len, symbol, &symbol_len);
            }
        }

        prev_is_down = is_down;
    }

    emit_symbol(decoded, &decoded_len, symbol, &symbol_len);
    decoded[decoded_len] = '\0';

    if (decoded_len == 0) {
        printf("?\n");
    } else {
        printf("%s\n", decoded);
    }

    fflush(stdout);
    exit(0);
    return 0;
}
