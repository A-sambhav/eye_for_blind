#include <string.h>
#include <stdlib.h>
#include "config_manager.h"
#include "FreeRTOS.h"
#include "semphr.h"

#define CONFIG_MAX_ENTRIES 48

typedef enum {
    TYPE_INT, TYPE_FLOAT, TYPE_BOOL, TYPE_STRING
} val_type_t;

typedef struct {
    char key[CONFIG_MAX_KEY_LEN];
    val_type_t type;
    int32_t val_int;
    float val_float;
    bool val_bool;
    char val_str[CONFIG_MAX_VAL_LEN];
    bool dirty;
} config_entry_t;

typedef struct {
    char prefix[CONFIG_MAX_KEY_LEN];
    config_change_cb_t cb;
    void *ctx;
} config_watcher_t;

static struct {
    config_entry_t entries[CONFIG_MAX_ENTRIES];
    uint32_t count;
    config_watcher_t watchers[8];
    uint32_t watcher_count;
    bool dirty;
    SemaphoreHandle_t lock;
    bool initialized;
} cfg;

static config_entry_t *find_entry(const char *key)
{
    for (uint32_t i = 0; i < cfg.count; i++)
        if (strncmp(cfg.entries[i].key, key, CONFIG_MAX_KEY_LEN - 1) == 0)
            return &cfg.entries[i];
    return NULL;
}

static uint32_t add_entry(const char *key)
{
    if (cfg.count >= CONFIG_MAX_ENTRIES) return ~0U;
    uint32_t i = cfg.count++;
    strncpy(cfg.entries[i].key, key, CONFIG_MAX_KEY_LEN - 1);
    cfg.entries[i].dirty = true;
    return i;
}

static void notify_watchers(const char *key)
{
    for (uint32_t i = 0; i < cfg.watcher_count; i++) {
        if (strncmp(key, cfg.watchers[i].prefix,
                    strlen(cfg.watchers[i].prefix)) == 0) {
            if (cfg.watchers[i].cb)
                cfg.watchers[i].cb(key, cfg.watchers[i].ctx);
        }
    }
}

static void set_defaults(void)
{
    const struct { const char *k; int32_t v; } int_defaults[] = {
        {"system.log_level", 1},
        {"system.speech_volume", 80},
        {"system.fall_detect_enabled", 1},
        {"system.obstacle_warn_distance_cm", 200},
        {"system.step_count", 0},
    };
    const struct { const char *k; float v; } float_defaults[] = {
        {"system.obstacle_alert_distance_m", 2.0f},
        {"depth.confidence_threshold", 0.5f},
        {"depth.min_valid_m", 0.1f},
        {"depth.max_valid_m", 20.0f},
        {"od.confidence_threshold", 0.5f},
    };
    const struct { const char *k; bool v; } bool_defaults[] = {
        {"system.first_boot", true},
        {"system.ble_enabled", false},
        {"system.wifi_enabled", false},
        {"depth.temporal_filter", true},
        {"voice.wake_word_enabled", true},
    };
    const struct { const char *k; const char *v; } str_defaults[] = {
        {"system.mode", "blind_assist"},
        {"system.language", "en"},
        {"voice.wake_word", "hey glass"},
    };

    for (uint32_t i = 0; i < sizeof(int_defaults)/sizeof(int_defaults[0]); i++) {
        uint32_t idx = add_entry(int_defaults[i].k);
        if (idx < CONFIG_MAX_ENTRIES) {
            cfg.entries[idx].type = TYPE_INT;
            cfg.entries[idx].val_int = int_defaults[i].v;
        }
    }
    for (uint32_t i = 0; i < sizeof(float_defaults)/sizeof(float_defaults[0]); i++) {
        uint32_t idx = add_entry(float_defaults[i].k);
        if (idx < CONFIG_MAX_ENTRIES) {
            cfg.entries[idx].type = TYPE_FLOAT;
            cfg.entries[idx].val_float = float_defaults[i].v;
        }
    }
    for (uint32_t i = 0; i < sizeof(bool_defaults)/sizeof(bool_defaults[0]); i++) {
        uint32_t idx = add_entry(bool_defaults[i].k);
        if (idx < CONFIG_MAX_ENTRIES) {
            cfg.entries[idx].type = TYPE_BOOL;
            cfg.entries[idx].val_bool = bool_defaults[i].v;
        }
    }
    for (uint32_t i = 0; i < sizeof(str_defaults)/sizeof(str_defaults[0]); i++) {
        uint32_t idx = add_entry(str_defaults[i].k);
        if (idx < CONFIG_MAX_ENTRIES) {
            cfg.entries[idx].type = TYPE_STRING;
            strncpy(cfg.entries[idx].val_str, str_defaults[i].v, CONFIG_MAX_VAL_LEN - 1);
        }
    }
    cfg.dirty = true;
}

config_status_t config_init(void)
{
    memset(&cfg, 0, sizeof(cfg));
    cfg.lock = xSemaphoreCreateMutex();
    if (cfg.lock == NULL) return CONFIG_ERR_CORRUPT;
    cfg.initialized = true;
    set_defaults();
    return CONFIG_OK;
}

int32_t config_get_int(const char *key, int32_t default_val)
{
    if (!cfg.initialized || key == NULL) return default_val;
    xSemaphoreTake(cfg.lock, portMAX_DELAY);
    config_entry_t *e = find_entry(key);
    int32_t v = (e && e->type == TYPE_INT) ? e->val_int : default_val;
    xSemaphoreGive(cfg.lock);
    return v;
}

float config_get_float(const char *key, float default_val)
{
    if (!cfg.initialized || key == NULL) return default_val;
    xSemaphoreTake(cfg.lock, portMAX_DELAY);
    config_entry_t *e = find_entry(key);
    float v = (e && e->type == TYPE_FLOAT) ? e->val_float : default_val;
    xSemaphoreGive(cfg.lock);
    return v;
}

bool config_get_bool(const char *key, bool default_val)
{
    if (!cfg.initialized || key == NULL) return default_val;
    xSemaphoreTake(cfg.lock, portMAX_DELAY);
    config_entry_t *e = find_entry(key);
    bool v = (e && e->type == TYPE_BOOL) ? e->val_bool : default_val;
    xSemaphoreGive(cfg.lock);
    return v;
}

config_status_t config_get_string(const char *key, char *out,
                                   size_t out_len, const char *default_val)
{
    if (!cfg.initialized || key == NULL || out == NULL) return CONFIG_ERR_NOT_FOUND;
    xSemaphoreTake(cfg.lock, portMAX_DELAY);
    config_entry_t *e = find_entry(key);
    if (e && e->type == TYPE_STRING) {
        strncpy(out, e->val_str, out_len - 1);
        out[out_len - 1] = '\0';
    } else if (default_val) {
        strncpy(out, default_val, out_len - 1);
        out[out_len - 1] = '\0';
    } else {
        xSemaphoreGive(cfg.lock);
        return CONFIG_ERR_NOT_FOUND;
    }
    xSemaphoreGive(cfg.lock);
    return CONFIG_OK;
}

static config_status_t set_entry(const char *key, val_type_t type,
                                  int32_t vi, float vf, bool vb, const char *vs)
{
    if (!cfg.initialized || key == NULL) return CONFIG_ERR_NOT_FOUND;
    xSemaphoreTake(cfg.lock, portMAX_DELAY);
    config_entry_t *e = find_entry(key);
    if (e == NULL) {
        uint32_t idx = add_entry(key);
        if (idx >= CONFIG_MAX_ENTRIES) { xSemaphoreGive(cfg.lock); return CONFIG_ERR_FLASH_WRITE; }
        e = &cfg.entries[idx];
    }
    e->type = type;
    e->dirty = true;
    cfg.dirty = true;
    if (type == TYPE_INT) e->val_int = vi;
    else if (type == TYPE_FLOAT) e->val_float = vf;
    else if (type == TYPE_BOOL) e->val_bool = vb;
    else if (type == TYPE_STRING && vs) strncpy(e->val_str, vs, CONFIG_MAX_VAL_LEN - 1);
    xSemaphoreGive(cfg.lock);
    notify_watchers(key);
    return CONFIG_OK;
}

config_status_t config_set_int(const char *key, int32_t val)
{
    return set_entry(key, TYPE_INT, val, 0, false, NULL);
}

config_status_t config_set_float(const char *key, float val)
{
    return set_entry(key, TYPE_FLOAT, 0, val, false, NULL);
}

config_status_t config_set_bool(const char *key, bool val)
{
    return set_entry(key, TYPE_BOOL, val ? 1 : 0, 0, val, NULL);
}

config_status_t config_set_string(const char *key, const char *val)
{
    return set_entry(key, TYPE_STRING, 0, 0, false, val);
}

config_status_t config_register_callback(const char *key_prefix,
                                           config_change_cb_t cb, void *ctx)
{
    if (!cfg.initialized || key_prefix == NULL || cb == NULL || cfg.watcher_count >= 8)
        return CONFIG_ERR_NOT_FOUND;
    xSemaphoreTake(cfg.lock, portMAX_DELAY);
    uint32_t i = cfg.watcher_count++;
    strncpy(cfg.watchers[i].prefix, key_prefix, CONFIG_MAX_KEY_LEN - 1);
    cfg.watchers[i].cb = cb;
    cfg.watchers[i].ctx = ctx;
    xSemaphoreGive(cfg.lock);
    return CONFIG_OK;
}

config_status_t config_save(void)
{
    (void)cfg;
    return CONFIG_OK;
}

config_status_t config_load_defaults(void)
{
    if (!cfg.initialized) return CONFIG_ERR_NOT_FOUND;
    xSemaphoreTake(cfg.lock, portMAX_DELAY);
    cfg.count = 0;
    set_defaults();
    xSemaphoreGive(cfg.lock);
    return CONFIG_OK;
}

config_status_t config_deinit(void)
{
    cfg.initialized = false;
    return CONFIG_OK;
}
