#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>

#include "mock_sensor.h"

// Morse Code dictionary for A-Z
static const char *morse_dict[26] = {
    ".-",   "-...", "-.-.", "-..",  ".",    // A-E
    "..-.", "--.",  "....", "..",   ".---", // F-J
    "-.-",  ".-..", "--",   "-.",   "---",  // K-O
    ".--.", "--.-", ".-.",  "...",  "-",    // P-T
    "..-",  "...-", ".--",  "-..-", "-.--", // U-Y
    "--.."                                  // Z
};

// Words to select from based on secret_word.txt
static const char *word_bank[] = {
    "SOS", "RADIO", "WAVE", "MORSE", "CODE", "PULSE", "SIGNAL", "LIGHT"
};

// Config for timing
static int64_t DOT_US;
static int64_t DASH_US;
static int64_t SYMBOL_GAP_US;
static int64_t LETTER_GAP_US;
static int64_t END_HOLD_US = 5000000; // 5 seconds hold at end
static int64_t SAMPLE_PERIOD_US = 100; // 100 microseconds (10kHz rate)

// Config for physics (Z axis)
// Gravity is downward (negative Z). Up is positive Z.
static double BASE_G = -1.0; 
static double TRANSIT_TIME_US;
// Stop bounds
static double STOP_DOWN_US = 100; // 0.1ms stop DOWN -> high upward accel spike
static double STOP_UP_US = 200;   // 0.2ms stop UP -> moderate downward accel spike

static double down_spike_g = 510.0;
static double up_spike_g = 255.0;

enum key_state {
    KS_UP_REST = 0,
    KS_TRAVEL_DOWN,
    KS_DOWN_STOP,
    KS_DOWN_REST,
    KS_TRAVEL_UP,
    KS_UP_STOP,
    KS_END // Final wait
};

struct segment {
    enum key_state state;
    int64_t end_us;
};

#define MAX_SEGMENTS 1000
static struct segment timeline[MAX_SEGMENTS];
static int num_segments = 0;

static int64_t current_time_us = 0;
static int64_t current_sample = 0;
static bool initialized = false;

static void add_segment(enum key_state state, int64_t duration_us) {
    if (num_segments >= MAX_SEGMENTS) return;
    int64_t start_us = (num_segments == 0) ? 0 : timeline[num_segments-1].end_us;
    timeline[num_segments].state = state;
    timeline[num_segments].end_us = start_us + duration_us;
    num_segments++;
}

static void build_timeline() {
    // Randomize unit dot time between 100ms and 250ms
    DOT_US = 100000 + (rand() % 150001);
    DASH_US = 3 * DOT_US;
    SYMBOL_GAP_US = DOT_US;
    LETTER_GAP_US = 3 * DOT_US;

    // Randomize transit time between 3ms and 6ms
    TRANSIT_TIME_US = 3000 + (rand() % 3001);
    
    // Physics derivations:
    // Gap = 2.0mm = 0.002m
    // Average velocity = 0.002m / (TRANSIT_TIME_US / 1000000.0s)
    double transit_sec = TRANSIT_TIME_US / 1000000.0;
    double avg_vel = 0.002 / transit_sec;
    
    // Deceleration = Delta_V / Delta_t / 9.8 m/s/s
    double stop_down_sec = STOP_DOWN_US / 1000000.0;
    down_spike_g = (avg_vel / stop_down_sec) / 9.8;
    
    double stop_up_sec = STOP_UP_US / 1000000.0;
    up_spike_g = (avg_vel / stop_up_sec) / 9.8;

    int index = 0;
    FILE *f = fopen("/tests/secret_word.txt", "r");
    if (f) {
        if (fscanf(f, "%d", &index) != 1) index = 0;
        fclose(f);
    }
    if (index < 0 || index > 7) index = 0;
    
    const char *word = word_bank[index];
    
    // Start with key UP rest for 1 sec
    add_segment(KS_UP_REST, 1000000);
    
    for (int i = 0; word[i] != '\0'; i++) {
        int char_idx = word[i] - 'A';
        if (char_idx < 0 || char_idx > 25) continue;
        
        const char *symbols = morse_dict[char_idx];
        for (int j = 0; symbols[j] != '\0'; j++) {
            // Press key down
            add_segment(KS_TRAVEL_DOWN, TRANSIT_TIME_US);
            add_segment(KS_DOWN_STOP, STOP_DOWN_US);
            
            // Hold down
            int64_t hold_time = (symbols[j] == '-') ? DASH_US : DOT_US;
            // Subtract transit+stop time from the hold to maintain dot alignment
            add_segment(KS_DOWN_REST, hold_time - TRANSIT_TIME_US - STOP_DOWN_US);
            
            // Release key up
            add_segment(KS_TRAVEL_UP, TRANSIT_TIME_US);
            add_segment(KS_UP_STOP, STOP_UP_US);
            
            // Gap between symbols or letters
            bool is_last_symbol = (symbols[j+1] == '\0');
            int64_t gap_time = is_last_symbol ? LETTER_GAP_US : SYMBOL_GAP_US;
            add_segment(KS_UP_REST, gap_time - TRANSIT_TIME_US - STOP_UP_US);
        }
    }
    
    // Final end hold in UP position for 5 seconds
    add_segment(KS_END, END_HOLD_US + 1000000);
}

static double frand_noise() {
    return ((double)rand() / (double)RAND_MAX) * 0.04 - 0.02; // +/- 0.02g noise
}

bool get_sample_stru(readings_struct_t *readings) {
    if (!initialized) {
        srand(42);
        build_timeline();
        current_time_us = 0;
        current_sample = 0;
        initialized = true;
    }

    if (num_segments == 0) return false;
    
    // Find current state segment
    enum key_state state = KS_END;
    for (int i = 0; i < num_segments; i++) {
        if (current_time_us < timeline[i].end_us) {
            state = timeline[i].state;
            break;
        }
    }
    
    if (current_time_us >= timeline[num_segments-1].end_us) {
        // Timeline over
        return false;
    }

    double z_g = BASE_G;
    
    switch (state) {
        case KS_TRAVEL_DOWN:
            z_g = BASE_G; // Simple travel, negligible g
            break;
        case KS_DOWN_STOP:
            // Massive spike UP (+Z) to stop downward motion
            z_g = BASE_G + down_spike_g;
            break;
        case KS_DOWN_REST:
            z_g = BASE_G;
            break;
        case KS_TRAVEL_UP:
            z_g = BASE_G; 
            break;
        case KS_UP_STOP:
            // Spike DOWN (-Z) to stop upward motion
            z_g = BASE_G - up_spike_g;
            break;
        case KS_UP_REST:
        case KS_END:
            z_g = BASE_G;
            break;
    }

    readings->x_acc = frand_noise();
    readings->y_acc = frand_noise();
    readings->z_acc = z_g + frand_noise();
    readings->sampling_rate_usec = SAMPLE_PERIOD_US;
    readings->sample_number = current_sample;
    
    current_time_us += SAMPLE_PERIOD_US;
    current_sample++;
    
    return true;
}
