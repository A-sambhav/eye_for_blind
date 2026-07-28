#include <string.h>
#include "task_manager.h"
#include "system_health.h"
#include "logging_manager.h"
#include "FreeRTOS.h"
#include "task.h"

extern void camera_task_entry(void *params);
extern void depth_task_entry(void *params);
extern void ai_task_entry(void *params);
extern void navigation_task_entry(void *params);
extern void voice_task_entry(void *params);
extern void dec_eng_task_entry(void *params);
extern void bms_task_entry(void *params);
extern void sys_task_entry(void *params);
extern void log_task_entry(void *params);
extern void message_bus_dispatch_task(void *params);

static const task_def_t task_table[TASK_COUNT] = {
    [TASK_CAMERA]           = {TASK_CAMERA,           "camera",     camera_task_entry,           2048, 5, 33,   33},
    [TASK_DEPTH]            = {TASK_DEPTH,            "depth",      depth_task_entry,            1024, 4, 33,   33},
    [TASK_AI]               = {TASK_AI,               "ai",         ai_task_entry,               4096, 4, 33,   66},
    [TASK_NAVIGATION]       = {TASK_NAVIGATION,       "navigation", navigation_task_entry,       2048, 3, 100, 100},
    [TASK_VOICE]            = {TASK_VOICE,            "voice",      voice_task_entry,            4096, 3, 50,  100},
    [TASK_DECISION_ENGINE]  = {TASK_DECISION_ENGINE,  "dec_eng",    dec_eng_task_entry,          2048, 2, 66,  100},
    [TASK_BMS]              = {TASK_BMS,              "bms",        bms_task_entry,               512, 1, 1000, 2000},
    [TASK_SYSTEM]           = {TASK_SYSTEM,           "system",     sys_task_entry,               512, 1, 5000, 10000},
    [TASK_LOG]              = {TASK_LOG,              "log",        log_task_entry,              1024, 0, 100, 0},
    [TASK_MSG_BUS_DISPATCH] = {TASK_MSG_BUS_DISPATCH,  "msg_bus",    message_bus_dispatch_task,   1024, 4, 0,   0},
};

static TaskHandle_t task_handles[TASK_COUNT];

void task_manager_start_all(void)
{
    for (int i = 0; i < TASK_COUNT; i++) {
        const task_def_t *def = &task_table[i];
        BaseType_t result = xTaskCreate(
            def->entry, def->name, def->stack_words,
            NULL, def->priority, &task_handles[i]
        );
        if (result != pdPASS) {
            log_critical("task_mgr", "Failed to create %s", def->name);
            for (;;) { }
        }
    }
    vTaskStartScheduler();
    log_critical("task_mgr", "Scheduler start failed");
    for (;;) { }
}

void task_manager_feed_watchdog(task_id_t id)
{
    system_health_feed_watchdog(id);
}

static uint32_t now_ms(void)
{
    return xTaskGetTickCount() * portTICK_PERIOD_MS;
}

bool task_manager_all_healthy(void)
{
    uint32_t t = now_ms();
    for (int i = 0; i < TASK_COUNT; i++) {
        watchdog_slot_t slot;
        if (system_health_get_watchdog_state((task_id_t)i, &slot) == SYS_HEALTH_OK && slot.in_use) {
            if (t - slot.last_feed_ms > WATCHDOG_TIMEOUT_MS) {
                return false;
            }
        }
    }
    return true;
}

task_def_t *task_manager_get_def(task_id_t id)
{
    if (id < TASK_COUNT) {
        return (task_def_t *)&task_table[id];
    }
    return NULL;
}
