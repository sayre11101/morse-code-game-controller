#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/i2c_emul.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

// --- Global State Variables ---
static uint8_t reg_ofsx = 0;
static uint8_t reg_ofsy = 0;
static uint8_t reg_ofsz = 0;
static uint8_t reg_bw_rate = 0;
static uint8_t reg_data_format = 0;
static uint8_t reg_fifo_ctl = 0;
static uint8_t reg_power_ctl = 0;
static uint8_t reg_thresh_act = 0;
static uint8_t reg_int_enable = 0;

static float true_x = 0.0f, true_y = 0.0f, true_z = 0.0f;
static uint8_t dummy_data[6] = {0};

// --- JSON State Exporter ---
// This silently updates the gradebook file every time the agent writes to the bus
static void write_state_json() {
    FILE *fp = fopen("state.json", "w");
    if (fp) {
        fprintf(fp, "{\n");
        fprintf(fp, "  \"OFSX\": %d,\n", reg_ofsx);
        fprintf(fp, "  \"OFSY\": %d,\n", reg_ofsy);
        fprintf(fp, "  \"OFSZ\": %d,\n", reg_ofsz);
        fprintf(fp, "  \"BW_RATE\": %d,\n", reg_bw_rate);
        fprintf(fp, "  \"DATA_FORMAT\": %d,\n", reg_data_format);
        fprintf(fp, "  \"POWER_CTL\": %d,\n", reg_power_ctl);
        fprintf(fp, "  \"THRESH_ACT\": %d,\n", reg_thresh_act);
        fprintf(fp, "  \"INT_ENABLE\": %d,\n", reg_int_enable);
        fprintf(fp, "  \"FIFO_CTL\": %d,\n", reg_fifo_ctl);
        fprintf(fp, "  \"EXPECTED_X\": %.4f,\n", (double)true_x);
        fprintf(fp, "  \"EXPECTED_Y\": %.4f,\n", (double)true_y);
        fprintf(fp, "  \"EXPECTED_Z\": %.4f\n", (double)true_z);
        fprintf(fp, "}\n");
        fclose(fp);
    }
}

// --- Randomization Engine ---
// Generates the secret target answers to prevent hardcoding
static void generate_random_sensor_data() {
    srand(time(NULL)); 
    
    // Generate random raw readings between -500 and +500
    int16_t rand_x = (rand() % 1001) - 500;
    int16_t rand_y = (rand() % 1001) - 500;
    int16_t rand_z = (rand() % 1001) - 500;

    // Pack into the 6-byte little-endian array for the agent to read
    dummy_data[0] = rand_x & 0xFF;
    dummy_data[1] = (rand_x >> 8) & 0xFF;
    dummy_data[2] = rand_y & 0xFF;
    dummy_data[3] = (rand_y >> 8) & 0xFF;
    dummy_data[4] = rand_z & 0xFF;
    dummy_data[5] = (rand_z >> 8) & 0xFF;

    // Calculate the exact floats the agent SHOULD output
    true_x = rand_x * 0.00390625f;
    true_y = rand_y * 0.00390625f;
    true_z = rand_z * 0.00390625f;
    
    // Initialize the file immediately upon boot
    write_state_json();
}

// --- Emulator Interceptor ---
static int adxl345_emul_transfer_i2c(const struct emul *target, struct i2c_msg *msgs, int num_msgs, int addr) {
    if (num_msgs < 1) return 0;
    
    struct i2c_msg *msg0 = &msgs[0];
    uint8_t reg = msg0->buf[0];

    // Catch I2C Writes (Agent is configuring registers)
    if (num_msgs == 1 && (msg0->flags & I2C_MSG_READ) == 0) {
        if (msg0->len == 2) {
            uint8_t val = msg0->buf[1];
            switch(reg) {
                case 0x1E: reg_ofsx = val; break;
                case 0x1F: reg_ofsy = val; break;
                case 0x20: reg_ofsz = val; break;
                case 0x2C: reg_bw_rate = val; break;
                case 0x31: reg_data_format = val; break;
                case 0x38: reg_fifo_ctl = val; break;
                case 0x2D: reg_power_ctl = val; break;
                case 0x24: reg_thresh_act = val; break;
                case 0x2E: reg_int_enable = val; break;
            }
            write_state_json(); // Refresh the gradebook
        }
    } 
    // Catch I2C Reads (Agent is polling or burst reading)
    else if (num_msgs == 2 && (msgs[0].flags & I2C_MSG_READ) == 0 && (msgs[1].flags & I2C_MSG_READ) == I2C_MSG_READ) {
        struct i2c_msg *msg1 = &msgs[1];
        
        if (reg == 0x39 && msg1->len == 1) { 
            // Agent is polling FIFO_STATUS -> Hand them the Trigger Bit (0x80)
            msg1->buf[0] = 0x80; 
        } 
        else if (reg == 0x32 && msg1->len == 6) { 
            // Agent is executing the burst read -> Hand them the randomized little-endian array
            for (int i = 0; i < 6; i++) {
                msg1->buf[i] = dummy_data[i];
            }
        }
    }
    return 0;
}

static const struct i2c_emul_api adxl345_emul_api_i2c = {
    .transfer = adxl345_emul_transfer_i2c,
};

// --- Dummy Device Creation (The Linker Fix & Boot Trigger) ---
static int dummy_device_init(const struct device *dev) {
    // Fire the randomizer the exact millisecond the simulated hardware boots
    generate_random_sensor_data(); 
    return 0;
}

DEVICE_DT_DEFINE(DT_NODELABEL(adxl345), dummy_device_init, 
                 NULL, NULL, NULL, 
                 POST_KERNEL, 99, NULL);
                 
// --- Emulator Registration ---
// A required dummy init function to satisfy Zephyr 3.7.0's emulator API
static int adxl345_emul_init(const struct emul *target, const struct device *parent) {
    return 0;
}

#define ADXL345_EMUL_DEFINE(inst) \
    EMUL_DT_DEFINE(DT_NODELABEL(adxl345), adxl345_emul_init, NULL, NULL, &adxl345_emul_api_i2c, NULL)

ADXL345_EMUL_DEFINE(0);