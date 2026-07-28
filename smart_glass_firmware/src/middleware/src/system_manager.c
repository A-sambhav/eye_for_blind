#include <string.h>
#include <stdio.h>
#include "system_manager.h"
#include "config_manager.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "message_bus.h"
#include "task_manager.h"

static struct {
    sys_mode_t mode;
    sys_module_entry_t modules[SYS_MAX_MODULES];
    uint32_t module_count;
    uint32_t boot_time_tick;
    uint32_t boot_count;
    uint32_t shutdown_count;
    uint32_t ota_count;
    SemaphoreHandle_t lock;
    bool initialized;
} sm;

static uint32_t now_ms(void)
{
    return xTaskGetTickCount() * portTICK_PERIOD_MS;
}

sys_status_t sys_manager_init(void)
{
    memset(&sm, 0, sizeof(sm));
    sm.lock = xSemaphoreCreateMutex();
    if (sm.lock == NULL) return SYS_ERR_MODULE_FAIL;

    sm.boot_count = config_get_int("system.boot_count", 0) + 1;
    sm.mode = (sys_mode_t)config_get_int("system.mode_id", 0);
    if ((uint32_t)sm.mode > kModeFactoryTest) sm.mode = kModeBlindAssist;
    sm.boot_time_tick = xTaskGetTickCount();
    config_set_int("system.boot_count", sm.boot_count);

    sm.initialized = true;
    return SYS_OK;
}

sys_status_t sys_manager_start(void)
{
    for (uint32_t order = 0; order < 20; order++) {
        for (uint32_t i = 0; i < sm.module_count; i++) {
            if (sm.modules[i].init_order == order && !sm.modules[i].initialized) {
                if (sm.modules[i].init_cb) {
                    sys_status_t r = sm.modules[i].init_cb();
                    if (r != SYS_OK) return r;
                }
                sm.modules[i].initialized = true;
            }
        }
    }

    task_manager_start_all();
    return SYS_OK;
}

#ifndef HOST_BUILD
#define SYS_WFI() __asm("wfi")
#define SYS_BKPT() __asm("bkpt #0")
#else
#define SYS_WFI()
#define SYS_BKPT()
#endif

sys_status_t sys_manager_shutdown(void)
{
    xSemaphoreTake(sm.lock, portMAX_DELAY);
    sm.shutdown_count++;
    config_set_int("system.shutdown_count", (int32_t)sm.shutdown_count);
    config_save();

    for (int32_t i = (int32_t)sm.module_count - 1; i >= 0; i--) {
        if (sm.modules[i].initialized && sm.modules[i].deinit_cb) {
            sm.modules[i].deinit_cb();
        }
    }
    xSemaphoreGive(sm.lock);
    vTaskSuspendAll();
    SYS_WFI();
}

sys_status_t sys_manager_reboot(void)
{
    config_save();
    for (int32_t i = (int32_t)sm.module_count - 1; i >= 0; i--) {
        if (sm.modules[i].initialized && sm.modules[i].deinit_cb)
            sm.modules[i].deinit_cb();
    }
    taskDISABLE_INTERRUPTS();
    SYS_BKPT();
}

sys_status_t sys_manager_factory_reset(void)
{
    config_load_defaults();
    config_save();
    sys_manager_reboot();
    return SYS_OK;
}

sys_status_t sys_manager_set_mode(sys_mode_t mode)
{
    if ((uint32_t)mode > kModeFactoryTest) return SYS_ERR_MODE_INVALID;
    xSemaphoreTake(sm.lock, portMAX_DELAY);
    sm.mode = mode;
    config_set_int("system.mode_id", (int32_t)mode);
    xSemaphoreGive(sm.lock);
    return SYS_OK;
}

sys_mode_t sys_manager_get_mode(void)
{
    return sm.mode;
}

sys_status_t sys_manager_get_info(sys_info_t *out)
{
    if (!sm.initialized || out == NULL) return SYS_ERR_MODULE_FAIL;
    xSemaphoreTake(sm.lock, portMAX_DELAY);
    out->uptime_seconds = (now_ms() - sm.boot_time_tick) / 1000;
    out->mode = sm.mode;
    snprintf(out->version, sizeof(out->version), "%d.%d.%d",
             SYS_VERSION_MAJOR, SYS_VERSION_MINOR, SYS_VERSION_PATCH);
    out->boot_count = sm.boot_count;
    out->shutdown_count = sm.shutdown_count;
    out->ota_count = sm.ota_count;
    out->free_heap = (uint32_t)xPortGetFreeHeapSize();
    out->min_free_heap = (uint32_t)xPortGetMinimumEverFreeHeapSize();
    xSemaphoreGive(sm.lock);
    return SYS_OK;
}

sys_status_t sys_manager_get_uptime(uint32_t *out_seconds)
{
    if (!sm.initialized || out_seconds == NULL) return SYS_ERR_MODULE_FAIL;
    *out_seconds = (now_ms() - sm.boot_time_tick) / 1000;
    return SYS_OK;
}

sys_status_t sys_manager_register_module(const char *name,
                                          module_init_cb_t init_cb,
                                          module_deinit_cb_t deinit_cb,
                                          uint32_t init_order)
{
    if (!sm.initialized || name == NULL || sm.module_count >= SYS_MAX_MODULES)
        return SYS_ERR_MODULE_FAIL;
    xSemaphoreTake(sm.lock, portMAX_DELAY);
    uint32_t i = sm.module_count++;
    strncpy(sm.modules[i].name, name, sizeof(sm.modules[i].name) - 1);
    sm.modules[i].init_cb = init_cb;
    sm.modules[i].deinit_cb = deinit_cb;
    sm.modules[i].init_order = init_order;
    sm.modules[i].initialized = false;
    xSemaphoreGive(sm.lock);
    return SYS_OK;
}

void sys_task_entry(void *params)
{
    (void)params;
    TickType_t last_wake = xTaskGetTickCount();
    for (;;) {
        task_manager_feed_watchdog(TASK_SYSTEM);
        sys_info_t info;
        if (sys_manager_get_info(&info) == SYS_OK) {
            (void)info;
        }
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(5000));
    }
}
