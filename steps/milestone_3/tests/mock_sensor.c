#include <zephyr/kernel.h>
#include <zephyr/drivers/i2c.h>
#include <stdio.h>
#include <string.h>

#define ADXL345_REG_FIFO_STATUS 0x39
#define ADXL345_REG_DATAX0      0x32

static int poll_count = 0;

/* * ---------------------------------------------------------
 * MOCKING ZEPHYR I2C WRITE APIs
 * ---------------------------------------------------------
 */

// Intercepts i2c_reg_write_byte and prints it for the Python tests
int i2c_reg_write_byte(const struct device *dev, uint16_t dev_addr,
                       uint8_t reg_addr, uint8_t value) {
    // This exact format "Write 0x2D -> 0x08" matches our forgiving regex
    printf("Write 0x%02X -> 0x%02X\n", reg_addr, value);
    return 0;
}

// Intercepts the Device Tree version of the write API
int i2c_reg_write_byte_dt(const struct i2c_dt_spec *spec,
                          uint8_t reg_addr, uint8_t value) {
    return i2c_reg_write_byte(NULL, 0, reg_addr, value);
}

// Fallback for agents using standard i2c_write or i2c_write_dt
int i2c_write(const struct device *dev, const uint8_t *buf,
              uint32_t num_bytes, uint16_t addr) {
    if (num_bytes == 2) {
        printf("Write 0x%02X -> 0x%02X\n", buf[0], buf[1]);
    }
    return 0;
}

int i2c_write_dt(const struct i2c_dt_spec *spec, const uint8_t *buf, uint32_t num_bytes) {
    return i2c_write(NULL, buf, num_bytes, 0);
}

/* * ---------------------------------------------------------
 * MOCKING ZEPHYR I2C READ APIs
 * ---------------------------------------------------------
 */

// Intercepts register reads (used primarily for polling FIFO_STATUS)
int i2c_reg_read_byte(const struct device *dev, uint16_t dev_addr,
                      uint8_t reg_addr, uint8_t *value) {
    if (reg_addr == ADXL345_REG_FIFO_STATUS) {
        printf("Read 0x39 (FIFO polled)\n");
        poll_count++;
        
        // Simulate waiting for a few loops, then triggering the watermark
        if (poll_count > 2) {
            *value = 0x8C; // Trigger bit set (0x80) + 12 samples (0x0C)
            poll_count = 0; // Reset for the next batch
        } else {
            *value = 0x00; // FIFO empty
        }
    } else {
        *value = 0x00;
    }
    return 0;
}

int i2c_reg_read_byte_dt(const struct i2c_dt_spec *spec,
                         uint8_t reg_addr, uint8_t *value) {
    return i2c_reg_read_byte(NULL, 0, reg_addr, value);
}

// Intercepts burst reads (used for extracting the 6-byte data samples)
int i2c_burst_read(const struct device *dev, uint16_t dev_addr,
                   uint8_t start_addr, uint8_t *buf, uint32_t num_bytes) {
    if (start_addr == ADXL345_REG_DATAX0 && num_bytes > 0) {
        // Zero out the buffer by default
        memset(buf, 0, num_bytes);
        
        /* * Injecting data for the Python Math Test:
         * Expected Output: X = 0.99g, Y = -0.50g, Z = 1.99g
         * At 4mg/LSB resolution:
         * X = 0.99 / 0.004 = 247.5 (~248) -> 0x00F8
         * Y = -0.50 / 0.004 = -125        -> 0xFF83 (Two's complement)
         * Z = 1.99 / 0.004 = 497.5 (~498) -> 0x01F2
         */
        
        // If they read 12 samples (72 bytes), inject into the 12th sample (bytes 66-71)
        if (num_bytes >= 72) {
            buf[66] = 0xF8; buf[67] = 0x00; // X Axis
            buf[68] = 0x83; buf[69] = 0xFF; // Y Axis
            buf[70] = 0xF2; buf[71] = 0x01; // Z Axis
        } 
        // If they read 8 samples (48 bytes) for Milestone 3, inject into the 8th sample (bytes 42-47)
        else if (num_bytes >= 48) {
            buf[42] = 0xF8; buf[43] = 0x00;
            buf[44] = 0x83; buf[45] = 0xFF;
            buf[46] = 0xF2; buf[47] = 0x01;
        }
    }
    return 0;
}

int i2c_burst_read_dt(const struct i2c_dt_spec *spec,
                      uint8_t start_addr, uint8_t *buf, uint32_t num_bytes) {
    return i2c_burst_read(NULL, 0, start_addr, buf, num_bytes);
}

// Fallback for agents using i2c_write_read_dt (commonly used for register reads)
int i2c_write_read_dt(const struct i2c_dt_spec *spec, 
                      const void *write_buf, size_t num_write, 
                      void *read_buf, size_t num_read) {
    if (num_write == 1) {
        uint8_t reg = ((uint8_t*)write_buf)[0];
        if (num_read == 1) {
            return i2c_reg_read_byte(NULL, 0, reg, (uint8_t*)read_buf);
        } else if (num_read > 1) {
            return i2c_burst_read(NULL, 0, reg, (uint8_t*)read_buf, num_read);
        }
    }
    return 0;
}