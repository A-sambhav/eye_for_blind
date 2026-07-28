#include "FreeRTOS.h"
#include "task.h"
#include "system_health.h"
#include "logging_manager.h"

void vApplicationIdleHook(void)
{
    static TickType_t last_health = 0;
    TickType_t now = xTaskGetTickCount();
    if (now - last_health >= pdMS_TO_TICKS(1000)) {
        last_health = now;
        if (!task_manager_all_healthy()) {
            log_warn("health", "Task watchdog timeout detected");
        }
    }
}

void vApplicationTickHook(void)
{
}

void vApplicationStackOverflowHook(TaskHandle_t task, char *task_name)
{
    (void)task;
    log_critical("fatal", "Stack overflow in %s", task_name);
    system_health_report_boot_failure(task_name);
    for (;;) { }
}

void vApplicationMallocFailedHook(void)
{
    log_critical("fatal", "malloc failed (heap exhausted)");
    system_health_report_boot_failure("heap");
    for (;;) { }
}
