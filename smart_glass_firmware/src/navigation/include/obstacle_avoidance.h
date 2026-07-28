#ifndef OBSTACLE_AVOIDANCE_H
#define OBSTACLE_AVOIDANCE_H

#include <stdint.h>
#include <stdbool.h>
#include "path_planner.h"

typedef enum {
    kAvoidNone,
    kAvoidLeft,
    kAvoidRight,
    kAvoidStop,
    kAvoidSlowDown,
    kAvoidResume
} avoidance_type_t;

typedef struct {
    avoidance_type_t type;
    float bearing_deg;
    float distance_m;
    float duration_ms;
    uint32_t timestamp_us;
    bool emergency;
} avoidance_cmd_t;

typedef struct {
    bool collision_imminent;
    float min_clearance_m;
    float tti_seconds;
    avoidance_type_t active_avoidance;
    uint32_t avoid_count;
    uint32_t resume_count;
} avoid_status_info_t;

typedef struct {
    float safety_margin_m;
    float tti_warning_threshold_s;
    float tti_critical_threshold_s;
    float min_path_clearance_m;
    uint8_t max_consecutive_avoids;
    uint32_t resume_delay_ms;
} avoid_config_t;

typedef enum {
    AVOID_OK = 0,
    AVOID_ERR_NOT_INIT,
    AVOID_ERR_NO_PATH,
    AVOID_ERR_COLLISION
} avoid_status_t;

avoid_status_t obstacle_avoid_init(const avoid_config_t *config);
avoid_status_t obstacle_avoid_process(const path_msg_t *path,
                                       avoidance_cmd_t **out_cmd);
avoid_status_t obstacle_avoid_get_status(avoid_status_info_t *out);
avoid_status_t obstacle_avoid_set_safety_margin(float margin_m);
avoid_status_t obstacle_avoid_clearance_check(float *out_clearance);
avoid_status_t obstacle_avoid_deinit(void);

#endif /* OBSTACLE_AVOIDANCE_H */
