#ifndef EMERGENCY_MANAGER_H
#define EMERGENCY_MANAGER_H

#include <stdint.h>
#include <stdbool.h>

#define MAX_EMERGENCY_CONTACTS 5
#define EMERGENCY_TEXT_LEN 128

typedef enum {
    kEmergencyFall,
    kEmergencyWandering,
    kEmergencySOS,
    kEmergencyMedical,
    kEmergencyNone
} emergency_type_t;

typedef struct {
    uint32_t timestamp_us;
    float impact_magnitude_g;
    float orientation_change_deg;
    float freefall_duration_ms;
    bool confirmed;
} fall_event_t;

typedef struct {
    double center_lat, center_lon;
    float radius_m;
    bool active;
} geofence_t;

typedef struct {
    emergency_type_t type;
    char text[EMERGENCY_TEXT_LEN];
    double latitude, longitude;
    uint32_t timestamp_us;
    uint8_t severity;
    bool acknowledged;
} emergency_msg_t;

typedef struct {
    char name[64];
    char phone[20];
    char email[64];
    bool notify_on_fall;
    bool notify_on_wandering;
    bool notify_on_sos;
} emergency_contact_t;

typedef struct {
    float impact_threshold_g;
    float freefall_threshold_g;
    float orientation_change_threshold_deg;
    uint32_t impact_duration_ms;
    uint32_t post_fall_immobility_s;
    uint32_t false_positive_window_s;
    geofence_t safe_zone;
    emergency_contact_t contacts[MAX_EMERGENCY_CONTACTS];
} emergency_config_t;

typedef enum {
    EMERG_OK = 0,
    EMERG_ERR_NOT_INIT,
    EMERG_ERR_NO_CONTACTS,
    EMERG_ERR_GPS_UNAVAIL
} emergency_status_t;

emergency_status_t emergency_init(const emergency_config_t *config);
emergency_status_t emergency_check_fall(const float accel[3],
                                         const float gyro[3],
                                         fall_event_t *out_fall);
emergency_status_t emergency_check_wandering(double lat, double lon,
                                               bool *out_wandering);
emergency_status_t emergency_sos_triggered(void);
emergency_status_t emergency_get_status(emergency_msg_t *out);
emergency_status_t emergency_clear(emergency_type_t type);
emergency_status_t emergency_deinit(void);

#endif /* EMERGENCY_MANAGER_H */
