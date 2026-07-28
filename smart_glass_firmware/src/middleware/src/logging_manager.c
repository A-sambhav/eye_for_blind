#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "logging_manager.h"
#include "message_bus.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#define LOG_RATE_LIMIT_PERIOD_TICKS 100

static struct {
    log_config_t config;
    log_level_t module_levels[LOG_MAX_MODULE];
    char module_names[LOG_MAX_MODULE][LOG_MAX_MODULE];
    uint8_t module_count;
    uint32_t last_log_tick[LOG_MAX_MODULE];
    uint32_t log_count[LOG_MAX_MODULE];
    log_stats_t stats;
    SemaphoreHandle_t lock;
    bool initialized;
} lg;

static const char *level_str(log_level_t l)
{
    switch (l) {
        case LOG_DEBUG:   return "DBG";
        case LOG_INFO:    return "INF";
        case LOG_WARNING: return "WRN";
        case LOG_ERROR:   return "ERR";
        case LOG_CRITICAL:return "CRT";
        default:          return "???";
    }
}

static int find_module(const char *module)
{
    for (uint8_t i = 0; i < lg.module_count; i++)
        if (strncmp(lg.module_names[i], module, LOG_MAX_MODULE - 1) == 0)
            return i;
    return -1;
}

static void vlog(log_level_t level, const char *module, const char *format, va_list args)
{
    if (!lg.initialized) return;

    xSemaphoreTake(lg.lock, portMAX_DELAY);

    lg.stats.total_logs++;

    if (level < lg.config.global_level) {
        xSemaphoreGive(lg.lock);
        return;
    }

    int idx = find_module(module);
    if (idx >= 0 && level < lg.module_levels[idx]) {
        xSemaphoreGive(lg.lock);
        return;
    }

    uint32_t now = xTaskGetTickCount();
    if (idx >= 0 && lg.config.max_rate_per_module > 0) {
        if (now - lg.last_log_tick[idx] < LOG_RATE_LIMIT_PERIOD_TICKS) {
            lg.log_count[idx]++;
            if (lg.log_count[idx] > lg.config.max_rate_per_module) {
                lg.stats.dropped_rate_limited++;
                xSemaphoreGive(lg.lock);
                return;
            }
        } else {
            lg.last_log_tick[idx] = now;
            lg.log_count[idx] = 1;
        }
    }

    char buf[LOG_MAX_LINE];
    int len = snprintf(buf, sizeof(buf), "[%s][%s] ", level_str(level), module);
    if (len < (int)sizeof(buf)) {
        vsnprintf(buf + len, (size_t)(sizeof(buf) - len), format, args);
    }
    buf[sizeof(buf) - 1] = '\0';

    xSemaphoreGive(lg.lock);
}

log_status_t log_init(const log_config_t *config)
{
    if (config == NULL) return LOG_ERR_NOT_INIT;
    memset(&lg, 0, sizeof(lg));
    lg.config = *config;
    if (lg.config.global_level > LOG_CRITICAL) lg.config.global_level = LOG_INFO;
    if (lg.config.flush_interval_ms == 0) lg.config.flush_interval_ms = 100;

    lg.lock = xSemaphoreCreateMutex();
    if (lg.lock == NULL) return LOG_ERR_NOT_INIT;
    lg.initialized = true;
    return LOG_OK;
}

void log_debug(const char *module, const char *format, ...)
{
    va_list args; va_start(args, format); vlog(LOG_DEBUG, module, format, args); va_end(args);
}

void log_info(const char *module, const char *format, ...)
{
    va_list args; va_start(args, format); vlog(LOG_INFO, module, format, args); va_end(args);
}

void log_warn(const char *module, const char *format, ...)
{
    va_list args; va_start(args, format); vlog(LOG_WARNING, module, format, args); va_end(args);
}

void log_error(const char *module, const char *format, ...)
{
    va_list args; va_start(args, format); vlog(LOG_ERROR, module, format, args); va_end(args);
}

void log_critical(const char *module, const char *format, ...)
{
    va_list args; va_start(args, format); vlog(LOG_CRITICAL, module, format, args); va_end(args);
}

log_status_t log_set_level(const char *module, log_level_t level)
{
    if (!lg.initialized || module == NULL) return LOG_ERR_NOT_INIT;
    xSemaphoreTake(lg.lock, portMAX_DELAY);
    int idx = find_module(module);
    if (idx < 0) {
        if (lg.module_count >= LOG_MAX_MODULE) {
            xSemaphoreGive(lg.lock);
            return LOG_ERR_NOT_INIT;
        }
        idx = lg.module_count++;
        strncpy(lg.module_names[idx], module, LOG_MAX_MODULE - 1);
    }
    lg.module_levels[idx] = level;
    xSemaphoreGive(lg.lock);
    return LOG_OK;
}

log_status_t log_get_stats(log_stats_t *out)
{
    if (!lg.initialized || out == NULL) return LOG_ERR_NOT_INIT;
    xSemaphoreTake(lg.lock, portMAX_DELAY);
    *out = lg.stats;
    xSemaphoreGive(lg.lock);
    return LOG_OK;
}

log_status_t log_flush(void)
{
    (void)lg;
    return LOG_OK;
}

log_status_t log_deinit(void)
{
    lg.initialized = false;
    return LOG_OK;
}
