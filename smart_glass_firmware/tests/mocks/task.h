#ifndef TASK_H
#define TASK_H

#include "FreeRTOS.h"

typedef void *TaskHandle_t;
typedef void (*TaskFunction_t)(void *);

TickType_t xTaskGetTickCount(void);
void vTaskDelay(const TickType_t xTicksToDelay);
void vTaskDelayUntil(TickType_t *pxPreviousWakeTime, TickType_t xTimeIncrement);
BaseType_t xTaskCreate(TaskFunction_t pvTaskCode, const char *pcName,
                        configSTACK_DEPTH_TYPE usStackDepth, void *pvParameters,
                        UBaseType_t uxPriority, TaskHandle_t *pvCreatedTask);

#define tskIDLE_PRIORITY 0

#endif
