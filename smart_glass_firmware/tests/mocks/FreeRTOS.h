#ifndef FREERTOS_H
#define FREERTOS_H

#include <stdint.h>
#include <stddef.h>

#define configSTACK_DEPTH_TYPE uint16_t
#define portTICK_PERIOD_MS 1
#define configTICK_RATE_HZ 1000
#define portMAX_DELAY ((TickType_t)0xFFFFFFFF)
#define portMUX_TYPE int

typedef unsigned long TickType_t;
typedef long BaseType_t;
typedef unsigned long UBaseType_t;

#ifndef pdMS_TO_TICKS
#define pdMS_TO_TICKS(x) ((TickType_t)(x))
#endif

#define pdTRUE 1
#define pdFALSE 0
#define pdPASS 1
#define pdFAIL 0

#define taskENTER_CRITICAL()
#define taskEXIT_CRITICAL()

#define configUSE_PREEMPTION 1
#define configUSE_PORT_OPTIMISED_TASK_SELECTION 0
#define configTICK_RATE_HZ 1000
#define configMINIMAL_STACK_SIZE 128
#define configMAX_TASK_NAME_LEN 16
#define configUSE_TRACE_FACILITY 0
#define configUSE_STATS_FORMATTING_FUNCTIONS 0
#define configUSE_16_BIT_TICKS 0
#define configIDLE_SHOULD_YIELD 1
#define configUSE_MUTEXES 1
#define configQUEUE_REGISTRY_SIZE 0
#define configCHECK_FOR_STACK_OVERFLOW 0
#define configUSE_RECURSIVE_MUTEXES 0
#define configUSE_MALLOC_FAILED_HOOK 0
#define configUSE_APPLICATION_TASK_TAG 0
#define configUSE_CO_ROUTINES 0
#define configMAX_CO_ROUTINE_PRIORITIES 0
#define configUSE_TIMERS 1
#define configTIMER_TASK_PRIORITY 3
#define configTIMER_QUEUE_LENGTH 10
#define configTIMER_TASK_STACK_DEPTH 256

#define INCLUDE_xTaskGetTickCount 1
#define INCLUDE_vTaskDelay 1
#define INCLUDE_vTaskDelayUntil 1
#define INCLUDE_xSemaphoreCreateMutex 1
#define INCLUDE_xQueueGenericSend 1
#define INCLUDE_xQueueReceive 1
#define INCLUDE_xTaskGetCurrentTaskHandle 0
#define INCLUDE_xTaskGetIdleTaskHandle 0

#endif
