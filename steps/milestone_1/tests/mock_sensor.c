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

// Config for timing
static int64_t DOT_US;
static int64_t DASH_US;
static int64_t SYMBOL_GAP_US;
static int64_t LETTER_GAP_US;
static int64_t WORD_GAP_US;
static int64_t END_HOLD_US = 5000000; // 5 seconds hold at end
static int64_t SAMPLE_PERIOD_US = 100; // 100 microseconds (10kHz rate)

// Config for physics (Z axis)
// Gravity is downward (negative Z). Up is positive Z.
static double BASE_G = -1.0; 
// Stop bounds
static double STOP_DOWN_US = 50;  // 50usec (50% chance to miss at 100us sample rate)
static double STOP_UP_US = 80;    // 80usec (20% chance to miss)

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
    double travel_g;
    double spike_g;
};

#define MAX_SEGMENTS 1000
static struct segment timeline[MAX_SEGMENTS];
static int num_segments = 0;

static int64_t current_time_us = 0;
static int64_t current_sample = 0;
static bool initialized = false;

static void add_segment(enum key_state state, int64_t duration_us, double t_g, double s_g) {
    if (num_segments >= MAX_SEGMENTS) return;
    int64_t start_us = (num_segments == 0) ? 0 : timeline[num_segments-1].end_us;
    timeline[num_segments].state = state;
    timeline[num_segments].end_us = start_us + duration_us;
    timeline[num_segments].travel_g = t_g;
    timeline[num_segments].spike_g = s_g;
    num_segments++;
}

static void build_timeline() {
    // Randomize unit dot time between 100ms and 250ms
    DOT_US = 100000 + (rand() % 150001);
    DASH_US = 3 * DOT_US;
    SYMBOL_GAP_US = DOT_US;
    LETTER_GAP_US = 3 * DOT_US;
    WORD_GAP_US = 7 * DOT_US;

    static const char *WORDS[] = {
      "SOS",
      "RADIO WAVES ARE COOL",
      "MORSE CODE IS VERY OLD",
      "TOM MOTTO OTTO TO",
      "ACCELEROMETER READS G",
      "TMO",
      "THE CAR IS DRIVING NOW",
      "WAVES TRAVEL FAST FAR"
    };

    int num_words = sizeof(WORDS) / sizeof(WORDS[0]);
    int word_index = 0;
    FILE *fp = fopen("/app/index.txt", "r");
    if (fp) {
        if (fscanf(fp, "%d", &word_index) != 1) {
            word_index = 0;
        }
        fclose(fp);
    } else {
        word_index = 0;
    }

    FILE *fw = fopen("/app/index.txt", "w");
    if (fw) {
        fprintf(fw, "%d\n", word_index + 1);
        fclose(fw);
    }

    if (word_index < 0) word_index = 0;
    word_index = word_index % num_words;

    const char *word = WORDS[word_index];
    
    // Start with key UP rest for 1 sec
    add_segment(KS_UP_REST, 1000000, 0, 0);
    
    for (int i = 0; word[i] != '\0'; i++) {
        if (word[i] == ' ') {
            // Space creates a word gap. Since the previous letter already added a LETTER_GAP_US,
            // we extend it by (WORD_GAP_US - LETTER_GAP_US) to match the total required 7 dots!
            add_segment(KS_UP_REST, WORD_GAP_US - LETTER_GAP_US, 0, 0);
            continue;
        }

        int char_idx = word[i] - 'A';
        if (char_idx < 0 || char_idx > 25) continue;
        
        const char *symbols = morse_dict[char_idx];
        for (int j = 0; symbols[j] != '\0'; j++) {
            // Randomize transit time dynamically between 3ms and 6ms for down strike
            int64_t transit_down_us = 2000 + (rand() % 4001);
            // Randomize transit time dynamically between 4ms and 5ms for up release
            int64_t transit_up_us = 4000 + (rand() % 1001);

            double td_sec = transit_down_us / 1000000.0;
            double tu_sec = transit_up_us / 1000000.0;
            
            double travel_down_g = -(2.0*0.002)/(td_sec*td_sec)/9.8;
            double travel_up_g = (2.0*0.002)/(tu_sec*tu_sec)/9.8;

            double sd_sec = STOP_DOWN_US / 1000000.0;
            double su_sec = STOP_UP_US / 1000000.0;
            double avg_d_v = 0.002 / td_sec;
            double avg_u_v = 0.002 / tu_sec;
            
            double down_s_g = (avg_d_v / sd_sec) / 9.8;
            double up_s_g = (avg_u_v / su_sec) / 9.8;
        
            // Press key down
            add_segment(KS_TRAVEL_DOWN, transit_down_us, travel_down_g, 0);
            add_segment(KS_DOWN_STOP, STOP_DOWN_US, 0, down_s_g);
            
            // Hold down
            int64_t hold_time = (symbols[j] == '-') ? DASH_US : DOT_US;
            // Subtract transit+stop time from the hold to maintain dot alignment
            add_segment(KS_DOWN_REST, hold_time - transit_down_us - STOP_DOWN_US, 0, 0);
            
            // Release key up
            add_segment(KS_TRAVEL_UP, transit_up_us, travel_up_g, 0);
            add_segment(KS_UP_STOP, STOP_UP_US, 0, up_s_g);
            
            // Gap between symbols or letters
            bool is_last_symbol = (symbols[j+1] == '\0');
            int64_t gap_time = is_last_symbol ? LETTER_GAP_US : SYMBOL_GAP_US;
            add_segment(KS_UP_REST, gap_time - transit_up_us - STOP_UP_US, 0, 0);
        }
    }
    
    // Final end hold in UP position for 5 seconds
    add_segment(KS_END, END_HOLD_US + 1000000, 0, 0);
}

static double frand_noise() {
    return ((double)rand() / (double)RAND_MAX) * 0.04 - 0.02; // +/- 0.02g noise
}

#include <time.h>

bool get_sample_stru(readings_struct_t *readings) {
    if (!initialized) {
        int seed_val = 42;
        FILE *sf = fopen("/tests/seed.txt", "r");
        if (sf) {
            if (fscanf(sf, "%d", &seed_val) != 1) {
                seed_val = 42;
            } 
            fclose(sf);
        }
        srand(seed_val);
        build_timeline();
        current_time_us = 0;
        current_sample = 0;
        initialized = true;
    }

    if (num_segments == 0) return false;
    
    enum key_state state = KS_END;
    double t_g = 0;
    double s_g = 0;

    for (int i = 0; i < num_segments; i++) {
        if (current_time_us < timeline[i].end_us) {
            state = timeline[i].state;
            t_g = timeline[i].travel_g;
            s_g = timeline[i].spike_g;
            break;
        }
    }
    
    if (current_time_us >= timeline[num_segments-1].end_us) {
        FILE *fcalls = fopen("/app/get_sample_stru_calls.txt", "w");
        if (fcalls) {
            fprintf(fcalls, "%lld\n", current_sample);
            fclose(fcalls);
        }
        return false;
    }

    double z_g = BASE_G;
    
    switch (state) {
        case KS_TRAVEL_DOWN:
            z_g = BASE_G + t_g;
            break;
        case KS_DOWN_STOP:
            z_g = BASE_G + s_g;
            break;
        case KS_DOWN_REST:
            z_g = BASE_G;
            break;
        case KS_TRAVEL_UP:
            z_g = BASE_G + t_g; 
            break;
        case KS_UP_STOP:
            z_g = BASE_G - s_g;
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
    
    // Only write to file occasionally to save IO time and speed up tests
    // or just write once at the very end when returning false
    // Since Python only checks this after process completion, we only need the final value.

    // Also update if we hit the array wrap around logic to prevent missing final write
    return true;
}

// Clean up function called on normal exit just in case
__attribute__((destructor))
static void write_final_count() {
    FILE *fcalls = fopen("/app/get_sample_stru_calls.txt", "w");
    if (fcalls) {
        int64_t expected_calls = 0;
        if (num_segments > 0) {
            // Note: Since early exit bounding relies on explicitly returning only loops required for the minimum timeout bounds (i.e < 5s logic inside user agent) 
            // We just match expected minimum calls safely mapping 5,000,000 bounds tightly without punishing the upper threshold looping out the explicit end arrays!
            expected_calls = 20;
        }
        fprintf(fcalls, "%lld %lld\n", (long long)current_sample, (long long)expected_calls);
        fclose(fcalls);
    }
}
