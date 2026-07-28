#include <string.h>
#include "speech_synthesis.h"
#include "message_bus.h"
#include "message_types.h"
#include "logging_manager.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

typedef struct {
    speech_request_t queue[SPEECH_QUEUE_DEPTH];
    uint8_t head;
    uint8_t tail;
    uint8_t count;
} speech_ring_t;

static struct {
    speech_config_t config;
    speech_ring_t ring;
    SemaphoreHandle_t lock;
    SemaphoreHandle_t play_lock;
    speech_request_t active;
    bool is_speaking;
    bool initialized;
    uint32_t speak_count;
    uint32_t interrupt_count;
    char active_text[SPEECH_MAX_TEXT_LEN];
} sp;

static void insert_sorted(const speech_request_t *req)
{
    if (sp.ring.count >= SPEECH_QUEUE_DEPTH) {
        uint8_t lowest_idx = sp.ring.head;
        uint8_t lowest_pri = sp.ring.queue[sp.ring.head].priority;
        for (uint8_t i = 0; i < sp.ring.count; i++) {
            uint8_t idx = (sp.ring.head + i) % SPEECH_QUEUE_DEPTH;
            if (sp.ring.queue[idx].priority < lowest_pri) {
                lowest_pri = sp.ring.queue[idx].priority;
                lowest_idx = idx;
            }
        }
        if (req->priority <= lowest_pri) return;
        for (uint8_t i = lowest_idx; i < sp.ring.count; i++) {
            uint8_t idx = (sp.ring.head + i) % SPEECH_QUEUE_DEPTH;
            uint8_t next = (idx + 1) % SPEECH_QUEUE_DEPTH;
            sp.ring.queue[idx] = sp.ring.queue[next];
        }
        sp.ring.tail = (sp.ring.tail + SPEECH_QUEUE_DEPTH - 1) % SPEECH_QUEUE_DEPTH;
        sp.ring.count--;
    }

    uint8_t pos = sp.ring.tail;
    for (uint8_t i = 0; i < sp.ring.count; i++) {
        uint8_t idx = (sp.ring.head + i) % SPEECH_QUEUE_DEPTH;
        if (sp.ring.queue[idx].priority < req->priority) {
            pos = idx;
            break;
        }
    }

    if (pos == sp.ring.tail) {
        sp.ring.queue[sp.ring.tail] = *req;
        sp.ring.tail = (sp.ring.tail + 1) % SPEECH_QUEUE_DEPTH;
        sp.ring.count++;
    } else {
        uint8_t insert_at = (pos - sp.ring.head + sp.ring.count) % SPEECH_QUEUE_DEPTH;
        for (int i = sp.ring.count; i > insert_at; i--) {
            uint8_t from = (sp.ring.head + i - 1) % SPEECH_QUEUE_DEPTH;
            uint8_t to = (sp.ring.head + i) % SPEECH_QUEUE_DEPTH;
            sp.ring.queue[to] = sp.ring.queue[from];
        }
        uint8_t idx = (sp.ring.head + insert_at) % SPEECH_QUEUE_DEPTH;
        sp.ring.queue[idx] = *req;
        sp.ring.count++;
        sp.ring.tail = (sp.ring.tail + 1) % SPEECH_QUEUE_DEPTH;
    }
}

static void speech_task_tick(void)
{
    if (sp.is_speaking) return;
    if (sp.ring.count == 0) return;

    xSemaphoreTake(sp.lock, portMAX_DELAY);
    sp.active = sp.ring.queue[sp.ring.head];
    sp.ring.head = (sp.ring.head + 1) % SPEECH_QUEUE_DEPTH;
    sp.ring.count--;
    xSemaphoreGive(sp.lock);

    sp.is_speaking = true;
    strncpy(sp.active_text, sp.active.text, SPEECH_MAX_TEXT_LEN - 1);
    sp.active_text[SPEECH_MAX_TEXT_LEN - 1] = '\0';
    sp.speak_count++;

    log_info("speech", "Speaking [pri=%u]: %s", sp.active.priority, sp.active_text);

    uint32_t dur = sp.active.duration_ms;
    if (dur == 0) dur = strlen(sp.active_text) * 60;
    vTaskDelay(pdMS_TO_TICKS(dur < 100 ? 100 : dur));
    sp.is_speaking = false;
}

static void msg_callback(const bus_message_t *msg, void *user_ctx)
{
    (void)user_ctx;
    speech_request_t req;
    if (msg->payload_size > sizeof(req)) return;
    memcpy(&req, msg->payload, msg->payload_size);

    xSemaphoreTake(sp.lock, portMAX_DELAY);
    insert_sorted(&req);
    xSemaphoreGive(sp.lock);
}

speech_status_t speech_init(const speech_config_t *config)
{
    if (config == NULL) return SPEECH_ERR_INIT;
    memset(&sp, 0, sizeof(sp));
    sp.config = *config;

    sp.lock = xSemaphoreCreateMutex();
    sp.play_lock = xSemaphoreCreateMutex();
    if (!sp.lock || !sp.play_lock) return SPEECH_ERR_INIT;

    message_bus_subscribe(MSG_SPEECH_REQ, msg_callback, NULL);
    message_bus_subscribe(MSG_NAV_SPEECH, msg_callback, NULL);

    sp.initialized = true;
    log_info("speech", "Initialized vol=%u speed=%.1f muted=%d",
             sp.config.volume_percent, sp.config.speed, sp.config.muted);
    return SPEECH_OK;
}

speech_status_t speech_speak(const char *text, uint8_t priority)
{
    if (!sp.initialized || text == NULL) return SPEECH_ERR_INIT;
    speech_request_t req;
    memset(&req, 0, sizeof(req));
    strncpy(req.text, text, SPEECH_MAX_TEXT_LEN - 1);
    req.priority = priority;
    req.timestamp_us = xTaskGetTickCount() * portTICK_PERIOD_MS * 1000;
    req.duration_ms = strlen(text) * 60;

    xSemaphoreTake(sp.lock, portMAX_DELAY);
    insert_sorted(&req);
    xSemaphoreGive(sp.lock);

    speech_task_tick();
    return SPEECH_OK;
}

speech_status_t speech_interrupt(const char *text)
{
    if (!sp.initialized || text == NULL) return SPEECH_ERR_INIT;
    sp.is_speaking = false;
    sp.interrupt_count++;

    speech_request_t req;
    memset(&req, 0, sizeof(req));
    strncpy(req.text, text, SPEECH_MAX_TEXT_LEN - 1);
    req.priority = 255;
    req.timestamp_us = xTaskGetTickCount() * portTICK_PERIOD_MS * 1000;

    xSemaphoreTake(sp.lock, portMAX_DELAY);
    insert_sorted(&req);
    xSemaphoreGive(sp.lock);

    speech_task_tick();
    return SPEECH_OK;
}

speech_status_t speech_set_volume(uint8_t volume_percent)
{
    if (!sp.initialized) return SPEECH_ERR_INIT;
    sp.config.volume_percent = volume_percent > 100 ? 100 : volume_percent;
    return SPEECH_OK;
}

speech_status_t speech_mute(bool mute)
{
    if (!sp.initialized) return SPEECH_ERR_INIT;
    sp.config.muted = mute;
    if (mute) sp.is_speaking = false;
    return SPEECH_OK;
}

bool speech_is_speaking(void)
{
    return sp.is_speaking;
}

speech_status_t speech_get_queue_depth(uint8_t *out_depth)
{
    if (!sp.initialized || out_depth == NULL) return SPEECH_ERR_INIT;
    xSemaphoreTake(sp.lock, portMAX_DELAY);
    *out_depth = sp.ring.count;
    xSemaphoreGive(sp.lock);
    return SPEECH_OK;
}

speech_status_t speech_deinit(void)
{
    sp.initialized = false;
    return SPEECH_OK;
}
