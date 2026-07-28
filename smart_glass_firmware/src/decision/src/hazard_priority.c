#include <string.h>
#include <math.h>
#include "hazard_priority.h"
#include "message_bus.h"
#include "logging_manager.h"
#include "task_manager.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

static const uint8_t default_type_weights[12] = {
    [kHazardObstacle]      = 100,
    [kHazardDropOff]       = 200,
    [kHazardStairDescent]  = 200,
    [kHazardStairAscent]   = 150,
    [kHazardVehicle]       = 200,
    [kHazardPersonClose]   = 150,
    [kHazardOverhead]      = 100,
    [kHazardUnevenGround]  = 50,
    [kHazardDoorway]       = 50,
    [kHazardPole]          = 50,
    [kHazardMovingObject]  = 150,
    [kHazardNone]          = 0,
};

static struct {
    hazard_config_t config;
    hazard_event_t queue[HAZARD_PRIORITY_QUEUE_SIZE];
    uint8_t count;
    SemaphoreHandle_t lock;
    uint32_t total_hazards;
    uint32_t suppressed_count;
    hazard_event_t last_hazard;
    uint32_t last_hazard_tick;
    uint8_t type_weights[12];
    bool initialized;
} haz;

static uint32_t now_ms(void)
{
    return xTaskGetTickCount() * portTICK_PERIOD_MS;
}

static uint8_t compute_severity(hazard_type_t type, float distance,
                                 float velocity, bool is_moving)
{
    uint8_t weight = haz.type_weights[type];
    if (weight == 0) weight = default_type_weights[type];

    float max_dist = haz.config.max_hazard_distance;
    if (max_dist <= 0) max_dist = 10.0f;

    float dist_factor = 0;
    if (distance < max_dist && distance > 0) {
        dist_factor = (max_dist - distance) / max_dist * 50.0f;
    }

    float vel_factor = 0;
    if (is_moving && velocity > 0) {
        vel_factor = velocity * 10.0f;
        if (vel_factor > 50) vel_factor = 50;
    }

    uint8_t severity = weight + (uint8_t)dist_factor + (uint8_t)vel_factor;
    return severity;
}

static float compute_tti(float distance, float relative_velocity)
{
    float v = relative_velocity;
    if (v < 0.1f) v = 0.1f;
    return distance / v;
}

static bool is_duplicate(const hazard_event_t *event)
{
    if (haz.last_hazard_tick == 0) return false;

    uint32_t dt = now_ms() - haz.last_hazard_tick;
    if (dt > haz.config.dedup_window_ms) return false;

    if (event->type != haz.last_hazard.type) return false;

    float dx = event->pos_x - haz.last_hazard.pos_x;
    float dy = event->pos_y - haz.last_hazard.pos_y;
    float dz = event->pos_z - haz.last_hazard.pos_z;
    float dist = sqrtf(dx * dx + dy * dy + dz * dz);

    return dist < 1.0f;
}

static void insert_sorted(const hazard_event_t *event)
{
    if (haz.count >= HAZARD_PRIORITY_QUEUE_SIZE) {
        if (event->severity <= haz.queue[haz.count - 1].severity) {
            haz.suppressed_count++;
            return;
        }
        haz.count--;
    }

    int i;
    for (i = haz.count; i > 0; i--) {
        if (haz.queue[i - 1].severity >= event->severity) break;
        haz.queue[i] = haz.queue[i - 1];
    }
    haz.queue[i] = *event;
    haz.count++;
}

static void publish_top_hazard(void)
{
    if (haz.count == 0) return;

    hazard_event_t best = haz.queue[0];
    for (int i = 1; i < haz.count; i++) {
        haz.queue[i - 1] = haz.queue[i];
    }
    haz.count--;

    best.timestamp_us = now_ms() * 1000;

    if (best.severity < haz.config.min_severity_threshold) {
        haz.suppressed_count++;
        log_debug("hazard", "Suppressed severity %d < threshold %d",
                  best.severity, haz.config.min_severity_threshold);
        return;
    }

    haz.last_hazard = best;
    haz.last_hazard_tick = now_ms();

    hazard_event_msg_t msg;
    msg.count = 1;
    msg.events[0] = best;
    message_bus_publish(MSG_HAZARD_EVENT, &msg, sizeof(msg), 4);
    log_info("hazard", "Type=%d severity=%d TTI=%.1fs dist=%.1fm",
             best.type, best.severity, best.tti_seconds, best.distance_m);
}

static void hazard_list_callback(const bus_message_t *msg, void *user_ctx)
{
    (void)user_ctx;
    hazard_list_t list;
    if (msg->payload_size > sizeof(list)) return;
    memcpy(&list, msg->payload, msg->payload_size);

    if (list.count == 0) return;

    xSemaphoreTake(haz.lock, portMAX_DELAY);

    uint8_t max_events = haz.config.max_events_per_frame;
    if (max_events == 0) max_events = 3;

    uint32_t to_process = list.count;
    if (to_process > max_events) to_process = max_events;

    for (uint32_t i = 0; i < to_process && i < 12; i++) {
        hazard_event_t ev;
        memset(&ev, 0, sizeof(ev));
        ev.type = list.hazards[i];
        ev.severity = list.severity[i];
        ev.distance_m = list.distance[i];
        ev.confidence = 200;
        ev.timestamp_us = now_ms() * 1000;
        ev.is_moving = false;

        if (ev.severity == 0) {
            ev.severity = compute_severity(ev.type, ev.distance_m, 0, false);
        }

        if (ev.distance_m > 0) {
            ev.tti_seconds = compute_tti(ev.distance_m, 0.5f);
        } else {
            ev.tti_seconds = 999.0f;
            ev.distance_m = 999.0f;
        }

        if (is_duplicate(&ev)) {
            haz.suppressed_count++;
            continue;
        }

        insert_sorted(&ev);
        haz.total_hazards++;
    }

    publish_top_hazard();
    xSemaphoreGive(haz.lock);
}

hazard_status_t hazard_priority_init(const hazard_config_t *config)
{
    if (config == NULL) return HAZARD_ERR_NOT_INIT;

    memset(&haz, 0, sizeof(haz));
    haz.config = *config;
    haz.lock = xSemaphoreCreateMutex();
    if (haz.lock == NULL) return HAZARD_ERR_NOT_INIT;

    memcpy(haz.type_weights, default_type_weights, sizeof(default_type_weights));

    if (haz.config.max_hazard_distance <= 0) haz.config.max_hazard_distance = 10.0f;
    if (haz.config.min_severity_threshold == 0) haz.config.min_severity_threshold = 20;
    if (haz.config.max_events_per_frame == 0) haz.config.max_events_per_frame = 3;
    if (haz.config.dedup_window_ms == 0) haz.config.dedup_window_ms = 500;

    message_bus_subscribe(MSG_HAZARD_LIST, hazard_list_callback, NULL);

    haz.initialized = true;
    log_info("hazard", "Initialized, max_dist=%.1f min_sev=%d",
             haz.config.max_hazard_distance, haz.config.min_severity_threshold);
    return HAZARD_OK;
}

hazard_status_t hazard_priority_process(const hazard_list_t *hazards,
                                         hazard_event_msg_t **out_event)
{
    if (!haz.initialized) return HAZARD_ERR_NOT_INIT;
    (void)hazards;
    if (out_event) *out_event = NULL;
    return HAZARD_OK;
}

hazard_status_t hazard_priority_get_top_n(uint8_t n, hazard_event_t *out_events,
                                           uint8_t *out_count)
{
    if (!haz.initialized) return HAZARD_ERR_NOT_INIT;
    xSemaphoreTake(haz.lock, portMAX_DELAY);
    uint8_t available = haz.count < n ? haz.count : n;
    for (uint8_t i = 0; i < available; i++) {
        out_events[i] = haz.queue[i];
    }
    if (out_count) *out_count = available;
    xSemaphoreGive(haz.lock);
    return HAZARD_OK;
}

hazard_status_t hazard_priority_set_severity_weights(hazard_type_t type,
                                                      uint8_t weight)
{
    if (!haz.initialized) return HAZARD_ERR_NOT_INIT;
    if (type > kHazardNone) return HAZARD_ERR_NOT_INIT;
    haz.type_weights[type] = weight;
    return HAZARD_OK;
}

hazard_status_t hazard_priority_clear(void)
{
    if (!haz.initialized) return HAZARD_ERR_NOT_INIT;
    xSemaphoreTake(haz.lock, portMAX_DELAY);
    haz.count = 0;
    xSemaphoreGive(haz.lock);
    return HAZARD_OK;
}

hazard_status_t hazard_priority_deinit(void)
{
    haz.initialized = false;
    return HAZARD_OK;
}
