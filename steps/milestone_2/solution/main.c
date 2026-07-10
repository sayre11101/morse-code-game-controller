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

    while (get_sample_stru(&r)) {
        int64_t current_time = r.sample_number * r.sampling_rate_usec;
        
        // Use derivative of acceleration to find the stop spikes!
        // Car movement maxes out around 0.4g force spread out over ~2 seconds.
        // That's a tiny delta per 100usec sample.
        // A Morse Key stop generates ~500g in 100usec. Even clipped to 1.5,
        // it means the absolute check remains highly reliable since driving caps at 0.4g.
        
        // If the reading pins at closely to the positive 1.5g limit, it's hitting the bottom
        bool spike_down = (r.z_acc > 1.4);
        
        // If the reading pins at closely to the negative 1.5g limit, it's hitting the top.
        // Even with car physics pushing z_acc to -1.4g, the hit is -1.5g.
        bool spike_up = (r.z_acc < -1.45);
        
        // Use a cooldown or state lock to avoid bouncing on recovery
        if (spike_down && !is_down) {
            // Must have been up for at least 1ms to count as a transition
            if (last_transition_time == 0 || (current_time - last_transition_time > 1000)) {
                if (last_transition_time > 0) {
                    pulses[num_pulses].is_mark = false;
                    pulses[num_pulses].duration = current_time - last_transition_time;
                    num_pulses++;
                }
                is_down = true;
                last_transition_time = current_time;
            }
        } 
        else if (spike_up && is_down) {
            // Must have been down for at least 1ms to count as a transition
            if (last_transition_time == 0 || (current_time - last_transition_time > 1000)) {
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
    
    int64_t unit_time = min_mark;
    if (unit_time == 0) unit_time = 150000; 
    
    char word[100] = {0};
    int word_idx = 0;
    char current_letter[10] = {0};
    int symbol_idx = 0;
    
    for (int i = 0; i < num_pulses; i++) {
        double units = (double)pulses[i].duration / (double)unit_time;
        
        if (pulses[i].is_mark) {
            if (units < 2.0) {
                current_letter[symbol_idx++] = '.';
            } else {
                current_letter[symbol_idx++] = '-';
            }
        } else {
            if (units > 2.0) { // Letter gap or word gap
                current_letter[symbol_idx] = '\0';
                word[word_idx++] = decode_letter(current_letter);
                symbol_idx = 0;
                
                if (units >= 6.0) { // Word gap
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
