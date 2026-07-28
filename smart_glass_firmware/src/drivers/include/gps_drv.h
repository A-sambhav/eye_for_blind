#ifndef GPS_DRV_H
#define GPS_DRV_H

#include <stdint.h>
#include <stdbool.h>

#define GPS_UBX_BUF_SIZE 256
#define GPS_NMEA_MAX_LINE 128

typedef struct {
    int32_t lon_raw;      /* 1e-7 degrees */
    int32_t lat_raw;      /* 1e-7 degrees */
    int32_t alt_mm;
    int32_t speed_mmps;
    int32_t heading_deg_1e5;
    uint32_t hacc_mm;
    uint32_t vacc_mm;
    uint8_t num_sats;
    uint8_t fix_type;     /* 0=no, 2=2D, 3=3D, 4=RTK float, 5=RTK fixed */
    uint32_t tow_ms;
} gps_pvt_t;

typedef struct {
    uint32_t baud;
    uint8_t update_rate_hz;
    bool use_ubx;         /* true=UBX binary, false=NMEA */
} gps_drv_config_t;

int gps_drv_init(const gps_drv_config_t *config);
int gps_drv_read_pvt(gps_pvt_t *out);
int gps_drv_send_ubx(const uint8_t *msg, uint32_t len,
                     uint8_t *reply, uint32_t *reply_len);
int gps_drv_set_power_mode(uint8_t mode);  /* 0=full, 1=psm, 2=standby */
int gps_drv_cold_start(void);
int gps_drv_warm_start(void);
int gps_drv_deinit(void);

#endif /* GPS_DRV_H */
