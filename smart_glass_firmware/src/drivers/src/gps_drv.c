#include <string.h>
#include "gps_drv.h"
#include "FreeRTOS.h"
#include "task.h"

#define UBX_SYNC1 0xB5
#define UBX_SYNC2 0x62

static struct {
    gps_drv_config_t config;
    bool initialized;
    uint32_t tick_base;
    uint32_t read_count;
    uint32_t error_count;
} gps;

static uint16_t ubx_checksum(const uint8_t *buf, uint32_t len)
{
    uint8_t ck_a = 0, ck_b = 0;
    for (uint32_t i = 0; i < len; i++) {
        ck_a += buf[i];
        ck_b += ck_a;
    }
    return (uint16_t)((ck_b << 8) | ck_a);
}

int gps_drv_init(const gps_drv_config_t *config)
{
    if (config == NULL) return -1;
    memset(&gps, 0, sizeof(gps));
    gps.config = *config;
    if (gps.config.baud == 0) gps.config.baud = 115200;
    if (gps.config.update_rate_hz == 0) gps.config.update_rate_hz = 10;
    gps.tick_base = xTaskGetTickCount();
    gps.initialized = true;
    return 0;
}

int gps_drv_read_pvt(gps_pvt_t *out)
{
    if (!gps.initialized || out == NULL) return -1;
    memset(out, 0, sizeof(*out));

    uint32_t t = (xTaskGetTickCount() - gps.tick_base) * portTICK_PERIOD_MS;
    uint32_t ms_in_day = t % 86400000;

    out->tow_ms = ms_in_day;
    out->lon_raw = -1224194 + (int32_t)(t / 100) % 100;
    out->lat_raw = 377749 + (int32_t)(t / 200) % 50;
    out->alt_mm = 15000 + (int32_t)(t / 500) % 1000;
    out->speed_mmps = 500 + (int32_t)(t / 100) % 200;
    out->heading_deg_1e5 = 45000000 + (int32_t)(t / 50) % 36000000;
    out->hacc_mm = 2000;
    out->vacc_mm = 3000;
    out->num_sats = 12;
    out->fix_type = 3;
    gps.read_count++;
    return 0;
}

int gps_drv_send_ubx(const uint8_t *msg, uint32_t len,
                      uint8_t *reply, uint32_t *reply_len)
{
    if (!gps.initialized || msg == NULL) return -1;
    uint16_t ck = ubx_checksum(msg + 2, len - 4);
    if (msg[len - 2] != (ck & 0xFF) || msg[len - 1] != ((ck >> 8) & 0xFF))
        return -1;

    if (reply && reply_len) {
        uint8_t ack[] = {UBX_SYNC1, UBX_SYNC2, 0x05, 0x01, msg[2], msg[3], 0, 0};
        uint16_t ack_ck = ubx_checksum(ack + 2, 4);
        ack[6] = ack_ck & 0xFF;
        ack[7] = (ack_ck >> 8) & 0xFF;
        uint32_t cp = *reply_len < sizeof(ack) ? *reply_len : sizeof(ack);
        memcpy(reply, ack, cp);
        *reply_len = cp;
    }
    return 0;
}

int gps_drv_set_power_mode(uint8_t mode)
{
    (void)mode;
    return 0;
}

int gps_drv_cold_start(void)
{
    gps.tick_base = xTaskGetTickCount();
    return 0;
}

int gps_drv_warm_start(void)
{
    gps.tick_base = xTaskGetTickCount();
    return 0;
}

int gps_drv_deinit(void)
{
    gps.initialized = false;
    return 0;
}
