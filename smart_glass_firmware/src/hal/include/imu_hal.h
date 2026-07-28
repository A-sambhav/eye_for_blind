#ifndef IMU_HAL_H
#define IMU_HAL_H

#include "message_types.h"
#include <stdbool.h>

typedef enum {
    IMU_HAL_OK = 0,
    IMU_HAL_ERR_INIT,
    IMU_HAL_ERR_SELF_TEST
} imu_hal_status_t;

/* Configures orientation fusion mode and runs sensor self-test, per
 * "IMU driver (self-test -> pass/fail)" in the startup sequence. */
imu_hal_status_t imu_hal_init(void);

/* Fills `out` with the latest sample. Called at 1kHz from imu_task
 * (interrupt-driven per SW-ARCH-001 19.1). */
imu_hal_status_t imu_hal_read(imu_data_t *out);

#endif /* IMU_HAL_H */
