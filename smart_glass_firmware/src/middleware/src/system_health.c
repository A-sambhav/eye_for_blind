#include <string.h>
#include "system_health.h"
#include "task_manager.h"
#include "logging_manager.h"
#include "FreeRTOS.h"
#include "task.h"

static struct {
    watchdog_slot_t slots[WATCHDOG_MAX_TASKS];
    uint32_t bus_drop_count;
    uint32_t boot_failures;
    char boot_fail_component[32];
    bool initialized;
} health;

static uint32_t now_ms(void)
{
    return xTaskGetTickCount() * portTICK_PERIOD_MS;
}

void system_health_init(void)
{
    memset(&health, 0, sizeof(health));
    health.initialized = true;
}

void system_health_feed_watchdog(task_id_t id)
{
    if (!health.initialized || id >= WATCHDOG_MAX_TASKS) return;
    health.slots[id].last_feed_ms = now_ms();
    health.slots[id].in_use = true;
}

void system_health_report_boot_failure(const char *component)
{
    if (component == NULL) return;
    health.boot_failures++;
    strncpy(health.boot_fail_component, component, sizeof(health.boot_fail_component) - 1);
}

void system_health_note_bus_drop(void)
{
    health.bus_drop_count++;
}

sys_health_status_t system_health_get_watchdog_state(task_id_t id, watchdog_slot_t *out)
{
    if (!health.initialized) return SYS_HEALTH_ERR_INIT;
    if (id >= WATCHDOG_MAX_TASKS || out == NULL) return SYS_HEALTH_ERR_NOT_READY;
    *out = health.slots[id];
    return SYS_HEALTH_OK;
}
