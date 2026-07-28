#include "test_harness.h"
#include "FreeRTOS.h"

static int mock_log_count = 0;
void log_info(const char *m, const char *f, ...) { (void)m; (void)f; mock_log_count++; }
void log_warn(const char *m, const char *f, ...) { (void)m; (void)f; mock_log_count++; }
void log_error(const char *m, const char *f, ...) { (void)m; (void)f; mock_log_count++; }
void log_debug(const char *m, const char *f, ...) { (void)m; (void)f; mock_log_count++; }
void log_critical(const char *m, const char *f, ...) { (void)m; (void)f; mock_log_count++; }

TickType_t xTaskGetTickCount(void) { static TickType_t t=0; return t++; }

#include "../../src/ai/src/object_tracking.c"

TEST(tracker_init_ok)
{
    tracker_config_t cfg = { .iou_threshold = 0.5f, .confidence_threshold = 0.0f,
                             .max_stale_frames = 10, .max_tracks = 16,
                             .velocity_threshold_mps = 0.5f,
                             .use_kalman = true, .process_noise = 0.1f,
                             .measurement_noise = 0.5f };
    ASSERT_EQ(tracker_init(&cfg), TRACKER_OK);
    tracker_deinit();
}

TEST(tracker_init_null)
{
    ASSERT_EQ(tracker_init(NULL), TRACKER_ERR_INIT);
}

TEST(tracker_single_detection)
{
    tracker_config_t cfg = { .iou_threshold = 0.5f, .confidence_threshold = 0.0f,
                             .max_stale_frames = 10, .max_tracks = 16,
                             .velocity_threshold_mps = 0.5f, .use_kalman = true,
                             .process_noise = 0.1f, .measurement_noise = 0.5f };
    tracker_init(&cfg);

    detection_list_t dets;
    dets.count = 1;
    dets.timestamp_us = 1000;
    dets.boxes[0] = (bounding_box_t){ .x = 100, .y = 200, .width = 50, .height = 80,
        .confidence = 0.9f, .class_id = 1, .pos_x = 1.0f, .pos_y = 0, .pos_z = -0.5f };

    tracked_obj_msg_t *out = NULL;
    ASSERT_EQ(tracker_process(&dets, &out), TRACKER_OK);
    ASSERT_NOT_NULL(out);
    ASSERT_TRUE(out->active_count >= 1);

    tracker_deinit();
}

TEST(tracker_two_detections)
{
    tracker_config_t cfg = { .iou_threshold = 0.5f, .confidence_threshold = 0.0f,
                             .max_stale_frames = 10, .max_tracks = 16,
                             .velocity_threshold_mps = 0.5f, .use_kalman = true,
                             .process_noise = 0.1f, .measurement_noise = 0.5f };
    tracker_init(&cfg);

    detection_list_t dets;
    dets.count = 2;
    dets.timestamp_us = 1000;
    dets.boxes[0] = (bounding_box_t){ .x = 100, .y = 100, .width = 30, .height = 60,
        .confidence = 0.9f, .class_id = 1, .pos_x = 1.0f, .pos_y = 0, .pos_z = 0 };
    dets.boxes[1] = (bounding_box_t){ .x = 200, .y = 150, .width = 40, .height = 70,
        .confidence = 0.8f, .class_id = 2, .pos_x = -2.0f, .pos_y = 0.5f, .pos_z = 0 };

    tracked_obj_msg_t *out = NULL;
    ASSERT_EQ(tracker_process(&dets, &out), TRACKER_OK);
    ASSERT_TRUE(out->active_count >= 1);

    tracker_deinit();
}

TEST(tracker_max_tracks)
{
    tracker_config_t cfg = { .iou_threshold = 0.5f, .confidence_threshold = 0.0f,
                             .max_stale_frames = 10, .max_tracks = 3,
                             .velocity_threshold_mps = 0.5f, .use_kalman = false,
                             .process_noise = 0.1f, .measurement_noise = 0.5f };
    tracker_init(&cfg);

    detection_list_t dets;
    dets.count = 10;
    dets.timestamp_us = 1000;
    for (int i = 0; i < 10; i++) {
        dets.boxes[i] = (bounding_box_t){ .x = (uint16_t)(i * 50), .y = 100,
            .width = 30, .height = 60, .confidence = 0.9f, .class_id = 1,
            .pos_x = (float)i * 2, .pos_y = 0, .pos_z = 0 };
    }

    tracked_obj_msg_t *out = NULL;
    ASSERT_EQ(tracker_process(&dets, &out), TRACKER_OK);
    ASSERT_EQ(out->active_count, 3);

    tracker_deinit();
}

TEST(tracker_get_track_by_id)
{
    tracker_config_t cfg = { .iou_threshold = 0.5f, .confidence_threshold = 0.0f,
                             .max_stale_frames = 10, .max_tracks = 16,
                             .velocity_threshold_mps = 0.5f, .use_kalman = true,
                             .process_noise = 0.1f, .measurement_noise = 0.5f };
    tracker_init(&cfg);

    detection_list_t dets;
    dets.count = 1;
    dets.timestamp_us = 1000;
    dets.boxes[0] = (bounding_box_t){ .x = 100, .y = 200, .width = 50, .height = 80,
        .confidence = 0.9f, .class_id = 1, .pos_x = 1.0f, .pos_y = 0, .pos_z = -0.5f };

    tracked_obj_msg_t *out = NULL;
    tracker_process(&dets, &out);
    uint32_t tid = out->tracks[0].track_id;

    tracked_object_t track;
    ASSERT_EQ(tracker_get_track_by_id(tid, &track), TRACKER_OK);
    ASSERT_EQ(track.track_id, tid);

    tracker_deinit();
}

TEST(tracker_get_active_count)
{
    tracker_config_t cfg = { .iou_threshold = 0.5f, .confidence_threshold = 0.0f,
                             .max_stale_frames = 10, .max_tracks = 16,
                             .velocity_threshold_mps = 0.5f, .use_kalman = true,
                             .process_noise = 0.1f, .measurement_noise = 0.5f };
    tracker_init(&cfg);

    uint32_t count = 99;
    ASSERT_EQ(tracker_get_active_count(&count), TRACKER_OK);
    ASSERT_EQ(count, 0);

    detection_list_t dets;
    dets.count = 1;
    dets.timestamp_us = 1000;
    dets.boxes[0] = (bounding_box_t){ .x = 100, .y = 200, .width = 50, .height = 80,
        .confidence = 0.9f, .class_id = 1 };
    tracker_process(&dets, NULL);

    ASSERT_EQ(tracker_get_active_count(&count), TRACKER_OK);
    ASSERT_TRUE(count >= 1);

    tracker_deinit();
}

TEST(tracker_reset)
{
    tracker_config_t cfg = { .iou_threshold = 0.5f, .confidence_threshold = 0.0f,
                             .max_stale_frames = 10, .max_tracks = 16,
                             .velocity_threshold_mps = 0.5f, .use_kalman = true,
                             .process_noise = 0.1f, .measurement_noise = 0.5f };
    tracker_init(&cfg);

    ASSERT_EQ(tracker_reset(), TRACKER_OK);

    tracker_deinit();
}

int main(void)
{
    return run_all_tests();
}
