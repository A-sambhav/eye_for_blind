#include <string.h>
#include "gps_hal.h"
#include "gps_drv.h"
#include "message_bus.h"
#include "logging_manager.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

static struct {
    gps_hal_config_t config;
    gps_pvt_t last_pvt;
    uint32_t last_read_tick;
    uint32_t read_interval_ticks;
    uint32_t read_count;
    uint32_t error_count;
    SemaphoreHandle_t lock;
    bool initialized;
} gh;

static void pvt_to_gps_position(const gps_pvt_t *pvt, gps_position_t *out)
{
    memset(out, 0, sizeof(*out));
    out->latitude = (double)pvt->lat_raw * 1e-7;
    out->longitude = (double)pvt->lon_raw * 1e-7;
    out->altitude = (double)pvt->alt_mm / 1000.0;
    out->speed_mps = (float)pvt->speed_mmps / 1000.0f;
    out->heading_deg = (float)pvt->heading_deg_1e5 / 1e5f;
    out->hdop = (float)pvt->hacc_mm / 1000.0f;
    out->vdop = (float)pvt->vacc_mm / 1000.0f;
    out->num_satellites = pvt->num_sats;
    out->fix_type = pvt->fix_type;
    out->timestamp_us = pvt->tow_ms * 1000;
    out->valid = (pvt->fix_type >= 3);
}

gps_hal_status_t gps_hal_init(const gps_hal_config_t *config)
{
    if (config == NULL) return GPS_HAL_ERR_INIT;
    memset(&gh, 0, sizeof(gh));
    gh.config = *config;
    if (gh.config.update_rate_hz == 0) gh.config.update_rate_hz = 10;
    if (gh.config.max_hdop <= 0) gh.config.max_hdop = 2.0f;
    if (gh.config.min_sats == 0) gh.config.min_sats = 4;

    gh.lock = xSemaphoreCreateMutex();
    if (gh.lock == NULL) return GPS_HAL_ERR_INIT;

    gps_drv_config_t drv_cfg = {
        .baud = 115200,
        .update_rate_hz = gh.config.update_rate_hz,
        .use_ubx = true,
    };
    if (gps_drv_init(&drv_cfg) != 0) return GPS_HAL_ERR_INIT;
    gps_drv_set_power_mode(gh.config.power_mode);

    gh.read_interval_ticks = pdMS_TO_TICKS(1000 / gh.config.update_rate_hz);
    gh.last_read_tick = xTaskGetTickCount();
    gh.initialized = true;
    log_info("gps_hal", "Init rate=%dHz power=%d hdop=%.1f sats=%d",
             gh.config.update_rate_hz, gh.config.power_mode,
             gh.config.max_hdop, gh.config.min_sats);
    return GPS_HAL_OK;
}

gps_hal_status_t gps_hal_read(gps_position_t *out)
{
    if (!gh.initialized || out == NULL) return GPS_HAL_ERR_INIT;

    uint32_t now = xTaskGetTickCount();
    if (now - gh.last_read_tick >= gh.read_interval_ticks) {
        xSemaphoreTake(gh.lock, portMAX_DELAY);
        gps_pvt_t pvt;
        if (gps_drv_read_pvt(&pvt) == 0) {
            gh.last_pvt = pvt;
            gh.read_count++;
            gps_position_t pos;
            pvt_to_gps_position(&pvt, &pos);
            message_bus_publish(MSG_GPS_POSITION, &pos, sizeof(pos), 3);
            xSemaphoreGive(gh.lock);
            *out = pos;
            return GPS_HAL_OK;
        } else {
            gh.error_count++;
            xSemaphoreGive(gh.lock);
            return GPS_HAL_ERR_READ;
        }
    }

    gps_position_t pos;
    pvt_to_gps_position(&gh.last_pvt, &pos);
    *out = pos;
    return GPS_HAL_OK;
}

gps_hal_status_t gps_hal_get_fix_quality(uint8_t *out_fix)
{
    if (!gh.initialized || out_fix == NULL) return GPS_HAL_ERR_INIT;
    *out_fix = gh.last_pvt.fix_type;
    return GPS_HAL_OK;
}

gps_hal_status_t gps_hal_set_update_rate(uint8_t hz)
{
    if (!gh.initialized || hz == 0) return GPS_HAL_ERR_INIT;
    gh.config.update_rate_hz = hz;
    gh.read_interval_ticks = pdMS_TO_TICKS(1000 / hz);
    return GPS_HAL_OK;
}

gps_hal_status_t gps_hal_set_power_mode(uint8_t mode)
{
    if (!gh.initialized) return GPS_HAL_ERR_INIT;
    gh.config.power_mode = mode;
    gps_drv_set_power_mode(mode);
    return GPS_HAL_OK;
}

gps_hal_status_t gps_hal_deinit(void)
{
    gps_drv_deinit();
    gh.initialized = false;
    return GPS_HAL_OK;
}
