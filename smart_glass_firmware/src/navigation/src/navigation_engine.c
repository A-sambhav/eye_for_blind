#include <string.h>
#include <math.h>
#include "navigation_engine.h"
#include "message_bus.h"
#include "message_types.h"
#include "logging_manager.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#define EARTH_RADIUS_M  6371000.0f
#define PI_F            3.14159265f
#define DEG_TO_RAD_F    (PI_F / 180.0f)
#define RAD_TO_DEG_F    (180.0f / PI_F)
#define DR_ALPHA        0.15f

typedef enum {
    NAV_STATE_IDLE,
    NAV_STATE_ROUTE_ACTIVE,
    NAV_STATE_NAVIGATING,
    NAV_STATE_TURN,
    NAV_STATE_ARRIVED,
    NAV_STATE_OFF_ROUTE,
    NAV_STATE_RECALC
} nav_state_t;

static struct {
    nav_config_t config;
    nav_state_t state;
    nav_status_t status;
    nav_position_t dest_pos;
    bool dest_set;
    SemaphoreHandle_t lock;
    bool initialized;

    nav_position_t last_gps;
    nav_position_t dr_pos;
    bool has_gps;
    float dr_heading_deg;
    uint32_t last_imu_tick;
    uint32_t last_gps_tick;
    uint32_t instruction_count;
    uint32_t last_instruction_tick;
} nav;

static float deg2rad_f(float d) { return d * DEG_TO_RAD_F; }
static float rad2deg_f(float r) { return r * RAD_TO_DEG_F; }

static float wrap_360(float deg)
{
    float r = fmodf(deg, 360.0f);
    return r < 0 ? r + 360.0f : r;
}

static float bearing_diff_deg(float a, float b)
{
    float d = b - a;
    while (d > 180) d -= 360;
    while (d < -180) d += 360;
    return d;
}

static void compute_bearing_distance(double lat1, double lon1,
                                      double lat2, double lon2,
                                      float *bearing, float *distance)
{
    float lat1r = deg2rad_f((float)lat1);
    float lon1r = deg2rad_f((float)lon1);
    float lat2r = deg2rad_f((float)lat2);
    float lon2r = deg2rad_f((float)lon2);
    float dlat = lat2r - lat1r;
    float dlon = lon2r - lon1r;

    if (distance) {
        float a = sinf(dlat * 0.5f) * sinf(dlat * 0.5f) +
                  cosf(lat1r) * cosf(lat2r) *
                  sinf(dlon * 0.5f) * sinf(dlon * 0.5f);
        float c = 2.0f * atan2f(sqrtf(a), sqrtf(1.0f - a));
        *distance = EARTH_RADIUS_M * c;
    }
    if (bearing) {
        float y = sinf(dlon) * cosf(lat2r);
        float x = cosf(lat1r) * sinf(lat2r) -
                  sinf(lat1r) * cosf(lat2r) * cosf(dlon);
        *bearing = wrap_360(rad2deg_f(atan2f(y, x)));
    }
}

static void generate_waypoints(void)
{
    float total_dist;
    compute_bearing_distance(nav.status.current_pos.latitude,
                             nav.status.current_pos.longitude,
                             nav.dest_pos.latitude,
                             nav.dest_pos.longitude,
                             NULL, &total_dist);

    float interval = 5.0f;
    uint32_t n = (uint32_t)(total_dist / interval);
    if (n > NAV_MAX_WAYPOINTS - 1) n = NAV_MAX_WAYPOINTS - 1;
    if (n == 0) n = 1;

    nav.status.waypoint_count = n + 1;
    nav.status.current_waypoint_idx = 1;

    nav.status.waypoints[0] = (nav_waypoint_t){
        .latitude = nav.status.current_pos.latitude,
        .longitude = nav.status.current_pos.longitude,
        .altitude = nav.status.current_pos.altitude,
        .reached = true,
        .arrival_radius_m = nav.config.arrival_threshold_m
    };
    for (uint32_t i = 1; i <= n; i++) {
        float frac = (float)i / (float)n;
        double lat = nav.status.current_pos.latitude +
                     (nav.dest_pos.latitude - nav.status.current_pos.latitude) * frac;
        double lon = nav.status.current_pos.longitude +
                     (nav.dest_pos.longitude - nav.status.current_pos.longitude) * frac;
        nav.status.waypoints[i] = (nav_waypoint_t){
            .latitude = lat,
            .longitude = lon,
            .altitude = nav.status.current_pos.altitude +
                        (nav.dest_pos.altitude - nav.status.current_pos.altitude) * frac,
            .reached = false,
            .arrival_radius_m = nav.config.arrival_threshold_m
        };
    }
    nav.status.waypoints[n] = (nav_waypoint_t){
        .latitude = nav.dest_pos.latitude,
        .longitude = nav.dest_pos.longitude,
        .altitude = nav.dest_pos.altitude,
        .reached = false,
        .arrival_radius_m = nav.config.arrival_threshold_m
    };

    nav.status.total_distance_m = total_dist;
    nav.status.remaining_distance_m = total_dist;
    nav.status.recalc_count = 0;
}

static void check_off_route(void)
{
    if (!nav.dest_set || nav.status.waypoint_count == 0) return;
    nav_waypoint_t *wp = &nav.status.waypoints[nav.status.current_waypoint_idx];

    float brg, dist;
    compute_bearing_distance(nav.status.current_pos.latitude,
                             nav.status.current_pos.longitude,
                             wp->latitude, wp->longitude,
                             &brg, &dist);

    float path_brg;
    compute_bearing_distance(nav.status.waypoints[0].latitude,
                             nav.status.waypoints[0].longitude,
                             nav.dest_pos.latitude, nav.dest_pos.longitude,
                             &path_brg, NULL);

    float dev = fabsf(bearing_diff_deg(brg, path_brg));
    float deviation_m = dist * sinf(deg2rad_f(dev));
    nav.status.deviation_m = deviation_m;
    nav.status.is_off_route = (deviation_m > nav.config.max_deviation_m);

    if (nav.status.is_off_route) {
        log_warn("nav", "Off-route deviation=%.1fm", deviation_m);
    }
}

static void generate_instruction(void)
{
    uint32_t now = xTaskGetTickCount();
    if (now - nav.last_instruction_tick < pdMS_TO_TICKS(3000)) return;

    nav_waypoint_t *wp = &nav.status.waypoints[nav.status.current_waypoint_idx];
    float brg, dist;
    compute_bearing_distance(nav.status.current_pos.latitude,
                             nav.status.current_pos.longitude,
                             wp->latitude, wp->longitude,
                             &brg, &dist);

    nav.status.remaining_distance_m = dist;
    for (uint32_t i = nav.status.current_waypoint_idx; i < nav.status.waypoint_count; i++) {
        nav.status.remaining_distance_m += 0;
    }

    float heading = nav.status.current_pos.heading_deg;
    float angle = bearing_diff_deg(heading, brg);

    nav_turn_t turn;
    turn.bearing_deg = (uint16_t)brg;
    turn.distance_m = dist;
    turn.turn_angle_deg = fabsf(angle);

    if (dist < nav.config.arrival_threshold_m) {
        turn.turn_type = 0;
        turn.is_last_turn = (nav.status.current_waypoint_idx >= nav.status.waypoint_count - 1);
    } else if (angle > 30) {
        turn.turn_type = 2;
        turn.is_last_turn = false;
    } else if (angle < -30) {
        turn.turn_type = 1;
        turn.is_last_turn = false;
    } else if (angle < -160 || angle > 160) {
        turn.turn_type = 3;
        turn.is_last_turn = false;
    } else {
        turn.turn_type = 0;
        turn.is_last_turn = false;
    }

    nav.status.next_turn = turn;
    nav.instruction_count++;
    nav.last_instruction_tick = now;

    nav_speech_t speech;
    speech.bearing_deg = turn.bearing_deg;
    speech.distance_m = turn.distance_m;
    speech.turn_type = turn.turn_type;
    speech.is_arrival = turn.is_last_turn;
    message_bus_publish(MSG_NAV_SPEECH, &speech, sizeof(speech), 2);
}

static void update_waypoints(void)
{
    if (!nav.dest_set || nav.status.waypoint_count == 0) return;

    nav_waypoint_t *wp = &nav.status.waypoints[nav.status.current_waypoint_idx];
    float brg, dist;
    compute_bearing_distance(nav.status.current_pos.latitude,
                             nav.status.current_pos.longitude,
                             wp->latitude, wp->longitude,
                             &brg, &dist);

    if (dist < wp->arrival_radius_m) {
        wp->reached = true;
        nav.status.current_waypoint_idx++;
        log_info("nav", "Waypoint %lu reached", (unsigned long)nav.status.current_waypoint_idx - 1);

        if (nav.status.current_waypoint_idx >= nav.status.waypoint_count) {
            nav.state = NAV_STATE_ARRIVED;
            nav.status.route_active = false;
            log_info("nav", "Destination reached");
        }
    }
}

static void gps_callback(const bus_message_t *msg, void *user_ctx)
{
    (void)user_ctx;
    gps_position_t gps;
    if (msg->payload_size > sizeof(gps)) return;
    memcpy(&gps, msg->payload, msg->payload_size);
    if (!gps.valid) return;

    xSemaphoreTake(nav.lock, portMAX_DELAY);

    nav.last_gps.latitude = gps.latitude;
    nav.last_gps.longitude = gps.longitude;
    nav.last_gps.heading_deg = gps.heading_deg;
    nav.last_gps.speed_mps = gps.speed_mps;
    nav.last_gps.accuracy = gps.hdop;
    nav.last_gps.timestamp_us = gps.timestamp_us;
    nav.last_gps.estimated = false;
    nav.has_gps = true;
    nav.last_gps_tick = xTaskGetTickCount();

    nav.status.current_pos.latitude =
        (1.0 - DR_ALPHA) * gps.latitude + DR_ALPHA * nav.dr_pos.latitude;
    nav.status.current_pos.longitude =
        (1.0 - DR_ALPHA) * gps.longitude + DR_ALPHA * nav.dr_pos.longitude;
    nav.status.current_pos.heading_deg = gps.heading_deg;
    nav.status.current_pos.speed_mps = gps.speed_mps;
    nav.status.current_pos.accuracy = gps.hdop;
    nav.status.current_pos.timestamp_us = gps.timestamp_us;
    nav.status.current_pos.estimated = false;

    if (nav.state == NAV_STATE_ROUTE_ACTIVE || nav.state == NAV_STATE_NAVIGATING) {
        update_waypoints();
        check_off_route();
        generate_instruction();
    }
    xSemaphoreGive(nav.lock);
}

static void imu_callback(const bus_message_t *msg, void *user_ctx)
{
    (void)user_ctx;
    imu_data_t imu;
    if (msg->payload_size > sizeof(imu)) return;
    memcpy(&imu, msg->payload, msg->payload_size);

    xSemaphoreTake(nav.lock, portMAX_DELAY);

    float dt = (imu.timestamp_us - nav.dr_pos.timestamp_us) * 1e-6f;
    if (dt <= 0 || dt > 1.0f) dt = 0.01f;

    nav.dr_heading_deg += imu.gyro_z * dt * RAD_TO_DEG_F;
    nav.dr_heading_deg = wrap_360(nav.dr_heading_deg);

    if (imu.step_detected) {
        float step_len = 0.65f;
        float h = deg2rad_f(nav.dr_heading_deg);
        double dlat = step_len * cosf(h) * RAD_TO_DEG_F / EARTH_RADIUS_M;
        double dlon = step_len * sinf(h) * RAD_TO_DEG_F /
                      (EARTH_RADIUS_M * cosf(deg2rad_f((float)nav.dr_pos.latitude)));
        nav.dr_pos.latitude += dlat;
        nav.dr_pos.longitude += dlon;
        nav.dr_pos.heading_deg = nav.dr_heading_deg;
        nav.dr_pos.timestamp_us = imu.timestamp_us;
        nav.dr_pos.estimated = true;

        if (!nav.has_gps) {
            nav.status.current_pos = nav.dr_pos;
        }
    }
    nav.last_imu_tick = xTaskGetTickCount();
    xSemaphoreGive(nav.lock);
}

static void override_callback(const bus_message_t *msg, void *user_ctx)
{
    (void)user_ctx;
    nav_override_t ov;
    if (msg->payload_size > sizeof(ov)) return;
    memcpy(&ov, msg->payload, msg->payload_size);

    xSemaphoreTake(nav.lock, portMAX_DELAY);
    float h = deg2rad_f(nav.status.current_pos.heading_deg + ov.bearing);
    double dlat = ov.distance * cosf(h) * RAD_TO_DEG_F / EARTH_RADIUS_M;
    double dlon = ov.distance * sinf(h) * RAD_TO_DEG_F /
                  (EARTH_RADIUS_M * cosf(deg2rad_f((float)nav.status.current_pos.latitude)));
    double new_lat = nav.status.current_pos.latitude + dlat;
    double new_lon = nav.status.current_pos.longitude + dlon;

    nav_waypoint_t avoid_wp;
    avoid_wp.latitude = new_lat;
    avoid_wp.longitude = new_lon;
    avoid_wp.altitude = nav.status.current_pos.altitude;
    avoid_wp.reached = false;
    avoid_wp.arrival_radius_m = nav.config.arrival_threshold_m;

    if (nav.status.waypoint_count < NAV_MAX_WAYPOINTS) {
        memmove(&nav.status.waypoints[nav.status.current_waypoint_idx + 1],
                &nav.status.waypoints[nav.status.current_waypoint_idx],
                (nav.status.waypoint_count - nav.status.current_waypoint_idx) * sizeof(nav_waypoint_t));
        nav.status.waypoints[nav.status.current_waypoint_idx] = avoid_wp;
        nav.status.waypoint_count++;
    }

    nav.state = NAV_STATE_ROUTE_ACTIVE;
    nav.status.recalc_count++;
    log_info("nav", "Override applied bearing=%.0f dist=%.1f reason=%u",
             ov.bearing, ov.distance, ov.reason);
    xSemaphoreGive(nav.lock);
}

nav_status_code_t nav_engine_init(const nav_config_t *config)
{
    if (config == NULL) return NAV_ERR_NOT_INIT;
    memset(&nav, 0, sizeof(nav));
    nav.config = *config;
    if (nav.config.arrival_threshold_m <= 0) nav.config.arrival_threshold_m = 3.0f;
    if (nav.config.max_deviation_m <= 0) nav.config.max_deviation_m = 5.0f;
    if (nav.config.recalc_threshold_m <= 0) nav.config.recalc_threshold_m = 10.0f;
    if (nav.config.turn_announce_distance_m <= 0) nav.config.turn_announce_distance_m = 5.0f;
    if (nav.config.gps_update_interval_ms == 0) nav.config.gps_update_interval_ms = 200;

    nav.lock = xSemaphoreCreateMutex();
    if (nav.lock == NULL) return NAV_ERR_NOT_INIT;
    nav.state = NAV_STATE_IDLE;

    message_bus_subscribe(MSG_GPS_POSITION, gps_callback, NULL);
    message_bus_subscribe(MSG_IMU_DATA, imu_callback, NULL);
    message_bus_subscribe(MSG_NAV_OVERRIDE, override_callback, NULL);

    nav.initialized = true;
    log_info("nav", "Initialized arrival=%.1f dev=%.1f recalc=%.1f",
             nav.config.arrival_threshold_m, nav.config.max_deviation_m,
             nav.config.recalc_threshold_m);
    return NAV_OK;
}

nav_status_code_t nav_engine_set_destination(double lat, double lon,
                                              const char *label)
{
    if (!nav.initialized) return NAV_ERR_NOT_INIT;
    xSemaphoreTake(nav.lock, portMAX_DELAY);

    nav.dest_pos.latitude = lat;
    nav.dest_pos.longitude = lon;
    nav.dest_pos.altitude = nav.status.current_pos.altitude;
    nav.dest_set = true;

    generate_waypoints();
    nav.state = NAV_STATE_ROUTE_ACTIVE;
    nav.status.route_active = true;
    nav.instruction_count = 0;
    nav.last_instruction_tick = 0;

    log_info("nav", "Destination set: %s (%.6f, %.6f), %.0f waypoints",
             label ? label : "?", lat, lon,
             (double)nav.status.waypoint_count);
    xSemaphoreGive(nav.lock);
    return NAV_OK;
}

nav_status_code_t nav_engine_get_status(nav_status_t *out)
{
    if (!nav.initialized || out == NULL) return NAV_ERR_NOT_INIT;
    xSemaphoreTake(nav.lock, portMAX_DELAY);
    *out = nav.status;
    xSemaphoreGive(nav.lock);
    return NAV_OK;
}

nav_status_code_t nav_engine_cancel_route(void)
{
    if (!nav.initialized) return NAV_ERR_NOT_INIT;
    xSemaphoreTake(nav.lock, portMAX_DELAY);
    nav.state = NAV_STATE_IDLE;
    nav.status.route_active = false;
    nav.dest_set = false;
    nav.status.waypoint_count = 0;
    nav.status.current_waypoint_idx = 0;
    log_info("nav", "Route cancelled");
    xSemaphoreGive(nav.lock);
    return NAV_OK;
}

nav_status_code_t nav_engine_override(const nav_override_t *override)
{
    if (!nav.initialized || override == NULL) return NAV_ERR_NOT_INIT;
    xSemaphoreTake(nav.lock, portMAX_DELAY);
    nav_override_t ov = *override;
    float h = deg2rad_f(nav.status.current_pos.heading_deg + ov.bearing);
    double dlat = ov.distance * cosf(h) * RAD_TO_DEG_F / EARTH_RADIUS_M;
    double dlon = ov.distance * sinf(h) * RAD_TO_DEG_F /
                  (EARTH_RADIUS_M * cosf(deg2rad_f((float)nav.status.current_pos.latitude)));

    nav_waypoint_t avoid_wp;
    avoid_wp.latitude = nav.status.current_pos.latitude + dlat;
    avoid_wp.longitude = nav.status.current_pos.longitude + dlon;
    avoid_wp.altitude = nav.status.current_pos.altitude;
    avoid_wp.reached = false;
    avoid_wp.arrival_radius_m = nav.config.arrival_threshold_m;

    if (nav.status.waypoint_count < NAV_MAX_WAYPOINTS) {
        memmove(&nav.status.waypoints[nav.status.current_waypoint_idx + 1],
                &nav.status.waypoints[nav.status.current_waypoint_idx],
                (nav.status.waypoint_count - nav.status.current_waypoint_idx) * sizeof(nav_waypoint_t));
        nav.status.waypoints[nav.status.current_waypoint_idx] = avoid_wp;
        nav.status.waypoint_count++;
    }
    nav.status.recalc_count++;
    log_info("nav", "Override received bearing=%.0f dist=%.1f reason=%u",
             ov.bearing, ov.distance, ov.reason);
    xSemaphoreGive(nav.lock);
    return NAV_OK;
}

nav_status_code_t nav_engine_recalc(void)
{
    if (!nav.initialized) return NAV_ERR_NOT_INIT;
    xSemaphoreTake(nav.lock, portMAX_DELAY);
    if (nav.dest_set) {
        generate_waypoints();
        nav.state = NAV_STATE_ROUTE_ACTIVE;
        nav.status.route_active = true;
        nav.status.is_off_route = false;
        nav.status.recalc_count++;
        log_info("nav", "Route recalculated");
    }
    xSemaphoreGive(nav.lock);
    return NAV_OK;
}

nav_status_code_t nav_engine_get_next_turn(nav_turn_t *out)
{
    if (!nav.initialized || out == NULL) return NAV_ERR_NOT_INIT;
    xSemaphoreTake(nav.lock, portMAX_DELAY);
    *out = nav.status.next_turn;
    xSemaphoreGive(nav.lock);
    return NAV_OK;
}

nav_status_code_t nav_engine_deinit(void)
{
    nav.initialized = false;
    return NAV_OK;
}
