#include "adxl_regs.h"
#include <math.h>
#include <stdint.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// --- Basic Simulation State ---
static double current_sampling_freq = 122880.0; // Nominal 60Hz * 2048
static double current_phase = 0.0;

// --- Hardware Abstraction API ---

void set_adc_sampling_frequency(double target_hz) {
    // The agent calls this to adjust the ADC sampling rate
    current_sampling_freq = target_hz;
}

void fill_adc_buf(adc_buf_t *buf) {
    uint16_t *buffer = buf->adc_buf;
    
    // In the agent's sandbox, we only simulate a perfectly flat 60Hz carrier.
    // The real FSK physics engine and secret words will be injected during the final eval.
    double dt = 1.0 / current_sampling_freq;
    double nominal_grid_freq = 60.0; 

    for (int i = 0; i < SAMPLES_PER_CYCLE; i++) {
        // Integrate phase continuously to avoid popping/clicking
        current_phase += 2.0 * M_PI * nominal_grid_freq * dt;

        // Keep phase bounded
        if (current_phase > 2.0 * M_PI) {
            current_phase -= 2.0 * M_PI;
        }

        // Generate 12-bit right-justified ADC value (0 to 4095)
        // Matches the 2047 amplitude established in Milestone 1
        buffer[i] = (uint16_t)(2047.0 * sin(current_phase) + 2048.0);
    }
}