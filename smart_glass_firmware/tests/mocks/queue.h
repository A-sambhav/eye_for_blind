#ifndef QUEUE_H
#define QUEUE_H

#include "FreeRTOS.h"

typedef void *QueueHandle_t;

QueueHandle_t xQueueCreate(UBaseType_t uxQueueLength, UBaseType_t uxItemSize);
BaseType_t xQueueSend(QueueHandle_t xQueue, const void *pvItemToQueue, TickType_t xTicksToWait);
BaseType_t xQueueReceive(QueueHandle_t xQueue, void *pvBuffer, TickType_t xTicksToWait);
BaseType_t xQueueGenericSend(QueueHandle_t xQueue, const void *pvItemToQueue,
                              TickType_t xTicksToWait, BaseType_t xCopyPosition);
UBaseType_t uxQueueMessagesWaiting(QueueHandle_t xQueue);

#define queueSEND_TO_BACK 0
#define queueSEND_TO_FRONT 1

#endif
