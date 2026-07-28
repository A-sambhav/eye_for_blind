#ifndef REMINDER_MANAGER_H
#define REMINDER_MANAGER_H

#include <stdint.h>
#include <stdbool.h>

#define MAX_REMINDERS 64
#define REMINDER_TEXT_LEN 128

typedef enum {
    kReminderTime,
    kReminderLocation,
    kReminderBoth
} reminder_trigger_t;

typedef enum {
    kRepeatNone,
    kRepeatDaily,
    kRepeatWeekly,
    kRepeatMonthly
} reminder_repeat_t;

typedef struct {
    uint32_t reminder_id;
    char text[REMINDER_TEXT_LEN];
    reminder_trigger_t trigger;
    reminder_repeat_t repeat;
    uint32_t repeat_interval_s;
    uint32_t start_timestamp;
    uint32_t end_timestamp;
    double latitude, longitude;
    float radius_m;
    bool acknowledged;
    bool active;
    uint8_t priority;
    uint32_t last_triggered;
    uint32_t trigger_count;
} reminder_t;

typedef struct {
    uint32_t check_interval_ms;
    uint32_t advance_warning_s;
    bool enable_location_reminders;
    uint32_t max_reminders;
} reminder_config_t;

typedef enum {
    REMINDER_OK = 0,
    REMINDER_ERR_NOT_INIT,
    REMINDER_ERR_MAX_REACHED,
    REMINDER_ERR_NOT_FOUND,
    REMINDER_ERR_DB
} reminder_status_t;

reminder_status_t reminder_init(const reminder_config_t *config);
reminder_status_t reminder_add(const reminder_t *r);
reminder_status_t reminder_remove(uint32_t reminder_id);
reminder_status_t reminder_update(uint32_t id, const reminder_t *r);
reminder_status_t reminder_acknowledge(uint32_t reminder_id);
reminder_status_t reminder_get_due(uint32_t max_count,
                                     reminder_t *out_reminders,
                                     uint32_t *out_count);
reminder_status_t reminder_get_all(uint32_t *out_count,
                                    reminder_t *out_reminders);
reminder_status_t reminder_deinit(void);

#endif /* REMINDER_MANAGER_H */
