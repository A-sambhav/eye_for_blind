#include <string.h>
#include "watchdog_manager.h"
#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"
#include "semphr.h"

static struct {
    wdog_config_t config;
    wdog_task_entry_t tasks[WDOG_MAX_TASKS];
    uint32_t task_count;
    SemaphoreHandle_t lock;
    TimerHandle_t monitor_timer;
    bool initialized;
} wd;

static uint32_t now_ms(void)
{
    return xTaskGetTickCount() * portTICK_PERIOD_MS;
}

static void monitor_callback(TimerHandle_t xTimer)
{
    (void)xTimer;
    if (!wd.initialized) return;
    xSemaphoreTake(wd.lock, 0);

    uint32_t t = now_ms();
    for (uint32_t i = 0; i < wd.task_count; i++) {
        wdog_task_entry_t *e = &wd.tasks[i];
        if (!e->enabled || e->expired) continue;
        if (t - e->last_kick_tick > e->timeout_ms) {
            e->miss_count++;
            e->expired = true;
        }
    }

    xSemaphoreGive(wd.lock);
}

static int find_task(TaskHandle_t task)
{
    for (uint32_t i = 0; i < wd.task_count; i++)
        if (wd.tasks[i].task == task) return (int)i;
    return -1;
}

static int find_task_by_name(const char *name)
{
    for (uint32_t i = 0; i < wd.task_count; i++)
        if (strncmp(wd.tasks[i].name, name, WDOG_TASK_NAME_LEN - 1) == 0)
            return (int)i;
    return -1;
}

wdog_status_t wdog_init(const wdog_config_t *config)
{
    if (config == NULL) return WDOG_ERR_NOT_INIT;
    memset(&wd, 0, sizeof(wd));
    wd.config = *config;
    if (wd.config.hardware_timeout_ms == 0) wd.config.hardware_timeout_ms = 5000;
    if (wd.config.monitor_interval_ms == 0) wd.config.monitor_interval_ms = 100;
    if (wd.config.default_task_timeout_ms == 0) wd.config.default_task_timeout_ms = 500;

    wd.lock = xSemaphoreCreateMutex();
    if (wd.lock == NULL) return WDOG_ERR_NOT_INIT;

    wd.monitor_timer = xTimerCreate("wdog_mon", pdMS_TO_TICKS(wd.config.monitor_interval_ms),
                                     pdTRUE, NULL, monitor_callback);
    if (wd.monitor_timer == NULL) return WDOG_ERR_NOT_INIT;

    wd.initialized = true;
    xTimerStart(wd.monitor_timer, 0);
    return WDOG_OK;
}

wdog_status_t wdog_register_task(TaskHandle_t task, const char *name,
                                  uint32_t timeout_ms)
{
    if (!wd.initialized) return WDOG_ERR_NOT_INIT;
    if (wd.task_count >= WDOG_MAX_TASKS) return WDOG_ERR_MAX_TASKS;

    xSemaphoreTake(wd.lock, portMAX_DELAY);
    uint32_t i = wd.task_count++;
    wd.tasks[i].task = task;
    strncpy(wd.tasks[i].name, name ? name : "unknown", WDOG_TASK_NAME_LEN - 1);
    wd.tasks[i].timeout_ms = timeout_ms > 0 ? timeout_ms : wd.config.default_task_timeout_ms;
    wd.tasks[i].last_kick_tick = now_ms();
    wd.tasks[i].enabled = true;
    wd.tasks[i].expired = false;
    xSemaphoreGive(wd.lock);
    return WDOG_OK;
}

wdog_status_t wdog_kick(TaskHandle_t task)
{
    if (!wd.initialized) return WDOG_ERR_NOT_INIT;
    int idx = find_task(task);
    if (idx < 0) return WDOG_ERR_TIMEOUT;
    xSemaphoreTake(wd.lock, portMAX_DELAY);
    wdog_task_entry_t *e = &wd.tasks[idx];
    e->last_kick_tick = now_ms();
    e->kick_count++;
    e->expired = false;
    xSemaphoreGive(wd.lock);
    return WDOG_OK;
}

wdog_status_t wdog_kick_all(void)
{
    if (!wd.initialized) return WDOG_ERR_NOT_INIT;
    uint32_t t = now_ms();
    xSemaphoreTake(wd.lock, portMAX_DELAY);
    for (uint32_t i = 0; i < wd.task_count; i++) {
        wd.tasks[i].last_kick_tick = t;
        wd.tasks[i].kick_count++;
        wd.tasks[i].expired = false;
    }
    xSemaphoreGive(wd.lock);
    return WDOG_OK;
}

wdog_status_t wdog_get_status(wdog_status_info_t *out)
{
    if (!wd.initialized || out == NULL) return WDOG_ERR_NOT_INIT;
    xSemaphoreTake(wd.lock, portMAX_DELAY);
    memcpy(out->tasks, wd.tasks, sizeof(wdog_task_entry_t) * wd.task_count);
    out->task_count = wd.task_count;
    out->monitor_interval_ms = wd.config.monitor_interval_ms;
    xSemaphoreGive(wd.lock);
    return WDOG_OK;
}

wdog_status_t wdog_get_task_status(const char *name,
                                    wdog_task_entry_t *out)
{
    if (!wd.initialized || name == NULL || out == NULL) return WDOG_ERR_NOT_INIT;
    int idx = find_task_by_name(name);
    if (idx < 0) return WDOG_ERR_TIMEOUT;
    xSemaphoreTake(wd.lock, portMAX_DELAY);
    *out = wd.tasks[idx];
    xSemaphoreGive(wd.lock);
    return WDOG_OK;
}

wdog_status_t wdog_deinit(void)
{
    if (wd.monitor_timer) {
        xTimerStop(wd.monitor_timer, 0);
        xTimerDelete(wd.monitor_timer, 0);
    }
    wd.initialized = false;
    return WDOG_OK;
}
