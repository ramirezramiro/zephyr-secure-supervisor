#ifndef SENSOR_HTS221_H
#define SENSOR_HTS221_H

#include <stdbool.h>

/**
 * Start the HTS221 sensor polling thread.
 *
 * @param safe_mode_active True if the system is currently running in safe mode.
 * @return 0 on success, negative errno on failure.
 */
int sensor_hts221_start(bool safe_mode_active);

#endif /* SENSOR_HTS221_H */
