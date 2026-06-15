#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/i2c_emul.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <time.h>

// --- Global State Variables ---
static uint8_t reg_power_ctl = 0;
static uint8_t reg_thresh_act = 0;
static uint8_t reg_fifo_ctl = 0;
static uint8_t reg_ofsx = 0;
static uint8_t reg_ofsy = 0;
static uint8_t reg_ofsz = 0;
static uint8_t initial_thresh_act = 0;

// --- Tracked Configuration Registers ---
static uint8_t reg_bw_rate = 0;
static uint8_t reg_data_format = 0;
static uint8_t initial_fifo_ctl = 0;
static bool fifo_ctl_written = false;
static uint8_t act_inact_ctl = 0;
// --------------------------------------------

static int current_stage = 1;
static int fifo_read_index = 0;
static int standby_violation = 0;

static float expected_stage1_x[12];
static float expected_stage1_y[12];
static float expected_stage1_z[12];

static float expected_stage2_x[12];
static float expected_stage2_y[12];
static float expected_stage2_z[12];

static float expected_stage3_x[8];
static float expected_stage3_y[8];
static float expected_stage3_z[8];

static uint8_t current_fifo[12][6] = {0};

// --- JSON State Exporter ---
static void write_state_json()
{
    FILE *fp = fopen("state.json", "w");
    if (fp)
    {
        fprintf(fp, "{\n");
        fprintf(fp, "  \"STAGE\": %d,\n", current_stage);
        fprintf(fp, "  \"STANDBY_VIOLATION\": %d,\n", standby_violation);
        fprintf(fp, "  \"POWER_CTL\": %d,\n", reg_power_ctl);
        
        fprintf(fp, "  \"INITIAL_THRESH_ACT\": %d,\n", initial_thresh_act);
        fprintf(fp, "  \"BW_RATE\": %d,\n", reg_bw_rate);
        fprintf(fp, "  \"DATA_FORMAT\": %d,\n", reg_data_format);
        fprintf(fp, "  \"INITIAL_FIFO_CTL\": %u,\n", initial_fifo_ctl);
        fprintf(fp, "  \"ACT_INACT_CTL\": %u,\n", act_inact_ctl);

        fprintf(fp, "  \"THRESH_ACT\": %d,\n", reg_thresh_act);
        fprintf(fp, "  \"FIFO_CTL\": %d,\n", reg_fifo_ctl);
        fprintf(fp, "  \"OFSX\": %d,\n", reg_ofsx);
        fprintf(fp, "  \"OFSY\": %d,\n", reg_ofsy);
        fprintf(fp, "  \"OFSZ\": %d,\n", reg_ofsz);

        // Stage 1 Dumps
        fprintf(fp, "  \"EXPECTED_STAGE1_X\": [");
        for (int i = 0; i < 12; i++) fprintf(fp, "%.4f%s", (double)expected_stage1_x[i], (i == 11) ? "" : ",");
        fprintf(fp, "],\n  \"EXPECTED_STAGE1_Y\": [");
        for (int i = 0; i < 12; i++) fprintf(fp, "%.4f%s", (double)expected_stage1_y[i], (i == 11) ? "" : ",");
        fprintf(fp, "],\n  \"EXPECTED_STAGE1_Z\": [");
        for (int i = 0; i < 12; i++) fprintf(fp, "%.4f%s", (double)expected_stage1_z[i], (i == 11) ? "" : ",");
        fprintf(fp, "],\n");

        // Stage 2 Dumps
        fprintf(fp, "  \"EXPECTED_STAGE2_X\": [");
        for (int i = 0; i < 12; i++) fprintf(fp, "%.4f%s", (double)expected_stage2_x[i], (i == 11) ? "" : ",");
        fprintf(fp, "],\n  \"EXPECTED_STAGE2_Y\": [");
        for (int i = 0; i < 12; i++) fprintf(fp, "%.4f%s", (double)expected_stage2_y[i], (i == 11) ? "" : ",");
        fprintf(fp, "],\n  \"EXPECTED_STAGE2_Z\": [");
        for (int i = 0; i < 12; i++) fprintf(fp, "%.4f%s", (double)expected_stage2_z[i], (i == 11) ? "" : ",");
        fprintf(fp, "],\n");

        // Stage 3 Dumps
        fprintf(fp, "  \"EXPECTED_STAGE3_X\": [");
        for (int i = 0; i < 8; i++) fprintf(fp, "%.4f%s", (double)expected_stage3_x[i], (i == 7) ? "" : ",");
        fprintf(fp, "],\n  \"EXPECTED_STAGE3_Y\": [");
        for (int i = 0; i < 8; i++) fprintf(fp, "%.4f%s", (double)expected_stage3_y[i], (i == 7) ? "" : ",");
        fprintf(fp, "],\n  \"EXPECTED_STAGE3_Z\": [");
        for (int i = 0; i < 8; i++) fprintf(fp, "%.4f%s", (double)expected_stage3_z[i], (i == 7) ? "" : ",");
        fprintf(fp, "]\n");

        fprintf(fp, "}\n");
        fclose(fp);
    }
}

// --- Realistic Data Generator ---
static void generate_batch(int size, float target_g)
{
    fifo_read_index = 0;

    for (int i = 0; i < size; i++)
    {
        int16_t raw_x, raw_y, raw_z;

        if (i < size - 2)
        {
            raw_x = (rand() % 25) - 12;
            raw_y = (rand() % 25) - 12;
            raw_z = (rand() % 25) - 12;
        }
        else
        {
            int16_t base_spike = (int16_t)(target_g / 0.00390625f);
            raw_x = base_spike + (rand() % 25) - 12;
            raw_y = (rand() % 25) - 12;
            raw_z = (rand() % 25) - 12;
        }

        current_fifo[i][0] = raw_x & 0xFF;
        current_fifo[i][1] = (raw_x >> 8) & 0xFF;
        current_fifo[i][2] = raw_y & 0xFF;
        current_fifo[i][3] = (raw_y >> 8) & 0xFF;
        current_fifo[i][4] = raw_z & 0xFF;
        current_fifo[i][5] = (raw_z >> 8) & 0xFF;

        float final_x = raw_x * 0.00390625f;
        float final_y = raw_y * 0.00390625f;
        float final_z = raw_z * 0.00390625f;

        if (current_stage == 1) {
            expected_stage1_x[i] = final_x;
            expected_stage1_y[i] = final_y;
            expected_stage1_z[i] = final_z;
        } else if (current_stage == 2) {
            expected_stage2_x[i] = final_x;
            expected_stage2_y[i] = final_y;
            expected_stage2_z[i] = final_z;
        } else if (current_stage == 3) {
            expected_stage3_x[i] = final_x;
            expected_stage3_y[i] = final_y;
            expected_stage3_z[i] = final_z;
        }
    }
    write_state_json();
}

// --- Emulator Interceptor ---
static int adxl345_emul_transfer_i2c(const struct emul *target, struct i2c_msg *msgs, int num_msgs, int addr)
{
    if (num_msgs < 1)
        return 0;

    struct i2c_msg *msg0 = &msgs[0];
    uint8_t reg = msg0->buf[0];

    // Catch I2C Writes
    if (num_msgs == 1 && (msg0->flags & I2C_MSG_READ) == 0 && msg0->len == 2)
    {
        uint8_t val = msg0->buf[1];

        printf("[EMULATOR] I2C Write - Register: 0x%02X, Value: 0x%02X\n", reg, val);

        if ((reg == 0x24 || reg == 0x38) && reg_power_ctl == 0x08)
        {
            standby_violation = 1;
        }

        switch (reg)
        {
        case 0x2C:
            reg_bw_rate = val;
            break;
        case 0x31:
            reg_data_format = val;
            break;
        case 0x2D:
            reg_power_ctl = val;
            break;
        case 0x24:
            if (initial_thresh_act == 0)
            {
                initial_thresh_act = val;
            }
            reg_thresh_act = val;
            break;
        case 0x27:
            act_inact_ctl = val;
            break;
        case 0x38:
            if (!fifo_ctl_written) {
                initial_fifo_ctl = val;
                fifo_ctl_written = true;
            }
            reg_fifo_ctl = val;
            break;
        case 0x1E:
            reg_ofsx = val;
            break;
        case 0x1F:
            reg_ofsy = val;
            break;
        case 0x20:
            reg_ofsz = val;
            break;
        }

        if (current_stage == 2 && reg_power_ctl == 0x08 && reg_thresh_act == 0x13 && reg_fifo_ctl == 0xC8)
        {
            current_stage = 3;
            printf("[EMULATOR] Reconfiguration detected. Moving to Stage 3.\n");
            generate_batch(8, 1.30f);
        }
        write_state_json();
    }

    // Catch I2C Reads
    else if (num_msgs == 2 && (msgs[0].flags & I2C_MSG_READ) == 0 && (msgs[1].flags & I2C_MSG_READ) == I2C_MSG_READ)
    {
        struct i2c_msg *msg1 = &msgs[1];

        if (reg == 0x00 && msg1->len == 1)
        {
            msg1->buf[0] = 0xE5; // Return the expected ADXL345 Device ID
        }

        else if ((reg == 0x39 || reg == 0x30) && msg1->len == 1)
        {
            msg1->buf[0] = 0x80;

            if (reg == 0x30 && current_stage == 1 && fifo_read_index >= 12)
            {
                printf("[EMULATOR] INT_SOURCE Read (Interrupt Cleared). Moving to Stage 2.\n");
                current_stage = 2;
                generate_batch(12, 1.60f);
            }
        }
        else if (reg == 0x32 && msg1->len == 6)
        {
            int max_size = (current_stage == 3) ? 8 : 12;
            if (fifo_read_index < max_size)
            {
                for (int i = 0; i < 6; i++)
                {
                    msg1->buf[i] = current_fifo[fifo_read_index][i];
                }
                fifo_read_index++;
            }
            else
            {
                for (int i = 0; i < 6; i++)
                    msg1->buf[i] = 0;
            }
        }
    }
    return 0;
}

static const struct i2c_emul_api adxl345_emul_api_i2c = {.transfer = adxl345_emul_transfer_i2c};

static int dummy_device_init(const struct device *dev)
{
    srand(time(NULL));
    generate_batch(12, 1.60f);
    return 0;
}

DEVICE_DT_DEFINE(DT_NODELABEL(adxl345), dummy_device_init, NULL, NULL, NULL, POST_KERNEL, 99, NULL);

static int adxl345_emul_init(const struct emul *target, const struct device *parent) { return 0; }

#define ADXL345_EMUL_DEFINE(inst) EMUL_DT_DEFINE(DT_NODELABEL(adxl345), adxl345_emul_init, NULL, NULL, &adxl345_emul_api_i2c, NULL)
ADXL345_EMUL_DEFINE(0);