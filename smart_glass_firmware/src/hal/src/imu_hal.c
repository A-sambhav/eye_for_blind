#include <stddef.h>
#include "imu_hal.h"
#include "i2c_imu_drv.h"
#include "FreeRTOS.h"
#include "task.h"

static bool imu_ready;

imu_hal_status_t imu_hal_init(void)
{
    if (i2c_imu_drv_init() != 0) {
        return IMU_HAL_ERR_INIT;
    }
    if (i2c_imu_drv_self_test() != 0) {
        return IMU_HAL_ERR_SELF_TEST;
    }
    imu_ready = true;
    return IMU_HAL_OK;
}

imu_hal_status_t imu_hal_read(imu_data_t *out)
{
    if (!imu_ready || out == NULL) {
        return IMU_HAL_ERR_INIT;
    }

    i2c_imu_raw_t raw;
    if (i2c_imu_drv_read(&raw) != 0) {
        return IMU_HAL_ERR_INIT;
    }

    /* TODO(imu_hal): apply calibration offsets loaded from db layer
     * (imu_calibration table in schema.sql) once db_manager is wired up.
     * Currently a straight unit conversion with zero offsets. */
    out->accel_x = raw.accel_x_raw * IMU_ACCEL_LSB_TO_MPS2;
    out->accel_y = raw.accel_y_raw * IMU_ACCEL_LSB_TO_MPS2;
    out->accel_z = raw.accel_z_raw * IMU_ACCEL_LSB_TO_MPS2;
    out->gyro_x = raw.gyro_x_raw * IMU_GYRO_LSB_TO_RADPS;
    out->gyro_y = raw.gyro_y_raw * IMU_GYRO_LSB_TO_RADPS;
    out->gyro_z = raw.gyro_z_raw * IMU_GYRO_LSB_TO_RADPS;
    out->timestamp_us = xTaskGetTickCount() * portTICK_PERIOD_MS * 1000;

    return IMU_HAL_OK;
}
