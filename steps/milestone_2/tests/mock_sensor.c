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
static int64_t END_HOLD_US = 5000000; 
static int64_t SAMPLE_PERIOD_US = 100; 

// Config for physics (Z axis keys)
static double BASE_G = -1.0; 
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
    DOT_US = 100000 + (rand() % 150001);
    DASH_US = 3 * DOT_US;
    SYMBOL_GAP_US = DOT_US;
    LETTER_GAP_US = 3 * DOT_US;
    WORD_GAP_US = 7 * DOT_US;

    static const char *WORDS[] = {
      "SOS SEND HELP NOW PLEASE",
      "RADIO WAVES ARE COOL",
      "MORSE CODE IS VERY OLD",
      "PHYSICS AND SOFTWARE",
      "ACCELEROMETER READS G",
      "SOLVE THE PUZZLE FAST",
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

    // write selected word for verify matching logic
    FILE *fsw = fopen("/tests/secret_word.txt", "w");
    if (fsw) {
        fprintf(fsw, "%s\n", word);
        fclose(fsw);
    }
    
    add_segment(KS_UP_REST, 1000000, 0, 0);
    
    for (int i = 0; word[i] != '\0'; i++) {
        if (word[i] == ' ') {
            add_segment(KS_UP_REST, WORD_GAP_US - LETTER_GAP_US, 0, 0);
            continue;
        }

        int char_idx = word[i] - 'A';
        if (char_idx < 0 || char_idx > 25) continue;
        
        const char *symbols = morse_dict[char_idx];
        for (int j = 0; symbols[j] != '\0'; j++) {
            int64_t transit_down_us = 2000 + (rand() % 4001);
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

            add_segment(KS_TRAVEL_DOWN, transit_down_us, travel_down_g, 0);
            add_segment(KS_DOWN_STOP, STOP_DOWN_US, 0, down_s_g);
            
            int64_t hold_time = (symbols[j] == '-') ? DASH_US : DOT_US;
            add_segment(KS_DOWN_REST, hold_time - transit_down_us - STOP_DOWN_US, 0, 0);
            
            add_segment(KS_TRAVEL_UP, transit_up_us, travel_up_g, 0);
            add_segment(KS_UP_STOP, STOP_UP_US, 0, up_s_g);
            
            bool is_last_symbol = (symbols[j+1] == '\0');
            int64_t gap_time = is_last_symbol ? LETTER_GAP_US : SYMBOL_GAP_US;
            add_segment(KS_UP_REST, gap_time - transit_up_us - STOP_UP_US, 0, 0);
        }
    }
    
    add_segment(KS_END, END_HOLD_US + 1000000, 0, 0);
}

// Car physics generators
static double car_vel_ms = 0.0;     // 0 to 20 m/s
static double car_accel_x = 0.0;    // x accel in g
static double car_accel_y = 0.0;    // y accel in g
static double car_accel_z = 0.0;    // base z accel in g (gravity + hills)
static double car_bank_angle = 0.0; // road bank angle in radians

static int64_t last_car_event_time = 0;
static int car_state = 0; 
// 0=steady, 1=accel, 2=brake, 3=turn_left, 4=turn_right, 5=hill_up, 6=hill_down

static void update_car_physics(int64_t current_time) {
    // Change car state every 1 to 4 seconds randomly
    if (current_time - last_car_event_time > (int64_t)(1000000 + (rand() % 3000000))) {
        car_state = rand() % 7;
        last_car_event_time = current_time;
    }

    double dt = SAMPLE_PERIOD_US / 1000000.0;
    
    // Smooth transition variables for forces instead of instant jumps
    double target_y = 0.0;
    double target_z = 0.0;

    // Gradually return bank angle to zero when not turning
    if (car_state != 3 && car_state != 4) {
        if (car_bank_angle > 0.01) car_bank_angle -= 0.01;
        else if (car_bank_angle < -0.01) car_bank_angle += 0.01;
        else car_bank_angle = 0.0;
    }

    switch (car_state) {
        case 0: // coasting
            car_accel_x = 0.0;
            break;
        case 1: // accelerating linearly (up to 4 m/s^2)
            car_accel_x = 4.0 / 9.8; 
            car_vel_ms += 4.0 * dt;
            if (car_vel_ms > 20.0) {
                car_vel_ms = 20.0;
                car_accel_x = 0.0;
            }
            break;
        case 2: // braking safely (up to -0.9g)
            car_accel_x = -0.9;
            car_vel_ms += (-0.9 * 9.8) * dt;
            if (car_vel_ms < 0.0) {
                car_vel_ms = 0.0;
                car_accel_x = 0.0;
            }
            break;
        case 3: // turn left (if moving)
            if (car_vel_ms > 5.0) {
                // centrifugal force v^2 / r. Min radius 30m
                target_y = (car_vel_ms * car_vel_ms / 30.0) / 9.8;
                // gradually bank road up to 15 degrees (-0.26 radians)
                if (car_bank_angle > -0.26) car_bank_angle -= 0.01;
            }
            break;
        case 4: // turn right
            if (car_vel_ms > 5.0) {
                target_y = -(car_vel_ms * car_vel_ms / 30.0) / 9.8;
                // gradually bank road up to +15 degrees (+0.26 radians)
                if (car_bank_angle < 0.26) car_bank_angle += 0.01;
            }
            break;
        case 5: // hill up (concave up -> positive Z felt force)
            // min radius 30 meters = 400/30 = 13.3 m/s^2 (~1.36g max)
            if (car_vel_ms > 5.0) {
                target_z = (car_vel_ms * car_vel_ms / 30.0) / 9.8;
            }
            break;
        case 6: // hill down (convex -> negative Z felt force)
            if (car_vel_ms > 5.0) {
                target_z = -(car_vel_ms * car_vel_ms / 30.0) / 9.8;
            }
            break;
    }

    // Smoothly ease actual acceleration towards the targets to prevent vertical derivative magnitude blowups
    if (car_accel_y < target_y) car_accel_y += 0.05 * dt;
    if (car_accel_y > target_y) car_accel_y -= 0.05 * dt;

    if (car_accel_z < target_z) car_accel_z += 0.05 * dt;
    if (car_accel_z > target_z) car_accel_z -= 0.05 * dt;
}

static double frand_noise() {
    return ((double)rand() / (double)RAND_MAX) * 0.06 - 0.03; // +/- 0.03g background noise
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

    update_car_physics(current_time_us);

    double z_g = BASE_G;
    switch (state) {
        case KS_TRAVEL_DOWN: z_g = BASE_G + t_g; break;
        case KS_DOWN_STOP: z_g = BASE_G + s_g; break;
        case KS_DOWN_REST: z_g = BASE_G; break;
        case KS_TRAVEL_UP: z_g = BASE_G + t_g; break;
        case KS_UP_STOP: z_g = BASE_G - s_g; break;
        case KS_UP_REST: z_g = BASE_G; break;
        case KS_END: z_g = BASE_G; break;
    }

    // Key impact force occurs along the physical z-axis of the key structure
    double total_unbanked_z = z_g + car_accel_z;
    double total_unbanked_y = car_accel_y;
    
    // Apply banking to the car/key chassis
    // Y' = Y*cos(theta) - Z*sin(theta)
    // Z' = Y*sin(theta) + Z*cos(theta)
    double banked_y = total_unbanked_y * cos(car_bank_angle) - total_unbanked_z * sin(car_bank_angle);
    double banked_z = total_unbanked_y * sin(car_bank_angle) + total_unbanked_z * cos(car_bank_angle);

    // Apply baseline car physics to axes with noise
    readings->x_acc = car_accel_x + frand_noise();
    readings->y_acc = banked_y + frand_noise();
    readings->z_acc = banked_z + frand_noise();
    
    // Apply strict 1.5g hardware clipping
    if (readings->x_acc > 10.0) readings->x_acc = 10.0;
    if (readings->x_acc < -10.0) readings->x_acc = -10.0;
    
    if (readings->y_acc > 10.0) readings->y_acc = 10.0;
    if (readings->y_acc < -10.0) readings->y_acc = -10.0;
    
    if (readings->z_acc > 10.0) readings->z_acc = 10.0;
    if (readings->z_acc < -10.0) readings->z_acc = -10.0;

    readings->sampling_rate_usec = SAMPLE_PERIOD_US;
    readings->sample_number = current_sample;
    
    current_time_us += SAMPLE_PERIOD_US;
    current_sample++;
    
    // Only write to file occasionally to save IO time and speed up tests
    // or just write once at the very end when returning false
    // Since Python only checks this after process completion, we only need the final value.

    return true;
}

// Clean up function called on normal exit just in case
__attribute__((destructor))
static void write_final_count() {
    FILE *fcalls = fopen("/app/get_sample_stru_calls.txt", "w");
    if (fcalls) {
        fprintf(fcalls, "%lld\n", current_sample);
        fclose(fcalls);
    }
}
