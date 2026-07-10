#include "mock_sensor.h"
#include <sys/time.h>
#include <stddef.h>

int64_t now_us(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (int64_t)tv.tv_sec * 1000000LL + tv.tv_usec;
}

static readings_struct_t readings = {
    .sample_number = 0,
    .sampling_rate_usec = 100,
    .x_acc = 0,
    .y_acc = 0,
    .z_acc = 0,
};

bool get_sample_stru(readings_struct_t *cur_readings)
{
    if (cur_readings == NULL || cur_readings->sampling_rate_usec <= 0)
    {
        return false;
    }

    int64_t cur_sample = now_us() / readings.sampling_rate_usec;
    bool got_new_sample = cur_sample != readings.sample_number;

    readings.sample_number = cur_sample;

    // During the test the values of x_acc, y_acc and z_acc will be filled in
    *cur_readings = readings;

    return  got_new_sample;
}
