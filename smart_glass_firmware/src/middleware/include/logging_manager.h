#ifndef LOGGING_MANAGER_H
#define LOGGING_MANAGER_H

#include <stdint.h>
#include <stdbool.h>

#define LOG_MAX_LINE 256
#define LOG_MAX_MODULE 16

typedef enum {
    LOG_DEBUG = 0,
    LOG_INFO = 1,
    LOG_WARNING = 2,
    LOG_ERROR = 3,
    LOG_CRITICAL = 4
} log_level_t;

typedef struct {
    log_level_t global_level;
    uint32_t flush_interval_ms;
    bool enable_ble_output;
    uint8_t max_rate_per_module;
} log_config_t;

typedef struct {
    uint32_t total_logs;
    uint32_t dropped_rate_limited;
    uint32_t flushed_to_db;
    uint32_t db_errors;
} log_stats_t;

typedef enum {
    LOG_OK = 0,
    LOG_ERR_NOT_INIT
} log_status_t;

log_status_t log_init(const log_config_t *config);
void log_debug(const char *module, const char *format, ...);
void log_info(const char *module, const char *format, ...);
void log_warn(const char *module, const char *format, ...);
void log_error(const char *module, const char *format, ...);
void log_critical(const char *module, const char *format, ...);
log_status_t log_set_level(const char *module, log_level_t level);
log_status_t log_get_stats(log_stats_t *out);
log_status_t log_flush(void);
log_status_t log_deinit(void);

#endif /* LOGGING_MANAGER_H */
