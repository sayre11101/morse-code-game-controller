#include "mock_sensor.h"

// Scaffold file for the agent to know the get_sample_stru signature.
// The actual implementation is injected by the test suite dynamically during testing.

bool get_sample_stru(readings_struct_t *readings) {
    // Scaffold implementation
    // Provide blank dummy outputs so the codebase statically compiles during development
    if (readings) {
        readings->x_acc = 0.0;
        readings->y_acc = 0.0;
        readings->z_acc = -1.0;
        readings->sampling_rate_usec = 100;
        readings->sample_number = 0;
    }
    return false; // Tells the boilerplate logic to finish natively
}
