#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include "adxl_regs.h"

#define I2C_NODE DT_NODELABEL(i2c0)

/* Polling and classifier timing.  The simulated travel time is 30..200 ms. */
#define POLL_MS                 2
#define START_CONSEC_SAMPLES    2
#define QUIET_US                35000LL
#define MIN_TRAVEL_US           20000LL
#define END_UP_US               5000000LL  // 5 seconds in microseconds

/* Baseline/noise filters used while the key is resting. */
#define BASE_Q                  1024
#define BASE_SHIFT              5       /* 1/32 IIR */
#define NOISE_SHIFT             6       /* 1/64 IIR */
#define MIN_START_THRESHOLD     3       /* raw ADXL345 counts */
#define MIN_STOP_THRESHOLD      1       /* raw ADXL345 counts */

enum key_state {
    STATE_TRAVEL_DOWN = 0,
    STATE_DOWN,
    STATE_TRAVEL_UP,
    STATE_UP,
};

// ... helper functions ...

int main(void)
{
    const struct device *i2c_dev = DEVICE_DT_GET(I2C_NODE);
    if (!device_is_ready(i2c_dev)) {
        return -1;
    }

    adxl345_configure(i2c_dev);

    int16_t x, y, z;
    while (read_accel_raw(i2c_dev, &x, &y, &z) != 0) {
        k_sleep(K_MSEC(POLL_MS));
    }

    int32_t base_x = (int32_t)x * BASE_Q;
    int32_t base_y = (int32_t)y * BASE_Q;
    int32_t base_z = (int32_t)z * BASE_Q;
    int32_t noise = 0;
    int16_t prev_x = x, prev_y = y, prev_z = z;

    enum key_state state = STATE_UP;
    int64_t state_start_us = now_us();
    int64_t last_motion_us = state_start_us;
    int64_t motion_candidate_us = 0;
    int start_consec = 0;

    for (;;) {  // <-- INFINITE LOOP
        k_sleep(K_MSEC(POLL_MS));
        
        if (read_accel_raw(i2c_dev, &x, &y, &z) != 0) {
            continue;
        }

        int64_t t_us = now_us();
        int32_t sx = (int32_t)x * BASE_Q;
        int32_t sy = (int32_t)y * BASE_Q;
        int32_t sz = (int32_t)z * BASE_Q;

        int32_t score = (abs_i32(sx - base_x) +
                         abs_i32(sy - base_y) +
                         abs_i32(sz - base_z)) / BASE_Q;

        int32_t start_threshold = noise * 8 + MIN_START_THRESHOLD;
        if (start_threshold < MIN_START_THRESHOLD) {
            start_threshold = MIN_START_THRESHOLD;
        }
        int32_t stop_threshold = start_threshold / 3;
        if (stop_threshold < MIN_STOP_THRESHOLD) {
            stop_threshold = MIN_STOP_THRESHOLD;
        }

        if (state == STATE_UP || state == STATE_DOWN) {
            if (score >= start_threshold) {  // Detect motion onset
                if (start_consec == 0) {
                    motion_candidate_us = t_us;
                }
                start_consec++;

                if (start_consec >= START_CONSEC_SAMPLES) {
                    enum key_state next = (state == STATE_UP) ?
                                          STATE_TRAVEL_DOWN : STATE_TRAVEL_UP;
                    int64_t change_us = motion_candidate_us;

                    print_state_duration(state, state_start_us, change_us);
                    state = next;
                    state_start_us = change_us;
                    last_motion_us = t_us;
                    start_consec = 0;
                }
            } else {
                start_consec = 0;

                /* While resting, slowly track static offset/gravity and noise. */
                base_x += (sx - base_x) >> BASE_SHIFT;
                base_y += (sy - base_y) >> BASE_SHIFT;
                base_z += (sz - base_z) >> BASE_SHIFT;
                noise += (score - noise) >> NOISE_SHIFT;
                if (noise < 0) {
                    noise = 0;
                }

                // EXIT CONDITION (where it times out):
                if (state == STATE_UP && (t_us - state_start_us) >= END_UP_US) {
                    int64_t end_us = state_start_us + END_UP_US;
                    print_state_duration(STATE_UP, state_start_us, end_us);
                    printf("END\n");
                    fflush(stdout);
                    exit(0);  // <-- Should reach here but doesn't
                }
            }
        } else { /* travelling */
            if (score >= stop_threshold) {
                last_motion_us = t_us;
            }

            if ((t_us - last_motion_us) >= QUIET_US &&
                (t_us - state_start_us) >= MIN_TRAVEL_US) {
                enum key_state next = (state == STATE_TRAVEL_DOWN) ? STATE_DOWN : STATE_UP;
                int64_t change_us = last_motion_us;

                print_state_duration(state, state_start_us, change_us);
                state = next;
                state_start_us = change_us;
                start_consec = 0;

                /* Re-anchor the rest baseline at the stop just reached. */
                base_x = sx;
                base_y = sy;
                base_z = sz;
                noise = 0;
            }
        }

        prev_x = x;
        prev_y = y;
        prev_z = z;
    }  // <-- Never exits this loop
}