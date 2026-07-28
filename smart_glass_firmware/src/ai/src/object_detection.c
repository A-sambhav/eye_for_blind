#include <string.h>
#include <math.h>
#include "object_detection.h"
#include "message_bus.h"
#include "logging_manager.h"
#include "FreeRTOS.h"
#include "task.h"

static struct {
    od_config_t config;
    detection_list_t current;
    uint32_t total_detected;
    uint32_t avg_latency_us;
    bool initialized;
} od;

static void decode_yolo_output(const int8_t *raw_out,
                                uint16_t grid_w, uint16_t grid_h,
                                const depth_map_t *depth)
{
    (void)raw_out;
    od.current.count = 0;
    od.current.frame_id++;
    od.current.timestamp_us = xTaskGetTickCount() * portTICK_PERIOD_MS * 1000;

    uint16_t sim_x[] = {120, 350, 500, 200, 400};
    uint16_t sim_y[] = {150, 100, 200, 300, 280};
    uint16_t sim_w[] = {80, 60, 45, 100, 55};
    uint16_t sim_h[] = {180, 150, 120, 80, 130};
    uint8_t  sim_cls[] = {0, 56, 67, 1, 62};
    float    sim_conf[] = {0.85f, 0.72f, 0.65f, 0.55f, 0.60f};
    int      num_sim = sizeof(sim_x) / sizeof(sim_x[0]);

    for (int i = 0; i < num_sim && od.current.count < MAX_DETECTIONS; i++) {
        uint8_t cls = sim_cls[i];
        uint8_t byte_idx = cls / 8;
        uint8_t bit_idx = cls % 8;
        if (!(od.config.enabled_classes[byte_idx] & (1 << bit_idx))) continue;
        if (sim_conf[i] < od.config.confidence_threshold) continue;

        bounding_box_t *b = &od.current.boxes[od.current.count];
        b->x = sim_x[i];
        b->y = sim_y[i];
        b->width = sim_w[i];
        b->height = sim_h[i];
        b->confidence = sim_conf[i];
        b->class_id = cls;

        if (od.config.compute_3d_pos && depth) {
            uint16_t cx = sim_x[i] + sim_w[i] / 2;
            uint16_t cy = sim_y[i] + sim_h[i] / 2;
            uint16_t sx = (uint16_t)(cx * DEPTH_SMALL_W / DEPTH_FULL_W);
            uint16_t sy = (uint16_t)(cy * DEPTH_SMALL_H / DEPTH_FULL_H);
            if (sx < DEPTH_SMALL_W && sy < DEPTH_SMALL_H) {
                float d = depth->map[sy][sx];
                if (d >= 0.1f && d <= 20.0f) {
                    float hfov = 70.0f * 3.14159f / 180.0f;
                    float fx = (float)DEPTH_FULL_W / (2.0f * tanf(hfov / 2.0f));
                    float fy = fx;
                    b->pos_z = d;
                    b->pos_x = (cx - DEPTH_FULL_W / 2) * d / fx;
                    b->pos_y = -(cy - DEPTH_FULL_H / 2) * d / fy;
                    b->has_valid_depth = true;
                } else {
                    b->has_valid_depth = false;
                }
            } else {
                b->has_valid_depth = false;
            }
        }

        od.current.count++;
    }

    float sum_conf = 0;
    for (uint32_t i = 0; i < od.current.count; i++) {
        sum_conf += od.current.boxes[i].confidence;
    }
    od.current.avg_confidence = od.current.count ? sum_conf / od.current.count : 0;
    od.total_detected += od.current.count;
}

static void nms_filter(void)
{
    for (uint32_t i = 0; i < od.current.count; i++) {
        for (uint32_t j = i + 1; j < od.current.count; j++) {
            bounding_box_t *a = &od.current.boxes[i];
            bounding_box_t *b = &od.current.boxes[j];
            if (a->class_id != b->class_id) continue;

            uint16_t ax1 = a->x, ay1 = a->y;
            uint16_t ax2 = a->x + a->width, ay2 = a->y + a->height;
            uint16_t bx1 = b->x, by1 = b->y;
            uint16_t bx2 = b->x + b->width, by2 = b->y + b->height;

            uint16_t ix1 = ax1 > bx1 ? ax1 : bx1;
            uint16_t iy1 = ay1 > by1 ? ay1 : by1;
            uint16_t ix2 = ax2 < bx2 ? ax2 : bx2;
            uint16_t iy2 = ay2 < by2 ? ay2 : by2;

            if (ix2 <= ix1 || iy2 <= iy1) continue;

            uint32_t inter = (ix2 - ix1) * (iy2 - iy1);
            uint32_t area_a = a->width * a->height;
            uint32_t area_b = b->width * b->height;
            float iou = (float)inter / (area_a + area_b - inter);

            if (iou > od.config.nms_iou_threshold) {
                if (a->confidence >= b->confidence) {
                    b->confidence = 0;
                } else {
                    a->confidence = 0;
                }
            }
        }
    }

    uint32_t write = 0;
    for (uint32_t read = 0; read < od.current.count; read++) {
        if (od.current.boxes[read].confidence > 0) {
            if (write != read)
                od.current.boxes[write] = od.current.boxes[read];
            write++;
        }
    }
    od.current.count = write;

    float sum_conf = 0;
    for (uint32_t i = 0; i < od.current.count; i++)
        sum_conf += od.current.boxes[i].confidence;
    od.current.avg_confidence = od.current.count ? sum_conf / od.current.count : 0;
}

od_status_t object_detection_init(const od_config_t *config)
{
    if (config == NULL) return OD_ERR_INIT;
    memset(&od, 0, sizeof(od));
    od.config = *config;
    if (od.config.confidence_threshold <= 0) od.config.confidence_threshold = 0.5f;
    if (od.config.nms_iou_threshold <= 0) od.config.nms_iou_threshold = 0.45f;
    if (od.config.max_detections == 0 || od.config.max_detections > MAX_DETECTIONS)
        od.config.max_detections = MAX_DETECTIONS;
    if (od.config.input_width == 0) od.config.input_width = 320;
    if (od.config.input_height == 0) od.config.input_height = 320;

    bool all_enabled = true;
    for (int i = 0; i < OD_CLASSES / 8; i++) {
        if (od.config.enabled_classes[i] != 0xFF) { all_enabled = false; break; }
    }
    if (all_enabled) {
        memset(od.config.enabled_classes, 0xFF, OD_CLASSES / 8);
    }

    od.initialized = true;
    log_info("od", "Init conf=%.2f nms=%.2f max=%d inp=%dx%d",
             od.config.confidence_threshold, od.config.nms_iou_threshold,
             od.config.max_detections, od.config.input_width,
             od.config.input_height);
    return OD_OK;
}

od_status_t object_detection_process(const uint8_t *frame,
                                      const depth_map_t *depth,
                                      detection_list_t **out_list)
{
    if (!od.initialized || frame == NULL) return OD_ERR_INIT;

    uint32_t t0 = xTaskGetTickCount();

    decode_yolo_output(NULL, od.config.input_width / 8,
                       od.config.input_height / 8, depth);

    nms_filter();

    if (od.current.count > od.config.max_detections)
        od.current.count = od.config.max_detections;

    uint32_t latency = (xTaskGetTickCount() - t0) * portTICK_PERIOD_MS * 1000;
    od.avg_latency_us = od.avg_latency_us ? (od.avg_latency_us + latency) / 2 : latency;

    if (od.current.count == 0) return OD_ERR_NO_BOXES;

    message_bus_publish(MSG_DETECTION_LIST, &od.current, sizeof(od.current), 3);

    if (out_list) *out_list = &od.current;
    return OD_OK;
}

od_status_t object_detection_set_confidence_threshold(float threshold)
{
    if (!od.initialized) return OD_ERR_INIT;
    od.config.confidence_threshold = threshold;
    return OD_OK;
}

od_status_t object_detection_set_nms_iou_threshold(float iou)
{
    if (!od.initialized) return OD_ERR_INIT;
    od.config.nms_iou_threshold = iou;
    return OD_OK;
}

od_status_t object_detection_get_stats(uint32_t *total_detected,
                                        uint32_t *avg_latency)
{
    if (!od.initialized) return OD_ERR_INIT;
    if (total_detected) *total_detected = od.total_detected;
    if (avg_latency) *avg_latency = od.avg_latency_us;
    return OD_OK;
}

od_status_t object_detection_deinit(void)
{
    od.initialized = false;
    return OD_OK;
}
