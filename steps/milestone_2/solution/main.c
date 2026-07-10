#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include "mock_sensor.h"

// We parse morse code pulses.
// Spike > 100g in Z axis -> stop DOWN (we hit the bottom)
// Spike < -100g in Z axis -> stop UP (we hit the top)
// Time between UP->DOWN is a gap (symbol gap, letter gap, word gap)
// Time between DOWN->UP is a mark (dot or dash)

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
    
    // We are interested in states: TRUE=down, FALSE=up
    bool is_down = false;
    
    int64_t last_transition_time = 0;
    
    // Dynamically calculate unit timing
    int64_t min_mark = 99999999;
    
    struct {
        bool is_mark; // true=mark (down), false=gap (up)
        int64_t duration;
    } pulses[MAX_PULSES];
    int num_pulses = 0;

    while (get_sample_stru(&r)) {
        int64_t current_time = r.sample_number * r.sampling_rate_usec;
        
        bool spike_down = (r.z_acc > 100.0);
        bool spike_up = (r.z_acc < -100.0);
        
        if (spike_down && !is_down) {
            // Transition UP -> DOWN
            if (last_transition_time > 0) {
                int64_t gap = current_time - last_transition_time;
                if (num_pulses < MAX_PULSES) {
                    pulses[num_pulses].is_mark = false;
                    pulses[num_pulses].duration = gap;
                    num_pulses++;
                }
            }
            is_down = true;
            last_transition_time = current_time;
        } 
        else if (spike_up && is_down) {
            // Transition DOWN -> UP
            if (last_transition_time > 0) {
                int64_t mark = current_time - last_transition_time;
                if (num_pulses < MAX_PULSES) {
                    pulses[num_pulses].is_mark = true;
                    pulses[num_pulses].duration = mark;
                    num_pulses++;
                }
                if (mark < min_mark) {
                    min_mark = mark;
                }
            }
            is_down = false;
            last_transition_time = current_time;
        }
        
        // Check for end hold
        if (!is_down && last_transition_time > 0) {
            int64_t gap = current_time - last_transition_time;
            if (gap >= 5000000) { // 5 seconds
                break;
            }
        }
    }
    
    if (num_pulses == 0) return 0;
    
    // min_mark should represent roughly 1 dot unit.
    int64_t unit_time = min_mark;
    if (unit_time == 0) unit_time = 150000; // fallback just in case
    
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
    
    // Flush last letter
    if (symbol_idx > 0) {
        current_letter[symbol_idx] = '\0';
        word[word_idx++] = decode_letter(current_letter);
    }
    
    printf("%s\n", word);
    return 0;
}
