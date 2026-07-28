#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <stdint.h>
#include <stdbool.h>

#define CONFIG_MAX_KEY_LEN 64
#define CONFIG_MAX_VAL_LEN 256

typedef enum {
    CONFIG_OK = 0,
    CONFIG_ERR_NOT_FOUND,
    CONFIG_ERR_TYPE_MISMATCH,
    CONFIG_ERR_FLASH_WRITE,
    CONFIG_ERR_CORRUPT
} config_status_t;

typedef void (*config_change_cb_t)(const char *key, void *user_ctx);

config_status_t config_init(void);
int32_t config_get_int(const char *key, int32_t default_val);
float config_get_float(const char *key, float default_val);
bool config_get_bool(const char *key, bool default_val);
config_status_t config_get_string(const char *key, char *out,
                                   size_t out_len, const char *default_val);
config_status_t config_set_int(const char *key, int32_t val);
config_status_t config_set_float(const char *key, float val);
config_status_t config_set_bool(const char *key, bool val);
config_status_t config_set_string(const char *key, const char *val);
config_status_t config_register_callback(const char *key_prefix,
                                          config_change_cb_t cb, void *ctx);
config_status_t config_save(void);
config_status_t config_load_defaults(void);
config_status_t config_deinit(void);

#endif /* CONFIG_MANAGER_H */
