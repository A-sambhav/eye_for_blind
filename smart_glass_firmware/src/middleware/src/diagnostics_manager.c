#include <string.h>
#include "diagnostics_manager.h"
#include "logging_manager.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

static struct {
    diag_config_t config;
    diag_module_status_t modules[DIAG_MAX_MODULES];
    diag_status_cb_t status_cbs[DIAG_MAX_MODULES];
    diag_self_test_cb_t test_cbs[DIAG_MAX_MODULES];
    uint32_t count;
    uint32_t last_check_tick;
    uint32_t self_test_seq;
    SemaphoreHandle_t lock;
    bool initialized;
} diag;

static uint32_t now_ms(void)
{
    return xTaskGetTickCount() * portTICK_PERIOD_MS;
}

static int find_module(const char *name)
{
    for (uint32_t i = 0; i < diag.count; i++)
        if (strncmp(diag.modules[i].name, name, sizeof(diag.modules[i].name) - 1) == 0)
            return (int)i;
    return -1;
}

static void check_module(uint32_t idx)
{
    diag_module_status_t *m = &diag.modules[idx];
    m->total_checks++;
    m->last_response_tick = now_ms();

    if (diag.status_cbs[idx]) {
        diag_status_t s = diag.status_cbs[idx]();
        m->status = s;
        if (s != DIAG_OK) {
            m->error_count++;
        }
    }
}

diag_status_t diag_init(const diag_config_t *config)
{
    if (config == NULL) return DIAG_ERROR;
    memset(&diag, 0, sizeof(diag));
    diag.config = *config;
    if (diag.config.check_interval_ms == 0) diag.config.check_interval_ms = 5000;
    if (diag.config.response_timeout_ms == 0) diag.config.response_timeout_ms = 1000;
    if (diag.config.self_test_interval_ms == 0) diag.config.self_test_interval_ms = 60000;

    diag.lock = xSemaphoreCreateMutex();
    if (diag.lock == NULL) return DIAG_ERROR;
    diag.initialized = true;
    return DIAG_OK;
}

diag_status_t diag_register_module(const char *name,
                                    diag_status_cb_t status_cb,
                                    diag_self_test_cb_t test_cb)
{
    if (!diag.initialized || name == NULL) return DIAG_ERROR;
    if (diag.count >= DIAG_MAX_MODULES) return DIAG_ERROR;

    xSemaphoreTake(diag.lock, portMAX_DELAY);
    uint32_t i = diag.count++;
    strncpy(diag.modules[i].name, name, sizeof(diag.modules[i].name) - 1);
    diag.modules[i].status = DIAG_NOT_TESTED;
    diag.modules[i].last_response_tick = now_ms();
    diag.status_cbs[i] = status_cb;
    diag.test_cbs[i] = test_cb;
    xSemaphoreGive(diag.lock);
    return DIAG_OK;
}

diag_status_t diag_run_self_test(const char *module)
{
    if (!diag.initialized) return DIAG_ERROR;
    int idx = module ? find_module(module) : -1;
    if (module && idx < 0) return DIAG_ERROR;

    xSemaphoreTake(diag.lock, portMAX_DELAY);
    diag.self_test_seq++;

    if (idx >= 0) {
        if (diag.test_cbs[idx]) {
            uint32_t result = 0;
            diag_status_t s = diag.test_cbs[idx](&result);
            diag.modules[idx].self_test_result = result;
            diag.modules[idx].status = s;
        }
    } else {
        for (uint32_t i = 0; i < diag.count; i++) {
            if (diag.test_cbs[i]) {
                uint32_t result = 0;
                diag_status_t s = diag.test_cbs[i](&result);
                diag.modules[i].self_test_result = result;
                diag.modules[i].status = s;
            }
        }
    }

    xSemaphoreGive(diag.lock);
    return DIAG_OK;
}

diag_status_t diag_run_all_tests(void)
{
    return diag_run_self_test(NULL);
}

diag_status_t diag_get_health(diag_health_t *out)
{
    if (!diag.initialized || out == NULL) return DIAG_ERROR;

    uint32_t t = now_ms();
    if (t - diag.last_check_tick >= diag.config.check_interval_ms) {
        xSemaphoreTake(diag.lock, portMAX_DELAY);
        for (uint32_t i = 0; i < diag.count; i++) check_module(i);
        diag.last_check_tick = t;
        xSemaphoreGive(diag.lock);
    }

    xSemaphoreTake(diag.lock, portMAX_DELAY);
    memcpy(out->modules, diag.modules, sizeof(diag_module_status_t) * diag.count);
    out->count = diag.count;
    out->check_interval_ms = diag.config.check_interval_ms;
    out->response_timeout_ms = diag.config.response_timeout_ms;
    xSemaphoreGive(diag.lock);
    return DIAG_OK;
}

diag_status_t diag_get_module_status(const char *name,
                                      diag_module_status_t *out)
{
    if (!diag.initialized || name == NULL || out == NULL) return DIAG_ERROR;
    int idx = find_module(name);
    if (idx < 0) return DIAG_ERROR;

    xSemaphoreTake(diag.lock, portMAX_DELAY);
    *out = diag.modules[idx];
    xSemaphoreGive(diag.lock);
    return DIAG_OK;
}

diag_status_t diag_collect_perf(diag_perf_t *out)
{
    if (!diag.initialized || out == NULL) return DIAG_ERROR;
    out->free_heap_bytes = (uint32_t)xPortGetFreeHeapSize();
    out->min_free_heap_bytes = (uint32_t)xPortGetMinimumEverFreeHeapSize();

    out->stack_min_free_words[0] = (uint32_t)uxTaskGetStackHighWaterMark(NULL);
    out->stack_min_free_words[1] = 0;

    out->cpu_usage_pct = 0.0f;
    out->temperature_c = 25.0f;
    out->battery_voltage = 0.0f;
    out->battery_current = 0.0f;
    out->battery_soc = 0;
    return DIAG_OK;
}

diag_status_t diag_deinit(void)
{
    diag.initialized = false;
    return DIAG_OK;
}
