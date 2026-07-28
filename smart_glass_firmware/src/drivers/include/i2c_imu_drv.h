#ifndef I2C_IMU_DRV_H
#define I2C_IMU_DRV_H

#include <stdint.h>

/* Conversion constants — placeholders. Replace with the actual sensor's
 * datasheet sensitivity values once the specific IMU part is selected
 * (see HW-ARCH-001.md open item on IMU part number). */
#define IMU_ACCEL_LSB_TO_MPS2  (9.80665f / 16384.0f)  /* assumes +-2g, 16-bit */
#define IMU_GYRO_LSB_TO_RADPS  (0.0010652f)           /* assumes +-250dps, 16-bit */

typedef struct {
    int16_t accel_x_raw, accel_y_raw, accel_z_raw;
    int16_t gyro_x_raw, gyro_y_raw, gyro_z_raw;
} i2c_imu_raw_t;

int i2c_imu_drv_init(void);
int i2c_imu_drv_self_test(void);
int i2c_imu_drv_read(i2c_imu_raw_t *out);

#endif /* I2C_IMU_DRV_H */
