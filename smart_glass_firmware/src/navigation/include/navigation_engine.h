#ifndef NAVIGATION_ENGINE_H
#define NAVIGATION_ENGINE_H

#include <stdint.h>
#include <stdbool.h>

#define NAV_MAX_WAYPOINTS 256
#define NAV_SPEECH_MAX_LEN 128

typedef struct {
    double latitude, longitude;
    float altitude;
    float heading_deg;
    float speed_mps;
    float accuracy;
    uint32_t timestamp_us;
    bool estimated;
} nav_position_t;

typedef struct {
    double latitude, longitude;
    float altitude;
    char label[32];
    bool reached;
    float arrival_radius_m;
} nav_waypoint_t;

typedef struct {
    uint16_t bearing_deg;
    float distance_m;
    char street_name[32];
    uint8_t turn_type;
    float turn_angle_deg;
    bool is_last_turn;
} nav_turn_t;

typedef struct {
    nav_waypoint_t waypoints[NAV_MAX_WAYPOINTS];
    uint32_t waypoint_count;
    uint32_t current_waypoint_idx;
    nav_position_t current_pos;
    nav_turn_t next_turn;
    bool route_active;
    bool is_off_route;
    float total_distance_m;
    float remaining_distance_m;
    float deviation_m;
    uint32_t recalc_count;
} nav_status_t;

typedef struct {
    float arrival_threshold_m;
    float recalc_threshold_m;
    float max_deviation_m;
    float turn_announce_distance_m;
    uint32_t gps_update_interval_ms;
} nav_config_t;

typedef struct {
    float bearing;
    float distance;
    uint8_t reason;
} nav_override_t;

typedef struct {
    uint16_t bearing_deg;
    float distance_m;
    uint8_t turn_type;
    bool is_arrival;
} nav_speech_t;

typedef enum {
    NAV_OK = 0,
    NAV_ERR_NOT_INIT,
    NAV_ERR_NO_ROUTE,
    NAV_ERR_NO_DESTINATION,
    NAV_ERR_CANNOT_ROUTE
} nav_status_code_t;

nav_status_code_t nav_engine_init(const nav_config_t *config);
nav_status_code_t nav_engine_set_destination(double lat, double lon,
                                              const char *label);
nav_status_code_t nav_engine_get_status(nav_status_t *out);
nav_status_code_t nav_engine_cancel_route(void);
nav_status_code_t nav_engine_override(const nav_override_t *override);
nav_status_code_t nav_engine_recalc(void);
nav_status_code_t nav_engine_get_next_turn(nav_turn_t *out);
nav_status_code_t nav_engine_deinit(void);

#endif /* NAVIGATION_ENGINE_H */
