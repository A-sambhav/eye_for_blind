#include <string.h>
#include <math.h>
#include "context_manager.h"
#include "message_bus.h"
#include "logging_manager.h"
#include "task_manager.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

typedef struct {
    scene_type_t scene_type;
    uint32_t timestamp_us;
    float avg_depth;
    uint8_t object_count;
} scene_summary_t;

static struct {
    context_config_t config;
    scene_summary_t history[16];
    uint8_t head;
    uint8_t count;
    context_msg_t current;
    SemaphoreHandle_t lock;
    uint32_t scene_count;
    uint32_t last_imu_tick;
    bool initialized;
} ctx;

static uint32_t now_us(void)
{
    return xTaskGetTickCount() * portTICK_PERIOD_MS * 1000;
}

static bool is_indoor_scene(scene_type_t type)
{
    switch (type) {
        case kSceneIndoor:
        case kSceneCorridor:
        case kSceneStaircase:
            return true;
        default:
            return false;
    }
}

static void publish_context(void)
{
    message_bus_publish(MSG_CONTEXT, &ctx.current, sizeof(ctx.current), 2);
}

static void handle_scene(const scene_desc_t *scene)
{
    xSemaphoreTake(ctx.lock, portMAX_DELAY);

    scene_type_t prev_type = ctx.current.scene_type;

    ctx.current.scene_type = scene->scene_type;
    ctx.current.prev_scene_type = prev_type;
    ctx.current.timestamp_us = now_us();
    ctx.current.is_indoor = is_indoor_scene(scene->scene_type);
    ctx.current.time_since_transition_ms = 0;

    if (scene->scene_type != prev_type) {
        ctx.current.time_at_location_ms = 0;
        log_info("context", "Scene transition %d -> %d", prev_type, scene->scene_type);
    } else {
        ctx.current.time_at_location_ms += 100;
    }

    scene_summary_t s;
    s.scene_type = scene->scene_type;
    s.timestamp_us = scene->timestamp_us;
    s.object_count = scene->count;
    s.avg_depth = 2.0f;
    if (scene->count == 0 && (uint32_t)scene->scene_type < 8) {
        uint32_t total = 0, cells = 0;
        for (int y = 0; y < 8 && y < FREE_SPACE_GRID_H; y += 15) {
            for (int x = 0; x < FREE_SPACE_GRID_W; x += 20) {
                total += scene->free_space_grid[y][x];
                cells++;
            }
        }
        if (cells) s.avg_depth = (float)total / cells / 25.0f;
    }

    ctx.history[ctx.head] = s;
    ctx.head = (ctx.head + 1) % 16;
    if (ctx.count < 16) ctx.count++;

    ctx.scene_count++;
    xSemaphoreGive(ctx.lock);

    publish_context();
}

static void scene_callback(const bus_message_t *msg, void *user_ctx)
{
    (void)user_ctx;
    scene_desc_t scene;
    if (msg->payload_size > sizeof(scene)) return;
    memcpy(&scene, msg->payload, msg->payload_size);
    handle_scene(&scene);
}

static void imu_callback(const bus_message_t *msg, void *user_ctx)
{
    (void)user_ctx;
    imu_data_t imu;
    if (msg->payload_size > sizeof(imu)) return;
    memcpy(&imu, msg->payload, msg->payload_size);

    xSemaphoreTake(ctx.lock, portMAX_DELAY);

    ctx.last_imu_tick = xTaskGetTickCount();

    float total_accel = sqrtf(imu.accel_x * imu.accel_x +
                               imu.accel_y * imu.accel_y +
                               imu.accel_z * imu.accel_z);

    if (imu.fall_detected) {
        ctx.current.user_state = kUserFalling;
        log_warn("context", "Fall detected");
    } else if (imu.step_detected) {
        if (total_accel > 15.0f) {
            ctx.current.user_state = kUserRunning;
        } else {
            ctx.current.user_state = kUserWalking;
        }
    } else {
        float g = 9.81f;
        if (total_accel < g * 0.8f || total_accel > g * 1.2f) {
            ctx.current.user_state = kUserStationary;
        }
    }

    ctx.current.location.heading_deg = atan2f(imu.gyro_z, imu.gyro_x) * 180.0f / 3.14159f;
    ctx.current.location.avg_speed_mps = sqrtf(imu.accel_x * imu.accel_x +
                                                imu.accel_y * imu.accel_y) * 0.1f;

    xSemaphoreGive(ctx.lock);
}

context_status_t context_init(const context_config_t *config)
{
    if (config == NULL) return CONTEXT_ERR_NOT_INIT;

    memset(&ctx, 0, sizeof(ctx));
    ctx.config = *config;
    ctx.lock = xSemaphoreCreateMutex();
    if (ctx.lock == NULL) return CONTEXT_ERR_NOT_INIT;

    ctx.current.scene_type = kSceneUnknown;
    ctx.current.prev_scene_type = kSceneUnknown;
    ctx.current.user_state = kUserStationary;
    ctx.current.is_indoor = true;
    ctx.current.timestamp_us = now_us();

    message_bus_subscribe(MSG_SCENE_DESC, scene_callback, NULL);
    message_bus_subscribe(MSG_IMU_DATA, imu_callback, NULL);

    ctx.initialized = true;
    log_info("context", "Initialized, history=%d", config->history_size);
    return CONTEXT_OK;
}

context_status_t context_process(const scene_desc_t *scene, context_msg_t **out_context)
{
    if (!ctx.initialized) return CONTEXT_ERR_NOT_INIT;
    if (scene) {
        handle_scene(scene);
    }
    xSemaphoreTake(ctx.lock, portMAX_DELAY);
    if (out_context) *out_context = &ctx.current;
    xSemaphoreGive(ctx.lock);
    return CONTEXT_OK;
}

context_status_t context_get_current_state(user_state_t *out_state)
{
    if (!ctx.initialized || out_state == NULL) return CONTEXT_ERR_NOT_INIT;
    xSemaphoreTake(ctx.lock, portMAX_DELAY);
    *out_state = ctx.current.user_state;
    xSemaphoreGive(ctx.lock);
    return CONTEXT_OK;
}

context_status_t context_get_location_type(uint8_t *out_type)
{
    if (!ctx.initialized || out_type == NULL) return CONTEXT_ERR_NOT_INIT;
    xSemaphoreTake(ctx.lock, portMAX_DELAY);
    *out_type = ctx.current.is_indoor ? 0 : 1;
    xSemaphoreGive(ctx.lock);
    return CONTEXT_OK;
}

context_status_t context_is_moving(bool *out_moving)
{
    if (!ctx.initialized || out_moving == NULL) return CONTEXT_ERR_NOT_INIT;
    xSemaphoreTake(ctx.lock, portMAX_DELAY);
    *out_moving = (ctx.current.user_state == kUserWalking ||
                   ctx.current.user_state == kUserRunning);
    xSemaphoreGive(ctx.lock);
    return CONTEXT_OK;
}

context_status_t context_deinit(void)
{
    ctx.initialized = false;
    return CONTEXT_OK;
}
