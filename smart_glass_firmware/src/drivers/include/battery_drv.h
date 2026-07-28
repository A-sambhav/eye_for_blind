#ifndef BATTERY_DRV_H
#define BATTERY_DRV_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    uint32_t voltage_uv;
    int32_t current_ma;
    float temp_c;
    uint8_t soc_pct;
    uint8_t soc_unfiltered_pct;
    uint8_t charging_status; /* 0=discharging, 1=charging, 2=full, 3=error */
    uint16_t cycle_count;
    uint32_t full_capacity_uah;
    uint32_t remaining_capacity_uah;
    uint16_t fault_flags;
} bq_status_t;

typedef struct {
    uint8_t i2c_addr;
    uint32_t i2c_baud;
    uint8_t alert_soc_pct;
} battery_drv_config_t;

int battery_drv_init(const battery_drv_config_t *config);
int battery_drv_read_status(bq_status_t *out);
int battery_drv_set_alert_soc(uint8_t soc_pct);
int battery_drv_enter_shutdown(void);
int battery_drv_deinit(void);

#endif /* BATTERY_DRV_H */
