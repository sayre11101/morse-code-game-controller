#ifndef MOCK_SENSOR_H
#define MOCK_SENSOR_H

#include <stdint.h>
#include <stdbool.h>


typedef struct {
    double x_acc; 
    double y_acc;
    double z_acc;
    int64_t sampling_rate_usec;
    int64_t sample_number;
} readings_struct_t;


/**
 * @brief Get the sample stru object
 * 
 * @param readings structure of the readings
 * @return true if new reading available with new sample number
 * @return false if most recent reading has same sample number in the parameter structure
 */
bool get_sample_stru(readings_struct_t *readings);

#endif /* MOCK_SENSOR_H */