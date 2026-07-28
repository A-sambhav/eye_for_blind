#include "task_manager.h"
#include "logging_manager.h"
#include "FreeRTOS.h"
#include "task.h"

#define STUB_LOOP(task_id, period_ms)                                   \
    do {                                                                \
        TickType_t last_wake = xTaskGetTickCount();                     \
        for (;;) {                                                      \
            task_manager_feed_watchdog(task_id);                        \
            vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(period_ms));       \
        }                                                                \
    } while (0)

void ai_task_entry(void *params)           { (void)params; STUB_LOOP(TASK_AI, 33); }
void navigation_task_entry(void *params)   { (void)params; STUB_LOOP(TASK_NAVIGATION, 100); }
void voice_task_entry(void *params)        { (void)params; STUB_LOOP(TASK_VOICE, 50); }
void bms_task_entry(void *params)          { (void)params; STUB_LOOP(TASK_BMS, 1000); }

void log_task_entry(void *params)
{
    (void)params;
    TickType_t last_wake = xTaskGetTickCount();
    for (;;) {
        task_manager_feed_watchdog(TASK_LOG);
        log_flush();
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(100));
    }
}
