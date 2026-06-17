#ifndef ADC_BUF_H
#define ADC_BUF_H

#include <stdint.h>

#define SAMPLES_PER_CYCLE 2048
#define NOMINAL_LINE_FREQUENCY 60.0
#define NOMINAL_BAUD_RATE 1.0
#define MAX_MESSAGE_LENGTH 16
// 60 frames = 1 simulated second.
// 12000 frames = 200 simulated seconds - approx 200 bits
#define NUMBER_OF_ADC_FRAMES 12000

typedef struct {
  uint16_t adc_buf[SAMPLES_PER_CYCLE];
} adc_buf_t;

// --- Hardware Abstraction API ---
void set_adc_sampling_frequency(double sample_frequency);
void fill_adc_buf(adc_buf_t *buf);

#endif // ADC_BUF_H