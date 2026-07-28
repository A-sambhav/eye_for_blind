#ifndef OBJECT_TRACKING_H
#define OBJECT_TRACKING_H

#include <stdint.h>
#include <stdbool.h>
#include "object_detection.h"

#define MAX_TRACKS 64
#define TRACK_STALE_FRAMES 10
#define KALMAN_STATE_DIM 8

typedef struct {
    float state[KALMAN_STATE_DIM];
    float covariance[KALMAN_STATE_DIM][KALMAN_STATE_DIM];
    float measurement_noise;
    float process_noise;
} kalman_state_t;

typedef struct {
    uint32_t track_id;
    bounding_box_t last_box;
    kalman_state_t kalman;
    bounding_box_t predicted_box;
    float velocity_mps[3];
    uint32_t age;
    uint32_t hit_count;
    uint32_t stale_count;
    bool active;
    bool is_moving;
} tracked_object_t;

typedef struct {
    tracked_object_t tracks[MAX_TRACKS];
    uint32_t active_count;
    uint32_t next_id;
    uint32_t timestamp_us;
} tracked_obj_msg_t;

typedef struct {
    float iou_threshold;
    float confidence_threshold;
    uint32_t max_stale_frames;
    uint32_t max_tracks;
    float velocity_threshold_mps;
    bool use_kalman;
    float process_noise;
    float measurement_noise;
} tracker_config_t;

typedef enum {
    TRACKER_OK = 0,
    TRACKER_ERR_INIT,
    TRACKER_ERR_MAX_TRACKS,
    TRACKER_ERR_ASSOCIATION
} tracker_status_t;

tracker_status_t tracker_init(const tracker_config_t *config);
tracker_status_t tracker_process(const detection_list_t *detections,
                                  tracked_obj_msg_t **out_tracks);
tracker_status_t tracker_get_track_by_id(uint32_t track_id,
                                          tracked_object_t *out);
tracker_status_t tracker_get_active_count(uint32_t *out_count);
tracker_status_t tracker_reset(void);
tracker_status_t tracker_deinit(void);

#endif /* OBJECT_TRACKING_H */
