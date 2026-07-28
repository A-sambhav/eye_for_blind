#include "test_harness.h"
#include "FreeRTOS.h"
#include "message_bus.h"
#include "task_manager.h"

static int mock_log_count = 0;
void log_info(const char *m, const char *f, ...) { (void)m;(void)f;mock_log_count++; }
void log_warn(const char *m, const char *f, ...) { (void)m;(void)f;mock_log_count++; }
void log_error(const char *m, const char *f, ...) { (void)m;(void)f;mock_log_count++; }
void log_debug(const char *m, const char *f, ...) { (void)m;(void)f;mock_log_count++; }
void log_critical(const char *m, const char *f, ...) { (void)m;(void)f;mock_log_count++; }

TickType_t xTaskGetTickCount(void) { return 1000; }

msg_bus_status_t message_bus_publish(msg_type_t t, const void *p, uint16_t s, uint8_t pri)
{ (void)t;(void)p;(void)s;(void)pri; return MSG_BUS_OK; }
msg_bus_status_t message_bus_subscribe(msg_type_t t, bus_subscriber_fn c, void *u)
{ (void)t;(void)c;(void)u; return MSG_BUS_OK; }
msg_bus_status_t message_bus_unsubscribe(msg_type_t t, bus_subscriber_fn c)
{ (void)t;(void)c; return MSG_BUS_OK; }
void task_manager_feed_watchdog(task_id_t id) { (void)id; }
void vTaskDelayUntil(TickType_t *pxPreviousWakeTime, TickType_t xTimeIncrement)
{ (void)pxPreviousWakeTime; (void)xTimeIncrement; }

#include "../../src/ai/src/depth_processing.c"

TEST(depth_init_ok)
{
    depth_config_t cfg = { .confidence_threshold = 0.5f, .temporal_filter = false,
                           .temporal_frames = 4, .min_valid_depth = 0.3f,
                           .max_valid_depth = 20.0f };
    ASSERT_EQ(depth_init(&cfg), DEPTH_OK);
    depth_deinit();
}

TEST(depth_init_null)
{
    ASSERT_EQ(depth_init(NULL), DEPTH_ERR_INVALID_PARAM);
}

TEST(depth_process_frame)
{
    depth_config_t cfg = { .confidence_threshold = 0.5f, .temporal_filter = false,
                           .temporal_frames = 4, .min_valid_depth = 0.3f,
                           .max_valid_depth = 20.0f };
    depth_init(&cfg);

    uint8_t rgb[DEPTH_FULL_W * DEPTH_FULL_H * 3];
    memset(rgb, 128, sizeof(rgb));

    depth_map_t *out_map = NULL;
    ASSERT_EQ(depth_process_frame(rgb, &out_map), DEPTH_OK);
    ASSERT_NOT_NULL(out_map);

    depth_deinit();
}

TEST(depth_two_frames)
{
    depth_config_t cfg = { .confidence_threshold = 0.5f, .temporal_filter = false,
                           .temporal_frames = 4, .min_valid_depth = 0.3f,
                           .max_valid_depth = 20.0f };
    depth_init(&cfg);

    uint8_t rgb[DEPTH_FULL_W * DEPTH_FULL_H * 3];
    memset(rgb, 128, sizeof(rgb));

    depth_map_t *out1 = NULL;
    depth_process_frame(rgb, &out1);
    ASSERT_NOT_NULL(out1);
    uint32_t id1 = out1->frame_id;

    depth_map_t *out2 = NULL;
    depth_process_frame(rgb, &out2);
    ASSERT_NOT_NULL(out2);
    ASSERT_EQ(out2->frame_id - id1, 1);

    depth_deinit();
}

TEST(depth_temporal_filter)
{
    depth_config_t cfg = { .confidence_threshold = 0.5f, .temporal_filter = true,
                           .temporal_frames = 4, .min_valid_depth = 0.3f,
                           .max_valid_depth = 20.0f };
    depth_init(&cfg);

    uint8_t rgb[DEPTH_FULL_W * DEPTH_FULL_H * 3];
    memset(rgb, 128, sizeof(rgb));

    depth_map_t *out = NULL;
    ASSERT_EQ(depth_process_frame(rgb, &out), DEPTH_OK);
    ASSERT_NOT_NULL(out);

    depth_deinit();
}

TEST(depth_roi_min_dist)
{
    depth_config_t cfg = { .confidence_threshold = 0.5f, .temporal_filter = false,
                           .temporal_frames = 4, .min_valid_depth = 0.3f,
                           .max_valid_depth = 20.0f };
    depth_init(&cfg);

    uint8_t rgb[DEPTH_FULL_W * DEPTH_FULL_H * 3];
    memset(rgb, 128, sizeof(rgb));
    depth_process_frame(rgb, NULL);

    float min_dist = 0;
    ASSERT_EQ(depth_get_roi_min_dist(100, 80, 200, 150, &min_dist), DEPTH_OK);
    ASSERT_TRUE(min_dist >= 0);

    depth_deinit();
}

TEST(depth_calibrate)
{
    depth_config_t cfg = { .confidence_threshold = 0.5f, .temporal_filter = false,
                           .temporal_frames = 4, .min_valid_depth = 0.3f,
                           .max_valid_depth = 20.0f };
    depth_init(&cfg);
    ASSERT_EQ(depth_calibrate(), DEPTH_OK);
    depth_deinit();
}

TEST(depth_set_threshold)
{
    depth_config_t cfg = { .confidence_threshold = 0.5f, .temporal_filter = false,
                           .temporal_frames = 4, .min_valid_depth = 0.3f,
                           .max_valid_depth = 20.0f };
    depth_init(&cfg);

    ASSERT_EQ(depth_set_confidence_threshold(0.75f), DEPTH_OK);
    ASSERT_FLOAT_EQ(dp.config.confidence_threshold, 0.75f, 0.001f);

    depth_deinit();
}

int main(void)
{
    return run_all_tests();
}
