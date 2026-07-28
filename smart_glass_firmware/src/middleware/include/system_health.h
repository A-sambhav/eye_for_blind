#ifndef SYSTEM_HEALTH_H
#define SYSTEM_HEALTH_H

#include <stdbool.h>
#include <stdint.h>
#include "task_manager.h"

#define WATCHDOG_TIMEOUT_MS 500
#define WATCHDOG_MAX_TASKS  TASK_COUNT

typedef struct {
    uint32_t last_feed_ms;
    uint32_t missed_deadlines;
    bool in_use;
} watchdog_slot_t;

typedef enum {
    SYS_HEALTH_OK = 0,
    SYS_HEALTH_ERR_INIT,
    SYS_HEALTH_ERR_NOT_READY
} sys_health_status_t;

void system_health_init(void);
void system_health_feed_watchdog(task_id_t id);
void system_health_report_boot_failure(const char *component);
void system_health_note_bus_drop(void);
sys_health_status_t system_health_get_watchdog_state(task_id_t id, watchdog_slot_t *out);

#endif /* SYSTEM_HEALTH_H */
