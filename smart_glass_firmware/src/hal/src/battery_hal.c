#include <string.h>
#include "battery_hal.h"
#include "battery_drv.h"
#include "message_bus.h"
#include "logging_manager.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

static struct {
    battery_hal_config_t config;
    bq_status_t last_raw;
    battery_status_t last_published;
    uint32_t last_poll_tick;
    uint32_t poll_interval_ticks;
    SemaphoreHandle_t lock;
    bool initialized;
} bh;

static void publish_status(void)
{
    battery_status_t bs;
    memset(&bs, 0, sizeof(bs));
    bs.level_pct = bh.last_raw.soc_pct;
    bs.charging = (bh.last_raw.charging_status == 1);
    bs.low_warning = bh.last_raw.soc_pct < 15;
    bs.critical = bh.last_raw.soc_pct < 5;
    bs.voltage_mv = bh.last_raw.voltage_uv / 1000;
    bs.current_ma = bh.last_raw.current_ma;
    bs.temp_c = bh.last_raw.temp_c;
    bh.last_published = bs;

    message_bus_publish(MSG_BATTERY_STATUS, &bs, sizeof(bs), 2);
}

battery_hal_status_t battery_hal_init(const battery_hal_config_t *config)
{
    if (config == NULL) return BATTERY_HAL_ERR_INIT;
    memset(&bh, 0, sizeof(bh));
    bh.config = *config;
    if (bh.config.poll_interval_ms == 0) bh.config.poll_interval_ms = 5000;

    bh.lock = xSemaphoreCreateMutex();
    if (bh.lock == NULL) return BATTERY_HAL_ERR_INIT;

    battery_drv_config_t drv_cfg = {
        .i2c_addr = 0x0B,
        .i2c_baud = 100000,
        .alert_soc_pct = bh.config.alert_soc_pct,
    };
    if (battery_drv_init(&drv_cfg) != 0) return BATTERY_HAL_ERR_INIT;
    battery_drv_set_alert_soc(bh.config.alert_soc_pct);

    bh.poll_interval_ticks = pdMS_TO_TICKS(bh.config.poll_interval_ms);
    bh.last_poll_tick = xTaskGetTickCount();
    bh.initialized = true;
    log_info("battery_hal", "Init alert=%d%% poll=%dms",
             bh.config.alert_soc_pct, bh.config.poll_interval_ms);
    return BATTERY_HAL_OK;
}

battery_hal_status_t battery_hal_read(battery_status_t *out)
{
    if (!bh.initialized || out == NULL) return BATTERY_HAL_ERR_INIT;

    uint32_t now = xTaskGetTickCount();
    if (now - bh.last_poll_tick >= bh.poll_interval_ticks) {
        xSemaphoreTake(bh.lock, portMAX_DELAY);
        if (battery_drv_read_status(&bh.last_raw) == 0) {
            publish_status();
        }
        bh.last_poll_tick = now;
        xSemaphoreGive(bh.lock);
    }

    *out = bh.last_published;
    return BATTERY_HAL_OK;
}

uint8_t battery_hal_get_soc(void)
{
    return bh.last_published.level_pct;
}

uint32_t battery_hal_get_voltage_mv(void)
{
    return bh.last_published.voltage_mv;
}

int32_t battery_hal_get_current_ma(void)
{
    return bh.last_published.current_ma;
}

float battery_hal_get_temperature(void)
{
    return bh.last_published.temp_c;
}

battery_hal_status_t battery_hal_enter_shutdown(void)
{
    battery_drv_enter_shutdown();
    return BATTERY_HAL_OK;
}

battery_hal_status_t battery_hal_deinit(void)
{
    battery_drv_deinit();
    bh.initialized = false;
    return BATTERY_HAL_OK;
}
