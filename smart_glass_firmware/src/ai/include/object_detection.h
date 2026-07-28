#ifndef OBJECT_DETECTION_H
#define OBJECT_DETECTION_H

#include <stdint.h>
#include <stdbool.h>
#include "message_types.h"

#define OD_CLASSES 80

typedef struct {
    float confidence_threshold;
    float nms_iou_threshold;
    uint8_t max_detections;
    uint16_t input_width;
    uint16_t input_height;
    bool compute_3d_pos;
    uint8_t enabled_classes[OD_CLASSES / 8];
} od_config_t;

typedef enum {
    OD_OK = 0,
    OD_ERR_INIT,
    OD_ERR_INFER,
    OD_ERR_NO_BOXES
} od_status_t;

od_status_t object_detection_init(const od_config_t *config);
od_status_t object_detection_process(const uint8_t *frame,
                                      const depth_map_t *depth,
                                      detection_list_t **out_list);
od_status_t object_detection_set_confidence_threshold(float threshold);
od_status_t object_detection_set_nms_iou_threshold(float iou);
od_status_t object_detection_get_stats(uint32_t *total_detected,
                                        uint32_t *avg_latency);
od_status_t object_detection_deinit(void);

#endif /* OBJECT_DETECTION_H */
