#include <string.h>
#include <math.h>
#include "reminder_manager.h"
#include "message_bus.h"
#include "message_types.h"
#include "logging_manager.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

static struct {
    reminder_config_t config;
    reminder_t reminders[MAX_REMINDERS];
    uint32_t count;
    SemaphoreHandle_t lock;
    uint32_t last_check_tick;
    uint32_t triggered_count;
    uint32_t acknowledged_count;
    uint32_t next_id;
    bool initialized;
} rm;

static uint32_t now_s(void)
{
    return (xTaskGetTickCount() * portTICK_PERIOD_MS) / 1000;
}

static bool is_location_triggered(const reminder_t *r, double lat, double lon)
{
    if (r->trigger != kReminderLocation && r->trigger != kReminderBoth) return false;
    float dlat_rad = (float)(lat - r->latitude) * 3.14159265f / 180.0f;
    float dlon_rad = (float)(lon - r->longitude) * 3.14159265f / 180.0f;
    float lat_avg = (float)(lat + r->latitude) * 3.14159265f / 360.0f;
    float dx = dlon_rad * 6371000.0f * cosf(lat_avg);
    float dy = dlat_rad * 6371000.0f;
    float dist = sqrtf(dx * dx + dy * dy);
    return dist <= r->radius_m;
}

static bool is_time_triggered(const reminder_t *r)
{
    if (r->trigger != kReminderTime && r->trigger != kReminderBoth) return false;
    uint32_t current = now_s();
    if (r->start_timestamp == 0) return false;
    if (r->end_timestamp > 0 && current > r->end_timestamp) return false;
    if (current < r->start_timestamp && r->start_timestamp - current <= rm.config.advance_warning_s) return true;
    if (current >= r->start_timestamp) {
        if (r->repeat == kRepeatNone) return true;
        uint32_t elapsed = current - r->start_timestamp;
        uint32_t period = r->repeat_interval_s;
        if (period == 0) {
            switch (r->repeat) {
                case kRepeatDaily:   period = 86400; break;
                case kRepeatWeekly:  period = 604800; break;
                case kRepeatMonthly: period = 2592000; break;
                default: return false;
            }
        }
        if (period > 0 && elapsed >= period && (elapsed % period) < rm.config.check_interval_ms / 1000 + 1) {
            return true;
        }
    }
    return false;
}

static void check_due(void)
{
    uint32_t current_ts = now_s();
    for (uint32_t i = 0; i < rm.count; i++) {
        reminder_t *r = &rm.reminders[i];
        if (!r->active || r->acknowledged) continue;

        bool triggered = false;
        if (r->trigger == kReminderTime || r->trigger == kReminderBoth) {
            triggered = is_time_triggered(r);
        }
        if (!triggered && (r->trigger == kReminderLocation || r->trigger == kReminderBoth)) {
            if (rm.config.enable_location_reminders && (r->latitude != 0.0 || r->longitude != 0.0)) {
                triggered = is_location_triggered(r, r->latitude, r->longitude);
            }
        }

        if (triggered && current_ts - r->last_triggered > 60) {
            r->last_triggered = current_ts;
            r->trigger_count++;
            rm.triggered_count++;

            log_info("reminder", "Due: %s (id=%lu repeat=%d count=%lu)",
                     r->text, (unsigned long)r->reminder_id, r->repeat,
                     (unsigned long)r->trigger_count);

            message_bus_publish(MSG_REMINDER, r, sizeof(*r), 1);

            if (r->repeat == kRepeatNone) {
                r->active = false;
            }
        }
    }
}

reminder_status_t reminder_init(const reminder_config_t *config)
{
    if (config == NULL) return REMINDER_ERR_NOT_INIT;
    memset(&rm, 0, sizeof(rm));
    rm.config = *config;
    if (rm.config.check_interval_ms == 0) rm.config.check_interval_ms = 1000;
    if (rm.config.advance_warning_s == 0) rm.config.advance_warning_s = 30;
    if (rm.config.max_reminders == 0) rm.config.max_reminders = MAX_REMINDERS;

    rm.lock = xSemaphoreCreateMutex();
    if (rm.lock == NULL) return REMINDER_ERR_NOT_INIT;
    rm.next_id = 1;

    rm.initialized = true;
    log_info("reminder", "Initialized interval=%u advance=%u max=%u",
             rm.config.check_interval_ms, rm.config.advance_warning_s,
             rm.config.max_reminders);
    return REMINDER_OK;
}

reminder_status_t reminder_add(const reminder_t *r)
{
    if (!rm.initialized || r == NULL) return REMINDER_ERR_NOT_INIT;
    xSemaphoreTake(rm.lock, portMAX_DELAY);
    if (rm.count >= rm.config.max_reminders) {
        xSemaphoreGive(rm.lock);
        return REMINDER_ERR_MAX_REACHED;
    }
    reminder_t *slot = &rm.reminders[rm.count];
    *slot = *r;
    slot->reminder_id = rm.next_id++;
    slot->acknowledged = false;
    slot->active = true;
    slot->last_triggered = 0;
    slot->trigger_count = 0;
    rm.count++;
    log_info("reminder", "Added id=%lu: %s", (unsigned long)slot->reminder_id, r->text);
    xSemaphoreGive(rm.lock);
    return REMINDER_OK;
}

reminder_status_t reminder_remove(uint32_t reminder_id)
{
    if (!rm.initialized) return REMINDER_ERR_NOT_INIT;
    xSemaphoreTake(rm.lock, portMAX_DELAY);
    for (uint32_t i = 0; i < rm.count; i++) {
        if (rm.reminders[i].reminder_id == reminder_id) {
            rm.reminders[i] = rm.reminders[--rm.count];
            log_info("reminder", "Removed id=%lu", (unsigned long)reminder_id);
            xSemaphoreGive(rm.lock);
            return REMINDER_OK;
        }
    }
    xSemaphoreGive(rm.lock);
    return REMINDER_ERR_NOT_FOUND;
}

reminder_status_t reminder_update(uint32_t id, const reminder_t *r)
{
    if (!rm.initialized || r == NULL) return REMINDER_ERR_NOT_INIT;
    xSemaphoreTake(rm.lock, portMAX_DELAY);
    for (uint32_t i = 0; i < rm.count; i++) {
        if (rm.reminders[i].reminder_id == id) {
            rm.reminders[i] = *r;
            rm.reminders[i].reminder_id = id;
            log_info("reminder", "Updated id=%lu", (unsigned long)id);
            xSemaphoreGive(rm.lock);
            return REMINDER_OK;
        }
    }
    xSemaphoreGive(rm.lock);
    return REMINDER_ERR_NOT_FOUND;
}

reminder_status_t reminder_acknowledge(uint32_t reminder_id)
{
    if (!rm.initialized) return REMINDER_ERR_NOT_INIT;
    xSemaphoreTake(rm.lock, portMAX_DELAY);
    for (uint32_t i = 0; i < rm.count; i++) {
        if (rm.reminders[i].reminder_id == reminder_id) {
            rm.reminders[i].acknowledged = true;
            if (rm.reminders[i].repeat != kRepeatNone) {
                rm.reminders[i].start_timestamp = now_s() + rm.reminders[i].repeat_interval_s;
                rm.reminders[i].acknowledged = false;
            }
            rm.acknowledged_count++;
            log_info("reminder", "Acknowledged id=%lu", (unsigned long)reminder_id);
            xSemaphoreGive(rm.lock);
            return REMINDER_OK;
        }
    }
    xSemaphoreGive(rm.lock);
    return REMINDER_ERR_NOT_FOUND;
}

reminder_status_t reminder_get_due(uint32_t max_count,
                                    reminder_t *out_reminders,
                                    uint32_t *out_count)
{
    if (!rm.initialized) return REMINDER_ERR_NOT_INIT;
    check_due();

    xSemaphoreTake(rm.lock, portMAX_DELAY);
    uint32_t written = 0;
    for (uint32_t i = 0; i < rm.count && written < max_count; i++) {
        if (rm.reminders[i].active && !rm.reminders[i].acknowledged &&
            now_s() - rm.reminders[i].last_triggered < 300) {
            out_reminders[written++] = rm.reminders[i];
        }
    }
    if (out_count) *out_count = written;
    xSemaphoreGive(rm.lock);
    return REMINDER_OK;
}

reminder_status_t reminder_get_all(uint32_t *out_count,
                                    reminder_t *out_reminders)
{
    if (!rm.initialized) return REMINDER_ERR_NOT_INIT;
    xSemaphoreTake(rm.lock, portMAX_DELAY);
    if (out_reminders) {
        for (uint32_t i = 0; i < rm.count; i++) {
            out_reminders[i] = rm.reminders[i];
        }
    }
    if (out_count) *out_count = rm.count;
    xSemaphoreGive(rm.lock);
    return REMINDER_OK;
}

reminder_status_t reminder_deinit(void)
{
    rm.initialized = false;
    return REMINDER_OK;
}
