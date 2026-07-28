#ifndef DEPTH_PROCESSING_H
#define DEPTH_PROCESSING_H

#include <stdint.h>
#include <stdbool.h>
#include "message_types.h"
#include "ei_runtime.h"

typedef struct {
    float confidence_threshold;
    bool temporal_filter;
    uint8_t temporal_frames;
    float min_valid_depth;
    float max_valid_depth;
} depth_config_t;

typedef enum {
    DEPTH_OK = 0,
    DEPTH_ERR_INIT,
    DEPTH_ERR_INFER,
    DEPTH_ERR_INVALID_PARAM
} depth_status_t;

depth_status_t depth_init(const depth_config_t *config);
depth_status_t depth_process_frame(const uint8_t *rgb_frame,
                                    depth_map_t **out_map);
depth_status_t depth_get_roi_min_dist(uint16_t roi_x, uint16_t roi_y,
                                       uint16_t roi_w, uint16_t roi_h,
                                       float *out_min_m);
depth_status_t depth_set_confidence_threshold(float threshold);
depth_status_t depth_calibrate(void);
depth_status_t depth_deinit(void);

#endif /* DEPTH_PROCESSING_H */
