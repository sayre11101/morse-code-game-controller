#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include "mock_sensor.h"

#define MAX_PULSES 1000

static const char *morse_dict[26] = {
    ".-",   "-...", "-.-.", "-..",  ".",    // A-E
    "..-.", "--.",  "....", "..",   ".---", // F-J
    "-.-",  ".-..", "--",   "-.",   "---",  // K-O
    ".--.", "--.-", ".-.",  "...",  "-",    // P-T
    "..-",  "...-", ".--",  "-..-", "-.--", // U-Y
    "--.."                                  // Z
};

char decode_letter(const char *symbols) {
    for (int i = 0; i < 26; i++) {
        if (strcmp(morse_dict[i], symbols) == 0) {
            return 'A' + i;
        }
    }
    return '?';
}

int main(void) {
    readings_struct_t r;
    
    bool is_down = false;
    
    int64_t last_transition_time = 0;
    
    int64_t min_mark = 99999999;
    
    struct {
        bool is_mark; // true=mark (down), false=gap (up)
        int64_t duration;
    } pulses[MAX_PULSES];
    int num_pulses = 0;

    // Moving average filter
    double avg_z = -1.0;
    
    // Impact threshold parameters
    bool in_transit = false;

    while (get_sample_stru(&r)) {
        int64_t current_time = r.sample_number * r.sampling_rate_usec;
        
        // Slow moving average to track baseline gravity and car acceleration.
        // The car maneuvers happen on the order of seconds.
        // At 10kHz (100us), alpha=0.0001 means a time constant of ~10000 samples = ~1 second.
        avg_z = avg_z * 0.999 + r.z_acc * 0.001;
        
        // Key impacts are quick. We look for the deviation from the moving baseline!
        // Down strike is a massive negative spike.
        // Return up is a massive positive spike.
        
        double diff = r.z_acc - avg_z;

        bool spike_down = (diff < -1.5);
        bool spike_up   = (diff > +1.5);
        
        if (spike_down && !is_down) {
            in_transit = true;
        } else if (spike_up && is_down) {
            in_transit = true;
        } else if (in_transit && diff > -0.5 && diff < 0.5) {
            // Settled after transit
            if (!is_down) {
                // Was UP, completed travel DOWN
                if (last_transition_time == 0 || current_time - last_transition_time > 20000) {
                    if (last_transition_time > 0) {
                        pulses[num_pulses].is_mark = false;
                        pulses[num_pulses].duration = current_time - last_transition_time;
                        num_pulses++;
                    }
                    is_down = true;
                    last_transition_time = current_time;
                }
                in_transit = false;
            } else if (is_down) {
                // Was DOWN, completed travel UP
                if (last_transition_time == 0 || current_time - last_transition_time > 20000) {
                    if (last_transition_time > 0) {
                        int64_t mark = current_time - last_transition_time;
                        pulses[num_pulses].is_mark = true;
                        pulses[num_pulses].duration = mark;
                        num_pulses++;
                        if (mark < min_mark) min_mark = mark;
                    }
                    is_down = false;
                    last_transition_time = current_time;
                }
                in_transit = false;
            }
        }
        
        // End condition hold time
        if (!is_down && last_transition_time > 0) {
            int64_t gap = current_time - last_transition_time;
            if (gap >= 5000000) { // 5 seconds
                break;
            }
        }
        
    }
    
    if (num_pulses == 0) return 0;
    
    // Moving average tracking to handle drift
    int64_t running_dot_us = min_mark;
    if (running_dot_us == 0) running_dot_us = 150000;

    char word[100] = {0};
    int word_idx = 0;
    char current_letter[10] = {0};
    int symbol_idx = 0;

    for (int i = 0; i < num_pulses; i++) {
        double units = (double)pulses[i].duration / (double)running_dot_us;

        if (pulses[i].is_mark) {
            if (units < 2.0) {
                current_letter[symbol_idx++] = '.';
                // update running average slowly using true dots
                running_dot_us = (int64_t)((running_dot_us * 0.8) + (pulses[i].duration * 0.2));
            } else {
                current_letter[symbol_idx++] = '-';
            }
        } else {
            if (units > 2.0) { // Letter gap or word gap
                current_letter[symbol_idx] = '\0';
                word[word_idx++] = decode_letter(current_letter);
                symbol_idx = 0;

                if (units >= 5.0) { // Word gap
                    word[word_idx++] = ' ';
                }
            }
        }
    }
    
    if (symbol_idx > 0) {
        current_letter[symbol_idx] = '\0';
        word[word_idx++] = decode_letter(current_letter);
    }
    
    printf("%s\n", word);
    return 0;
}
