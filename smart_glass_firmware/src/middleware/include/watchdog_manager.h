#ifndef WATCHDOG_MANAGER_H
#define WATCHDOG_MANAGER_H

#include <stdint.h>
#include <stdbool.h>
#include "FreeRTOS.h"
#include "task.h"

#define WDOG_MAX_TASKS 16
#define WDOG_TASK_NAME_LEN 16

typedef enum {
    WDOG_OK = 0,
    WDOG_ERR_NOT_INIT,
    WDOG_ERR_MAX_TASKS,
    WDOG_ERR_TIMEOUT
} wdog_status_t;

typedef struct {
    TaskHandle_t task;
    char name[WDOG_TASK_NAME_LEN];
    uint32_t timeout_ms;
    uint32_t last_kick_tick;
    uint32_t kick_count;
    uint32_t miss_count;
    bool enabled;
    bool expired;
} wdog_task_entry_t;

typedef struct {
    wdog_task_entry_t tasks[WDOG_MAX_TASKS];
    uint32_t task_count;
    uint32_t monitor_interval_ms;
} wdog_status_info_t;

typedef struct {
    uint32_t hardware_timeout_ms;
    uint32_t monitor_interval_ms;
    uint32_t default_task_timeout_ms;
    bool enable_hardware_wdog;
    bool auto_recover_tasks;
} wdog_config_t;

wdog_status_t wdog_init(const wdog_config_t *config);
wdog_status_t wdog_register_task(TaskHandle_t task, const char *name,
                                  uint32_t timeout_ms);
wdog_status_t wdog_kick(TaskHandle_t task);
wdog_status_t wdog_kick_all(void);
wdog_status_t wdog_get_status(wdog_status_info_t *out);
wdog_status_t wdog_get_task_status(const char *name,
                                    wdog_task_entry_t *out);
wdog_status_t wdog_deinit(void);

#endif /* WATCHDOG_MANAGER_H */
