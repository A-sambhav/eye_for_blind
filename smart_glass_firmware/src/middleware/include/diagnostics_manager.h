#ifndef DIAGNOSTICS_MANAGER_H
#define DIAGNOSTICS_MANAGER_H

#include <stdint.h>
#include <stdbool.h>

#define DIAG_MAX_MODULES 32

typedef enum {
    DIAG_OK = 0,
    DIAG_WARNING,
    DIAG_ERROR,
    DIAG_NOT_RESPONDING,
    DIAG_NOT_TESTED
} diag_status_t;

typedef diag_status_t (*diag_status_cb_t)(void);
typedef diag_status_t (*diag_self_test_cb_t)(uint32_t *out_result);

typedef struct {
    char name[16];
    diag_status_t status;
    uint32_t last_response_tick;
    uint32_t error_count;
    uint32_t total_checks;
    char last_error[128];
    uint32_t self_test_result;
} diag_module_status_t;

typedef struct {
    diag_module_status_t modules[DIAG_MAX_MODULES];
    uint32_t count;
    uint32_t check_interval_ms;
    uint32_t response_timeout_ms;
} diag_health_t;

typedef struct {
    float cpu_usage_pct;
    uint32_t free_heap_bytes;
    uint32_t min_free_heap_bytes;
    uint32_t stack_min_free_words[2];
    float temperature_c;
    float battery_voltage;
    float battery_current;
    uint8_t battery_soc;
} diag_perf_t;

typedef struct {
    uint32_t check_interval_ms;
    uint32_t response_timeout_ms;
    uint32_t self_test_interval_ms;
    bool enable_perf_collection;
} diag_config_t;

typedef struct {
    diag_status_t status;
    diag_health_t health;
    diag_perf_t perf;
} diag_result_t;

diag_status_t diag_init(const diag_config_t *config);
diag_status_t diag_register_module(const char *name,
                                    diag_status_cb_t status_cb,
                                    diag_self_test_cb_t test_cb);
diag_status_t diag_run_self_test(const char *module);
diag_status_t diag_run_all_tests(void);
diag_status_t diag_get_health(diag_health_t *out);
diag_status_t diag_get_module_status(const char *name,
                                      diag_module_status_t *out);
diag_status_t diag_collect_perf(diag_perf_t *out);
diag_status_t diag_deinit(void);

#endif /* DIAGNOSTICS_MANAGER_H */
