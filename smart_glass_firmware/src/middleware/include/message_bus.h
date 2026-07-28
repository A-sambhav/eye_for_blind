#ifndef MESSAGE_BUS_H
#define MESSAGE_BUS_H

#include "message_types.h"

/* Max number of tasks that may subscribe to a single message type. */
#define MSG_BUS_MAX_SUBSCRIBERS_PER_TYPE 6

/* Subscriber callback, invoked from the bus dispatcher task context.
 * Callbacks must be short/non-blocking — heavy work should be handed off
 * via the subscriber's own task queue instead of running inline here. */
typedef void (*msg_bus_callback_t)(const bus_message_t *msg, void *user_ctx);

typedef enum {
    MSG_BUS_OK = 0,
    MSG_BUS_ERR_FULL,          /* internal queue full, message dropped   */
    MSG_BUS_ERR_TOO_LARGE,     /* payload exceeds MSG_MAX_PAYLOAD_BYTES  */
    MSG_BUS_ERR_NOT_INIT,
    MSG_BUS_ERR_MAX_SUBS
} msg_bus_status_t;

/* Must be called once during startup (see main.c) before any task
 * publishes or subscribes. Not thread-safe — call before scheduler start. */
msg_bus_status_t message_bus_init(void);

/* Publish a message. `priority` follows FreeRTOS queue-send semantics via
 * an internal priority queue: higher value = delivered first when the
 * dispatcher is backed up (e.g. MSG_FALL_DETECTED should outrank
 * MSG_BATTERY_LEVEL). Safe to call from ISR context via
 * message_bus_publish_from_isr instead. */
msg_bus_status_t message_bus_publish(msg_type_t type, const void *payload,
                                      size_t payload_size, uint8_t priority);

msg_bus_status_t message_bus_publish_from_isr(msg_type_t type, const void *payload,
                                               size_t payload_size, uint8_t priority,
                                               bool *higher_priority_task_woken);

/* Register `callback` to be invoked whenever a message of `type` is
 * dispatched. `user_ctx` is passed through unmodified — typically a
 * pointer to the subscribing task/module's own state struct. */
msg_bus_status_t message_bus_subscribe(msg_type_t type, msg_bus_callback_t callback,
                                        void *user_ctx);

/* Runs the dispatch loop. Intended to be the body of a dedicated FreeRTOS
 * task (see task_manager.c) — blocks on the internal queue and fans out
 * each message to subscribers in registration order. Does not return. */
void message_bus_dispatch_task(void *params);

#endif /* MESSAGE_BUS_H */
