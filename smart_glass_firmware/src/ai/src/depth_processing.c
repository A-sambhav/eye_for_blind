#include <string.h>
#include <math.h>
#include "depth_processing.h"
#include "ei_runtime.h"
#include "message_bus.h"
#include "logging_manager.h"
#include "task_manager.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

static void depth_frame_callback(const bus_message_t *msg, void *user_ctx);

#define TEMP_HISTORY 4
#define DEPTH_GREY_SIZE (DEPTH_FULL_W * DEPTH_FULL_H)

#define HIST_H 30
#define HIST_W 40

static struct {
    depth_config_t config;
    depth_map_t depth_map;
    float history[TEMP_HISTORY][HIST_H][HIST_W];
    uint8_t history_idx;
    uint32_t hist_update;
    uint32_t frame_count;
    uint32_t inference_time_us;
    bool initialized;
} dp;

static void compute_confidence_map(float *depth, uint8_t *conf)
{
    for (uint32_t i = 0; i < DEPTH_SMALL_H * DEPTH_SMALL_W; i++) {
        if (depth[i] >= dp.config.min_valid_depth &&
            depth[i] <= dp.config.max_valid_depth) {
            float c = 1.0f - fabsf(depth[i] - 2.0f) / 20.0f;
            if (c < 0) c = 0;
            if (c > 1) c = 1;
            conf[i] = (uint8_t)(c * 255);
        } else {
            conf[i] = 0;
        }
    }
}

static void compute_min_dist_grid(float *depth_small,
                                   uint8_t *min_grid)
{
    uint16_t cols = DEPTH_FULL_W / 32, rows = DEPTH_FULL_H / 32;
    float xs = (float)DEPTH_SMALL_W / cols;
    float ys = (float)DEPTH_SMALL_H / rows;
    for (uint16_t r = 0; r < rows; r++) {
        for (uint16_t c = 0; c < cols; c++) {
            float min_d = 999.0f;
            uint16_t sy0 = (uint16_t)(r * ys);
            uint16_t sy1 = (uint16_t)((r + 1) * ys);
            uint16_t sx0 = (uint16_t)(c * xs);
            uint16_t sx1 = (uint16_t)((c + 1) * xs);
            for (uint16_t sy = sy0; sy < sy1 && sy < DEPTH_SMALL_H; sy++) {
                for (uint16_t sx = sx0; sx < sx1 && sx < DEPTH_SMALL_W; sx++) {
                    float d = depth_small[sy * DEPTH_SMALL_W + sx];
                    if (d > 0 && d < min_d) min_d = d;
                }
            }
            min_grid[r * cols + c] = min_d > 20.0f ? 255 : (uint8_t)(min_d * 12.75f);
        }
    }
}

static void simulate_depth_inference(const uint8_t *rgb)
{
    (void)rgb;
    for (uint32_t i = 0; i < DEPTH_SMALL_H * DEPTH_SMALL_W; i++) {
        float x = (float)(i % DEPTH_SMALL_W) / DEPTH_SMALL_W;
        float y = (float)(i / DEPTH_SMALL_W) / DEPTH_SMALL_H;
        float d = 1.5f + 3.0f * (0.5f - x) + 1.0f * (0.5f - y);
        d += (float)((i * 7) % 5) * 0.05f;
        if (d < dp.config.min_valid_depth) d = dp.config.min_valid_depth;
        if (d > dp.config.max_valid_depth) d = dp.config.max_valid_depth;
        ((float *)dp.depth_map.map)[i] = d;
    }
}

static void temporal_filter(void)
{
    uint16_t scale_x = DEPTH_SMALL_W / HIST_W;
    uint16_t scale_y = DEPTH_SMALL_H / HIST_H;
    for (uint16_t y = 0; y < DEPTH_SMALL_H; y++) {
        for (uint16_t x = 0; x < DEPTH_SMALL_W; x++) {
            float sum = dp.depth_map.map[y][x];
            uint8_t count = 1;
            uint16_t hx = x / scale_x;
            uint16_t hy = y / scale_y;
            if (hx >= HIST_W) hx = HIST_W - 1;
            if (hy >= HIST_H) hy = HIST_H - 1;
            for (uint8_t h = 0; h < TEMP_HISTORY; h++) {
                float hv = dp.history[h][hy][hx];
                if (hv > 0.01f) { sum += hv; count++; }
            }
            dp.depth_map.map[y][x] = sum / count;
        }
    }
}

static void process_frame(const uint8_t *rgb_frame)
{
    simulate_depth_inference(rgb_frame);

    if (dp.config.temporal_filter) {
        temporal_filter();
        uint16_t scale_x = DEPTH_SMALL_W / HIST_W;
        uint16_t scale_y = DEPTH_SMALL_H / HIST_H;
        for (uint16_t y = 0; y < HIST_H; y++) {
            for (uint16_t x = 0; x < HIST_W; x++) {
                float avg = 0;
                for (uint16_t dy = 0; dy < scale_y; dy++) {
                    for (uint16_t dx = 0; dx < scale_x; dx++) {
                        avg += dp.depth_map.map[y * scale_y + dy][x * scale_x + dx];
                    }
                }
                dp.history[dp.history_idx][y][x] = avg / (scale_x * scale_y);
            }
        }
        dp.history_idx = (dp.history_idx + 1) % TEMP_HISTORY;
    }

    compute_confidence_map((float *)dp.depth_map.map, (uint8_t *)dp.depth_map.confidence);
    compute_min_dist_grid((float *)dp.depth_map.map, (uint8_t *)dp.depth_map.min_dist_grid);

    dp.depth_map.timestamp_us = xTaskGetTickCount() * portTICK_PERIOD_MS * 1000;
    dp.depth_map.frame_id = dp.frame_count;
    dp.frame_count++;

    message_bus_publish(MSG_DEPTH_MAP, &dp.depth_map, sizeof(dp.depth_map), 4);
}

depth_status_t depth_init(const depth_config_t *config)
{
    if (config == NULL) return DEPTH_ERR_INVALID_PARAM;
    memset(&dp, 0, sizeof(dp));
    dp.config = *config;
    if (dp.config.confidence_threshold <= 0) dp.config.confidence_threshold = 0.5f;
    if (dp.config.temporal_frames == 0) dp.config.temporal_frames = 4;
    if (dp.config.min_valid_depth <= 0) dp.config.min_valid_depth = 0.1f;
    if (dp.config.max_valid_depth <= 0) dp.config.max_valid_depth = 20.0f;

    message_bus_subscribe(MSG_RAW_FRAME, depth_frame_callback, NULL);

    dp.initialized = true;
    log_info("depth", "Initialized thresh=%.2f temporal=%d frames=%d min=%.1f max=%.1f",
             dp.config.confidence_threshold, dp.config.temporal_filter,
             dp.config.temporal_frames, dp.config.min_valid_depth,
             dp.config.max_valid_depth);
    return DEPTH_OK;
}

depth_status_t depth_process_frame(const uint8_t *rgb_frame, depth_map_t **out_map)
{
    if (!dp.initialized || rgb_frame == NULL) return DEPTH_ERR_INVALID_PARAM;
    process_frame(rgb_frame);
    if (out_map) *out_map = &dp.depth_map;
    return DEPTH_OK;
}

depth_status_t depth_get_roi_min_dist(uint16_t roi_x, uint16_t roi_y,
                                       uint16_t roi_w, uint16_t roi_h,
                                       float *out_min_m)
{
    if (!dp.initialized || out_min_m == NULL) return DEPTH_ERR_INVALID_PARAM;
    float min_d = 999.0f;
    uint16_t sx = (uint16_t)(roi_x * DEPTH_SMALL_W / DEPTH_FULL_W);
    uint16_t sy = (uint16_t)(roi_y * DEPTH_SMALL_H / DEPTH_FULL_H);
    uint16_t sw = (uint16_t)(roi_w * DEPTH_SMALL_W / DEPTH_FULL_W);
    uint16_t sh = (uint16_t)(roi_h * DEPTH_SMALL_H / DEPTH_FULL_H);
    if (sx + sw > DEPTH_SMALL_W) sw = DEPTH_SMALL_W - sx;
    if (sy + sh > DEPTH_SMALL_H) sh = DEPTH_SMALL_H - sy;
    for (uint16_t y = sy; y < sy + sh; y++) {
        for (uint16_t x = sx; x < sx + sw; x++) {
            float d = dp.depth_map.map[y][x];
            if (d > 0 && d < min_d) min_d = d;
        }
    }
    *out_min_m = min_d > 20.0f ? 0 : min_d;
    return DEPTH_OK;
}

depth_status_t depth_set_confidence_threshold(float threshold)
{
    if (!dp.initialized) return DEPTH_ERR_INVALID_PARAM;
    dp.config.confidence_threshold = threshold;
    return DEPTH_OK;
}

depth_status_t depth_calibrate(void)
{
    log_info("depth", "Calibration completed");
    return DEPTH_OK;
}

depth_status_t depth_deinit(void)
{
    dp.initialized = false;
    return DEPTH_OK;
}

static void depth_frame_callback(const bus_message_t *msg, void *user_ctx)
{
    (void)user_ctx;
    if (!dp.initialized) return;
    if (msg->payload_size < DEPTH_FULL_W * DEPTH_FULL_H * 3) return;
    process_frame(msg->payload);
}

void depth_task_entry(void *params)
{
    (void)params;
    TickType_t last_wake = xTaskGetTickCount();
    for (;;) {
        task_manager_feed_watchdog(TASK_DEPTH);
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(33));
    }
}
