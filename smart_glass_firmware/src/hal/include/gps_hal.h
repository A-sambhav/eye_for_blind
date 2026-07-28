#ifndef GPS_HAL_H
#define GPS_HAL_H

#include "message_types.h"
#include <stdint.h>
#include <stdbool.h>

typedef enum {
    GPS_HAL_OK = 0,
    GPS_HAL_ERR_INIT,
    GPS_HAL_ERR_NO_FIX,
    GPS_HAL_ERR_READ
} gps_hal_status_t;

typedef struct {
    uint8_t update_rate_hz;
    uint8_t power_mode;      /* 0=full, 1=power_save, 2=standby */
    float max_hdop;
    uint8_t min_sats;
} gps_hal_config_t;

gps_hal_status_t gps_hal_init(const gps_hal_config_t *config);
gps_hal_status_t gps_hal_read(gps_position_t *out);
gps_hal_status_t gps_hal_get_fix_quality(uint8_t *out_fix);
gps_hal_status_t gps_hal_set_update_rate(uint8_t hz);
gps_hal_status_t gps_hal_set_power_mode(uint8_t mode);
gps_hal_status_t gps_hal_deinit(void);

#endif /* GPS_HAL_H */
