#ifndef HAZARD_PRIORITY_H
#define HAZARD_PRIORITY_H

#include <stdint.h>
#include <stdbool.h>
#include "scene_understanding.h"

#define HAZARD_PRIORITY_QUEUE_SIZE 16

typedef struct {
    hazard_type_t type;
    float distance_m;
    float relative_velocity_mps;
    float tti_seconds;
    uint8_t severity;
    uint8_t confidence;
    uint16_t bearing_deg;
    float pos_x, pos_y, pos_z;
    bool is_moving;
    uint32_t timestamp_us;
    uint32_t hazard_id;
} hazard_event_t;

typedef struct {
    hazard_event_t events[HAZARD_PRIORITY_QUEUE_SIZE];
    uint8_t count;
} hazard_event_msg_t;

typedef struct {
    uint8_t type_weights[12];
    float max_hazard_distance;
    float min_severity_threshold;
    uint8_t max_events_per_frame;
    uint32_t dedup_window_ms;
} hazard_config_t;

typedef enum {
    HAZARD_OK = 0,
    HAZARD_ERR_NOT_INIT,
    HAZARD_ERR_QUEUE_FULL
} hazard_status_t;

hazard_status_t hazard_priority_init(const hazard_config_t *config);
hazard_status_t hazard_priority_process(const hazard_list_t *hazards,
                                         hazard_event_msg_t **out_event);
hazard_status_t hazard_priority_get_top_n(uint8_t n,
                                           hazard_event_t *out_events,
                                           uint8_t *out_count);
hazard_status_t hazard_priority_set_severity_weights(hazard_type_t type,
                                                      uint8_t weight);
hazard_status_t hazard_priority_clear(void);
hazard_status_t hazard_priority_deinit(void);

#endif /* HAZARD_PRIORITY_H */
