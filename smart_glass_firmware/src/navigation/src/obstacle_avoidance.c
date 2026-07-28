#include <string.h>
#include <math.h>
#include "obstacle_avoidance.h"
#include "navigation_engine.h"
#include "message_bus.h"
#include "message_types.h"
#include "logging_manager.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#define PI_F 3.14159265f
#define DEG_TO_RAD_F (PI_F / 180.0f)
#define RAD_TO_DEG_F (180.0f / PI_F)

#define TTI_INFINITE 999.0f
#define SCAN_BEARING_STEP 10

typedef enum {
    AVOID_STATE_MONITOR,
    AVOID_STATE_AVOIDING,
    AVOID_STATE_RESUMING
} avoid_fsm_state_t;

static struct {
    avoid_config_t config;
    avoid_fsm_state_t state;
    SemaphoreHandle_t lock;
    uint32_t consecutive_avoids;
    uint32_t last_avoid_tick;
    uint32_t last_resume_tick;
    avoidance_cmd_t last_cmd;
    bool initialized;
    uint32_t avoid_count;
    uint32_t resume_count;
    bool collision_imminent;
    float min_clearance;
    float tti_value;
    path_msg_t current_path;
    bool has_path;
} av;

static uint32_t now_ms(void)
{
    return xTaskGetTickCount() * portTICK_PERIOD_MS;
}

static float compute_tti(float distance_m, float relative_speed_mps)
{
    if (distance_m <= 0) return 0;
    float v = relative_speed_mps;
    if (v < 0.1f) v = 0.1f;
    return distance_m / v;
}

static float path_clearance(const path_msg_t *path, tracked_obj_msg_t *tracks)
{
    if (!path || path->count < 1 || !tracks) return 999.0f;

    float min_c = 999.0f;
    for (uint32_t ti = 0; ti < tracks->active_count && ti < MAX_TRACKS; ti++) {
        const tracked_object_t *trk = &tracks->tracks[ti];
        if (!trk->active) continue;

        float ttx = trk->last_box.pos_x;
        float tty = trk->last_box.pos_y;

        for (uint32_t wi = 0; wi < path->count; wi++) {
            float dx = path->waypoints[wi].x - ttx;
            float dy = path->waypoints[wi].y - tty;
            float d = sqrtf(dx * dx + dy * dy);
            if (d < min_c) min_c = d;
        }
    }
    return min_c;
}

static float detect_collision_tti(tracked_obj_msg_t *tracks,
                                   const path_msg_t *path)
{
    if (!tracks || !path || path->count < 1) return TTI_INFINITE;

    float min_tti = TTI_INFINITE;
    float heading = 0;
    if (path->waypoints[1].x > path->waypoints[0].x ||
        path->waypoints[1].y > path->waypoints[0].y) {
        heading = atan2f(path->waypoints[1].y - path->waypoints[0].y,
                         path->waypoints[1].x - path->waypoints[0].x);
    }

    for (uint32_t ti = 0; ti < tracks->active_count && ti < MAX_TRACKS; ti++) {
        const tracked_object_t *trk = &tracks->tracks[ti];
        if (!trk->active) continue;

        float ttx = trk->last_box.pos_x;
        float tty = trk->last_box.pos_y;

        float dist = sqrtf(ttx * ttx + tty * tty);

        float obj_angle = atan2f(tty, ttx);
        float angle_diff = fabsf(obj_angle - heading);
        if (angle_diff > PI_F) angle_diff = 2 * PI_F - angle_diff;
        if (angle_diff > PI_F / 3) continue;

        float rel_v = sqrtf(trk->velocity_mps[0] * trk->velocity_mps[0] +
                            trk->velocity_mps[1] * trk->velocity_mps[1]);
        float tti = compute_tti(dist, rel_v);
        if (tti < min_tti) min_tti = tti;
    }
    return min_tti;
}

static avoidance_type_t compute_avoidance_vector(float *out_bearing)
{
    float best_bearing = 0;
    float best_space = 0;
    avoidance_type_t best_type = kAvoidNone;

    for (int bearing = -90; bearing <= 90; bearing += SCAN_BEARING_STEP) {
        if (bearing == 0) continue;
        float space = 5.0f - fabsf((float)bearing) * 0.03f;
        if (space > best_space) {
            best_space = space;
            best_bearing = (float)bearing;
            best_type = (bearing < 0) ? kAvoidLeft : kAvoidRight;
        }
    }

    if (av.consecutive_avoids >= av.config.max_consecutive_avoids) {
        best_type = kAvoidStop;
        best_bearing = 0;
    }

    if (out_bearing) *out_bearing = best_bearing;
    return best_type;
}

static void publish_avoidance(avoidance_type_t type, float bearing_deg,
                               float distance_m, bool emergency)
{
    av.last_cmd.type = type;
    av.last_cmd.bearing_deg = bearing_deg;
    av.last_cmd.distance_m = distance_m;
    av.last_cmd.duration_ms = 2000;
    av.last_cmd.timestamp_us = now_ms() * 1000;
    av.last_cmd.emergency = emergency;
    av.last_avoid_tick = now_ms();

    nav_override_t ov;
    ov.bearing = bearing_deg;
    ov.distance = distance_m;
    ov.reason = (uint8_t)type;
    message_bus_publish(MSG_NAV_OVERRIDE, &ov, sizeof(ov), 3);
}

static void process_avoidance(void)
{
    if (!av.has_path) return;

    uint32_t now = now_ms();

    switch (av.state) {
    case AVOID_STATE_MONITOR: {
        av.collision_imminent = false;
        av.tti_value = TTI_INFINITE;

        float clearance = av.config.min_path_clearance_m;
        av.min_clearance = clearance;

        if (av.tti_value < av.config.tti_critical_threshold_s) {
            av.state = AVOID_STATE_AVOIDING;
            av.collision_imminent = true;
            av.consecutive_avoids = 0;

            float bearing;
            avoidance_type_t at = compute_avoidance_vector(&bearing);
            bool emergency = (av.tti_value < av.config.tti_critical_threshold_s * 0.5f);
            publish_avoidance(at, bearing, clearance, emergency);
            av.avoid_count++;
            av.consecutive_avoids++;
            log_warn("avoid", "Collision TTI=%.1fs type=%d", av.tti_value, at);
        } else if (av.tti_value < av.config.tti_warning_threshold_s) {
            float bearing;
            avoidance_type_t at = compute_avoidance_vector(&bearing);
            publish_avoidance(at, bearing, clearance, false);
        }
        break;
    }

    case AVOID_STATE_AVOIDING: {
        float tti = av.tti_value;
        if (tti > av.config.tti_warning_threshold_s * 1.5f &&
            now - av.last_avoid_tick > av.config.resume_delay_ms) {
            publish_avoidance(kAvoidResume, 0, 0, false);
            av.state = AVOID_STATE_RESUMING;
            av.last_resume_tick = now;
            av.resume_count++;
            log_info("avoid", "Path resuming (TTI=%.1fs)", tti);
        } else if (tti < av.config.tti_critical_threshold_s) {
            av.consecutive_avoids++;
            if (av.consecutive_avoids >= av.config.max_consecutive_avoids) {
                publish_avoidance(kAvoidStop, 0, 0, true);
                log_error("avoid", "Emergency stop — consecutive avoids exceeded");
            } else {
                float bearing;
                avoidance_type_t at = compute_avoidance_vector(&bearing);
                publish_avoidance(at, bearing, av.min_clearance,
                                  tti < av.config.tti_critical_threshold_s * 0.5f);
            }
        }
        break;
    }

    case AVOID_STATE_RESUMING: {
        float tti = av.tti_value;
        if (tti > av.config.tti_warning_threshold_s * 2.0f &&
            now - av.last_resume_tick > av.config.resume_delay_ms) {
            publish_avoidance(kAvoidNone, 0, 0, false);
            av.state = AVOID_STATE_MONITOR;
            av.consecutive_avoids = 0;
            log_info("avoid", "Path clear, resuming normal monitor");
        } else if (tti < av.config.tti_critical_threshold_s) {
            av.state = AVOID_STATE_AVOIDING;
            float bearing;
            avoidance_type_t at = compute_avoidance_vector(&bearing);
            publish_avoidance(at, bearing, av.min_clearance, true);
        }
        break;
    }
    }
}

static void path_callback(const bus_message_t *msg, void *user_ctx)
{
    (void)user_ctx;
    path_msg_t p;
    if (msg->payload_size > sizeof(p)) return;
    memcpy(&p, msg->payload, msg->payload_size);

    xSemaphoreTake(av.lock, portMAX_DELAY);
    av.current_path = p;
    av.has_path = true;
    xSemaphoreGive(av.lock);
}

static void tracked_callback(const bus_message_t *msg, void *user_ctx)
{
    (void)user_ctx;
    tracked_obj_msg_t tracks;
    if (msg->payload_size > sizeof(tracks)) return;
    memcpy(&tracks, msg->payload, msg->payload_size);

    xSemaphoreTake(av.lock, portMAX_DELAY);
    if (av.has_path) {
        av.min_clearance = path_clearance(&av.current_path, &tracks);
        av.tti_value = detect_collision_tti(&tracks, &av.current_path);
        process_avoidance();
    }
    xSemaphoreGive(av.lock);
}

avoid_status_t obstacle_avoid_init(const avoid_config_t *config)
{
    if (config == NULL) return AVOID_ERR_NOT_INIT;

    memset(&av, 0, sizeof(av));
    av.config = *config;
    if (av.config.safety_margin_m <= 0) av.config.safety_margin_m = 0.5f;
    if (av.config.tti_warning_threshold_s <= 0) av.config.tti_warning_threshold_s = 3.0f;
    if (av.config.tti_critical_threshold_s <= 0) av.config.tti_critical_threshold_s = 1.0f;
    if (av.config.min_path_clearance_m <= 0) av.config.min_path_clearance_m = 0.8f;
    if (av.config.max_consecutive_avoids == 0) av.config.max_consecutive_avoids = 5;
    if (av.config.resume_delay_ms == 0) av.config.resume_delay_ms = 2000;

    av.lock = xSemaphoreCreateMutex();
    if (av.lock == NULL) return AVOID_ERR_NOT_INIT;
    av.state = AVOID_STATE_MONITOR;

    message_bus_subscribe(MSG_PATH, path_callback, NULL);
    message_bus_subscribe(MSG_TRACKED_OBJECTS, tracked_callback, NULL);

    av.initialized = true;
    log_info("avoid", "Initialized TTI_warn=%.1f TTI_crit=%.1f margin=%.1f "
             "clear=%.1f max_avoid=%u resume=%u",
             av.config.tti_warning_threshold_s,
             av.config.tti_critical_threshold_s,
             av.config.safety_margin_m,
             av.config.min_path_clearance_m,
             av.config.max_consecutive_avoids,
             av.config.resume_delay_ms);
    return AVOID_OK;
}

avoid_status_t obstacle_avoid_process(const path_msg_t *path,
                                       avoidance_cmd_t **out_cmd)
{
    if (!av.initialized) return AVOID_ERR_NOT_INIT;
    xSemaphoreTake(av.lock, portMAX_DELAY);
    if (path) {
        av.current_path = *path;
        av.has_path = true;
    }
    if (out_cmd) {
        *out_cmd = (av.state == AVOID_STATE_MONITOR) ? NULL : &av.last_cmd;
    }
    xSemaphoreGive(av.lock);
    return AVOID_OK;
}

avoid_status_t obstacle_avoid_get_status(avoid_status_info_t *out)
{
    if (!av.initialized || out == NULL) return AVOID_ERR_NOT_INIT;
    xSemaphoreTake(av.lock, portMAX_DELAY);
    out->collision_imminent = av.collision_imminent;
    out->min_clearance_m = av.min_clearance;
    out->tti_seconds = av.tti_value;
    out->active_avoidance = (av.state == AVOID_STATE_MONITOR) ? kAvoidNone : av.last_cmd.type;
    out->avoid_count = av.avoid_count;
    out->resume_count = av.resume_count;
    xSemaphoreGive(av.lock);
    return AVOID_OK;
}

avoid_status_t obstacle_avoid_set_safety_margin(float margin_m)
{
    if (!av.initialized) return AVOID_ERR_NOT_INIT;
    if (margin_m <= 0) return AVOID_ERR_NOT_INIT;
    xSemaphoreTake(av.lock, portMAX_DELAY);
    av.config.safety_margin_m = margin_m;
    xSemaphoreGive(av.lock);
    return AVOID_OK;
}

avoid_status_t obstacle_avoid_clearance_check(float *out_clearance)
{
    if (!av.initialized) return AVOID_ERR_NOT_INIT;
    xSemaphoreTake(av.lock, portMAX_DELAY);
    if (out_clearance) *out_clearance = av.min_clearance;
    xSemaphoreGive(av.lock);
    return AVOID_OK;
}

avoid_status_t obstacle_avoid_deinit(void)
{
    av.initialized = false;
    return AVOID_OK;
}
