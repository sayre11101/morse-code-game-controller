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

    double last_z = -1.0;
    double last_y = 0.0;
    bool first_sample = true;

    while (get_sample_stru(&r)) {
        int64_t current_time = r.sample_number * r.sampling_rate_usec;
        
        if (first_sample) {
            last_z = r.z_acc;
            last_y = r.y_acc;
            first_sample = false;
        }

        // Compute instantaneous change across axes
        double dz = r.z_acc - last_z;
        double dy = r.y_acc - last_y;
        
        // Car banking physics takes seconds to change, so dy/dz naturally sits at ~0.0001 per sample.
        // However, the finger pushing the key accelerates it rapidly to cross the 2.0mm gap in 3.0ms.
        // This transit yields a sustained +/- 0.3g (squared > 0.04) delta compared to BASE_G!
        
        // Even harder: ADC sampling aliasing can make the subsequent stop spikes (3.0g bounds) randomly DISAPPEAR completely!
        // We must detect just the initial entry vector into the +/- 0.3g transit phase seamlessly instead of looking for massive jolts.
        
        bool impact = (dy*dy + dz*dz > 0.04);
        
        // M2 1.5g rigorous clipping: if the chassis is already vibrating near 1.3g, a 0.3g push 
        // clips the sensor and the delta visually zeroes out. We catch flatlines!
        bool clipped_top = (r.z_acc > 1.49 || r.y_acc > 1.49) && !is_down;
        bool clipped_bottom = (r.z_acc < -1.49 || r.y_acc < -1.49) && is_down;
        if (clipped_top || clipped_bottom) {
            impact = true;
            // Prevent spamming the terminal by only announcing when we actually transition!
        }

        // If dz is negative, it's either the start of downward transit, or an upward stop spike.
        // If dz is positive, it's either the end of downward transit (returning to base), or a downward stop spike.
        // Either indicates a massive polarity flip that shifts the physical key state boundary.
        bool spike_down = impact && ((dz > 0.1 || clipped_top) || (dz < -0.1 && dy*dy+dz*dz > 0.6));
        bool spike_up = impact && ((dz < -0.1 || clipped_bottom) || (dz > 0.1 && dy*dy+dz*dz > 0.6));
        
        // Use a cooldown or state lock to avoid bouncing on recovery
        if (spike_down && !is_down) {
            // Minimum 1ms spacing to avoid double triggering on an impact spike occurring right after its own transit start!
            if (last_transition_time == 0 || (current_time - last_transition_time > 1000)) {
                if (last_transition_time > 0) {
                    pulses[num_pulses].is_mark = false;
                    pulses[num_pulses].duration = current_time - last_transition_time;
                    num_pulses++;
                }
                is_down = true;
                last_transition_time = current_time;
                //if (clipped_top) printf("[FLATLINE] Sensor violently clipped forcing DOWN!\n");
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
                //if (clipped_bottom) printf("[FLATLINE] Sensor violently clipped forcing UP!\n");
            }
        }
        
        // End condition hold time
        if (!is_down && last_transition_time > 0) {
            int64_t gap = current_time - last_transition_time;
            if (gap >= 5000000) { // 5 seconds
                break;
            }
        }
        
        last_z = r.z_acc;
        last_y = r.y_acc;
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
