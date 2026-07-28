#include <string.h>
#include "message_bus.h"
#include "logging_manager.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

#define MSG_BUS_QUEUE_LENGTH 32

typedef struct {
    msg_bus_callback_t callback;
    void *user_ctx;
} subscriber_entry_t;

static struct {
    QueueHandle_t queue;
    SemaphoreHandle_t sub_lock;
    subscriber_entry_t subscribers[MSG_TYPE_COUNT][MSG_BUS_MAX_SUBSCRIBERS_PER_TYPE];
    uint8_t subscriber_count[MSG_TYPE_COUNT];
    bool initialized;
} bus;

msg_bus_status_t message_bus_init(void)
{
    memset(&bus, 0, sizeof(bus));
    bus.queue = xQueueCreate(MSG_BUS_QUEUE_LENGTH, sizeof(bus_message_t));
    if (bus.queue == NULL) return MSG_BUS_ERR_FULL;
    bus.sub_lock = xSemaphoreCreateMutex();
    if (bus.sub_lock == NULL) { vQueueDelete(bus.queue); return MSG_BUS_ERR_NOT_INIT; }
    bus.initialized = true;
    return MSG_BUS_OK;
}

msg_bus_status_t message_bus_publish(msg_type_t type, const void *payload,
                                      size_t payload_size, uint8_t priority)
{
    if (!bus.initialized) return MSG_BUS_ERR_NOT_INIT;
    if (payload_size > MSG_MAX_PAYLOAD_BYTES) return MSG_BUS_ERR_TOO_LARGE;

    bus_message_t msg;
    msg.type = type;
    msg.timestamp_us = xTaskGetTickCount() * portTICK_PERIOD_MS * 1000;
    msg.payload_size = payload_size;
    msg.priority = priority;
    if (payload && payload_size > 0) {
        memcpy(msg.payload, payload, payload_size);
    }

    if (xQueueSend(bus.queue, &msg, 0) != pdTRUE) {
        log_warn("msg_bus", "Queue full, dropped type %d", type);
        return MSG_BUS_ERR_FULL;
    }
    return MSG_BUS_OK;
}

msg_bus_status_t message_bus_publish_from_isr(msg_type_t type, const void *payload,
                                               size_t payload_size, uint8_t priority,
                                               bool *higher_priority_task_woken)
{
    if (!bus.initialized) return MSG_BUS_ERR_NOT_INIT;
    if (payload_size > MSG_MAX_PAYLOAD_BYTES) return MSG_BUS_ERR_TOO_LARGE;

    bus_message_t msg;
    msg.type = type;
    msg.timestamp_us = xTaskGetTickCount() * portTICK_PERIOD_MS * 1000;
    msg.payload_size = payload_size;
    msg.priority = priority;
    if (payload && payload_size > 0) {
        memcpy(msg.payload, payload, payload_size);
    }

    BaseType_t woken = pdFALSE;
    BaseType_t result = xQueueSendFromISR(bus.queue, &msg, &woken);
    if (higher_priority_task_woken) {
        *higher_priority_task_woken = (woken == pdTRUE);
    }
    return (result == pdTRUE) ? MSG_BUS_OK : MSG_BUS_ERR_FULL;
}

msg_bus_status_t message_bus_subscribe(msg_type_t type, msg_bus_callback_t callback,
                                        void *user_ctx)
{
    if (!bus.initialized) return MSG_BUS_ERR_NOT_INIT;
    if (type >= MSG_TYPE_COUNT || callback == NULL) return MSG_BUS_ERR_MAX_SUBS;

    xSemaphoreTake(bus.sub_lock, portMAX_DELAY);
    msg_bus_status_t status = MSG_BUS_OK;
    if (bus.subscriber_count[type] >= MSG_BUS_MAX_SUBSCRIBERS_PER_TYPE) {
        status = MSG_BUS_ERR_MAX_SUBS;
    } else {
        subscriber_entry_t *slot = &bus.subscribers[type][bus.subscriber_count[type]];
        slot->callback = callback;
        slot->user_ctx = user_ctx;
        bus.subscriber_count[type]++;
    }
    xSemaphoreGive(bus.sub_lock);
    return status;
}

void message_bus_dispatch_task(void *params)
{
    (void)params;
    bus_message_t msg;
    for (;;) {
        if (xQueueReceive(bus.queue, &msg, portMAX_DELAY) != pdTRUE) continue;

        xSemaphoreTake(bus.sub_lock, portMAX_DELAY);
        uint8_t count = bus.subscriber_count[msg.type];
        subscriber_entry_t local[MSG_BUS_MAX_SUBSCRIBERS_PER_TYPE];
        memcpy(local, bus.subscribers[msg.type], sizeof(subscriber_entry_t) * count);
        xSemaphoreGive(bus.sub_lock);

        for (uint8_t i = 0; i < count; i++) {
            local[i].callback(&msg, local[i].user_ctx);
        }
    }
}
