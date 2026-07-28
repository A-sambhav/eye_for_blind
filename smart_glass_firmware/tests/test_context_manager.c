#include "test_harness.h"
#include "FreeRTOS.h"
#include "semphr.h"
#include "message_bus.h"
#include "task_manager.h"

static int mock_log_count = 0;
void log_info(const char *m, const char *f, ...) { (void)m;(void)f;mock_log_count++; }
void log_warn(const char *m, const char *f, ...) { (void)m;(void)f;mock_log_count++; }
void log_error(const char *m, const char *f, ...) { (void)m;(void)f;mock_log_count++; }
void log_debug(const char *m, const char *f, ...) { (void)m;(void)f;mock_log_count++; }
void log_critical(const char *m, const char *f, ...) { (void)m;(void)f;mock_log_count++; }

TickType_t xTaskGetTickCount(void) { return 5000; }
SemaphoreHandle_t xSemaphoreCreateMutex(void) { return (SemaphoreHandle_t)1; }
BaseType_t xSemaphoreTake(SemaphoreHandle_t s, TickType_t t) { (void)s;(void)t; return pdPASS; }
BaseType_t xSemaphoreGive(SemaphoreHandle_t s) { (void)s; return pdPASS; }
void task_manager_feed_watchdog(task_id_t id) { (void)id; }

msg_bus_status_t message_bus_publish(msg_type_t t, const void *p, uint16_t s, uint8_t pri)
{ (void)t;(void)p;(void)s;(void)pri; return MSG_BUS_OK; }
msg_bus_status_t message_bus_subscribe(msg_type_t t, bus_subscriber_fn c, void *u)
{ (void)t;(void)c;(void)u; return MSG_BUS_OK; }
msg_bus_status_t message_bus_unsubscribe(msg_type_t t, bus_subscriber_fn c)
{ (void)t;(void)c; return MSG_BUS_OK; }

#include "../../src/decision/src/context_manager.c"

TEST(context_init_ok)
{
    context_config_t cfg = { .history_size = 10, .enable_location_naming = false };
    ASSERT_EQ(context_init(&cfg), CONTEXT_OK);
    context_deinit();
}

TEST(context_init_null)
{
    ASSERT_EQ(context_init(NULL), CONTEXT_ERR_NOT_INIT);
}

TEST(context_scene_indoor)
{
    context_config_t cfg = { .history_size = 10, .enable_location_naming = false };
    context_init(&cfg);

    scene_desc_t scene;
    memset(&scene, 0, sizeof(scene));
    scene.scene_type = kSceneIndoor;
    scene.timestamp_us = 1000;

    context_msg_t *out = NULL;
    ASSERT_EQ(context_process(&scene, &out), CONTEXT_OK);
    ASSERT_NOT_NULL(out);
    ASSERT_EQ(out->scene_type, kSceneIndoor);
    ASSERT_EQ(out->is_indoor, true);

    context_deinit();
}

TEST(context_scene_outdoor)
{
    context_config_t cfg = { .history_size = 10, .enable_location_naming = false };
    context_init(&cfg);

    scene_desc_t scene;
    memset(&scene, 0, sizeof(scene));
    scene.scene_type = kSceneOutdoor;
    scene.timestamp_us = 1000;

    context_msg_t *out = NULL;
    context_process(&scene, &out);
    ASSERT_EQ(out->is_indoor, false);

    context_deinit();
}

TEST(context_scene_transition)
{
    context_config_t cfg = { .history_size = 10, .enable_location_naming = false };
    context_init(&cfg);

    scene_desc_t indoor;
    memset(&indoor, 0, sizeof(indoor));
    indoor.scene_type = kSceneIndoor;
    indoor.timestamp_us = 1000;

    scene_desc_t outdoor;
    memset(&outdoor, 0, sizeof(outdoor));
    outdoor.scene_type = kSceneOutdoor;
    outdoor.timestamp_us = 2000;

    context_msg_t *out = NULL;
    context_process(&indoor, &out);
    ASSERT_EQ(out->prev_scene_type, kSceneUnknown);

    context_process(&outdoor, &out);
    ASSERT_EQ(out->prev_scene_type, kSceneIndoor);
    ASSERT_EQ(out->scene_type, kSceneOutdoor);

    context_deinit();
}

TEST(context_get_current_state)
{
    context_config_t cfg = { .history_size = 10, .enable_location_naming = false };
    context_init(&cfg);

    user_state_t state;
    ASSERT_EQ(context_get_current_state(&state), CONTEXT_OK);
    ASSERT_EQ(state, kUserStationary);

    context_deinit();
}

TEST(context_get_location_type)
{
    context_config_t cfg = { .history_size = 10, .enable_location_naming = false };
    context_init(&cfg);

    uint8_t loc_type;
    ASSERT_EQ(context_get_location_type(&loc_type), CONTEXT_OK);

    context_deinit();
}

TEST(context_is_moving)
{
    context_config_t cfg = { .history_size = 10, .enable_location_naming = false };
    context_init(&cfg);

    bool moving = true;
    ASSERT_EQ(context_is_moving(&moving), CONTEXT_OK);
    ASSERT_EQ(moving, false);

    context_deinit();
}

int main(void)
{
    return run_all_tests();
}
