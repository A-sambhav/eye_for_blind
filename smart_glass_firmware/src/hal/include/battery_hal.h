#ifndef BATTERY_HAL_H
#define BATTERY_HAL_H

#include "message_types.h"
#include <stdint.h>
#include <stdbool.h>

typedef enum {
    BATTERY_HAL_OK = 0,
    BATTERY_HAL_ERR_INIT,
    BATTERY_HAL_ERR_COMM
} battery_hal_status_t;

typedef struct {
    uint8_t alert_soc_pct;
    uint16_t poll_interval_ms;
} battery_hal_config_t;

battery_hal_status_t battery_hal_init(const battery_hal_config_t *config);
battery_hal_status_t battery_hal_read(battery_status_t *out);
uint8_t battery_hal_get_soc(void);
uint32_t battery_hal_get_voltage_mv(void);
int32_t battery_hal_get_current_ma(void);
float battery_hal_get_temperature(void);
battery_hal_status_t battery_hal_enter_shutdown(void);
battery_hal_status_t battery_hal_deinit(void);

#endif /* BATTERY_HAL_H */
