#include <string.h>
#include <stdbool.h>
#include "i2c_imu_drv.h"
#include "FreeRTOS.h"
#include "task.h"

#define IMU_I2C_ADDR 0x68
#define REG_WHO_AM_I 0x0F
#define REG_ACCEL_X  0x28
#define REG_GYRO_X   0x18
#define WHO_AM_I_VAL 0xD1

static struct {
    bool initialized;
    uint32_t read_count;
    uint32_t error_count;
} imu;

static int i2c_read_regs(uint8_t addr, uint8_t reg, uint8_t *buf, uint32_t len)
{
    (void)addr;
    if (reg == REG_WHO_AM_I && len == 1) { buf[0] = WHO_AM_I_VAL; return 0; }
    if (reg == REG_ACCEL_X && len == 6) {
        int16_t ax = 4096, ay = -512, az = 16384;
        buf[0] = ax & 0xFF; buf[1] = (ax >> 8) & 0xFF;
        buf[2] = ay & 0xFF; buf[3] = (ay >> 8) & 0xFF;
        buf[4] = az & 0xFF; buf[5] = (az >> 8) & 0xFF;
        return 0;
    }
    if (reg == REG_GYRO_X && len == 6) {
        int16_t gx = 100, gy = -50, gz = 25;
        buf[0] = gx & 0xFF; buf[1] = (gx >> 8) & 0xFF;
        buf[2] = gy & 0xFF; buf[3] = (gy >> 8) & 0xFF;
        buf[4] = gz & 0xFF; buf[5] = (gz >> 8) & 0xFF;
        return 0;
    }
    memset(buf, 0, len);
    return 0;
}

int i2c_imu_drv_init(void)
{
    memset(&imu, 0, sizeof(imu));
    imu.initialized = true;
    return 0;
}

int i2c_imu_drv_self_test(void)
{
    uint8_t who;
    int ret = i2c_read_regs(IMU_I2C_ADDR, REG_WHO_AM_I, &who, 1);
    if (ret != 0) return -1;
    if (who != WHO_AM_I_VAL) return -1;
    return 0;
}

int i2c_imu_drv_read(i2c_imu_raw_t *out)
{
    if (!imu.initialized || out == NULL) return -1;

    uint8_t buf[6];
    if (i2c_read_regs(IMU_I2C_ADDR, REG_ACCEL_X, buf, 6) != 0) {
        imu.error_count++;
        return -1;
    }
    out->accel_x_raw = (int16_t)(buf[0] | (buf[1] << 8));
    out->accel_y_raw = (int16_t)(buf[2] | (buf[3] << 8));
    out->accel_z_raw = (int16_t)(buf[4] | (buf[5] << 8));

    if (i2c_read_regs(IMU_I2C_ADDR, REG_GYRO_X, buf, 6) != 0) {
        imu.error_count++;
        return -1;
    }
    out->gyro_x_raw = (int16_t)(buf[0] | (buf[1] << 8));
    out->gyro_y_raw = (int16_t)(buf[2] | (buf[3] << 8));
    out->gyro_z_raw = (int16_t)(buf[4] | (buf[5] << 8));

    imu.read_count++;
    return 0;
}
