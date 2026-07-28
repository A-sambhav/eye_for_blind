#ifndef MESSAGE_BUS_H
#define MESSAGE_BUS_H

#include <stdint.h>
#include <stddef.h>
#include "message_types.h"

typedef void (*bus_subscriber_fn)(const bus_message_t *msg, void *user_ctx);

typedef enum { MSG_BUS_OK = 0, MSG_BUS_ERR_FULL, MSG_BUS_ERR_INVALID } msg_bus_status_t;

msg_bus_status_t message_bus_init(void);
msg_bus_status_t message_bus_publish(msg_type_t type, const void *payload,
                                      uint16_t payload_size, uint8_t priority);
msg_bus_status_t message_bus_subscribe(msg_type_t type,
                                        bus_subscriber_fn callback,
                                        void *user_ctx);
msg_bus_status_t message_bus_unsubscribe(msg_type_t type,
                                          bus_subscriber_fn callback);
void message_bus_dispatch(void);

#endif
