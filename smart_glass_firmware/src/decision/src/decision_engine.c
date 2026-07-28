#include <string.h>
#include "decision_engine.h"
#include "message_bus.h"
#include "message_types.h"
#include "logging_manager.h"
#include "task_manager.h"
#include "navigation_engine.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

typedef enum {
    kStateIdle,
    kStateCollect,
    kStatePrioritize,
    kStateSelect,
    kStateExecute
} dec_eng_state_t;

typedef struct {
    event_type_t e_type;
    uint8_t priority;
    uint32_t timestamp_us;
    uint8_t payload[MSG_MAX_PAYLOAD_BYTES];
    size_t payload_size;
    bool used;
} event_entry_t;

typedef struct {
    event_entry_t events[DEC_ENG_EVENT_QUEUE_SIZE];
    uint8_t count;
    SemaphoreHandle_t lock;
} event_queue_t;

static struct {
    dec_eng_config_t config;
    action_t last_action;
    SemaphoreHandle_t action_lock;
    uint32_t decision_count;
    uint32_t speech_count;
    uint32_t nav_override_count;
    uint32_t last_speech_tick;
    uint32_t speech_this_minute;
    uint32_t minute_start_tick;
    bool muted;
    bool initialized;
    event_queue_t event_q;
    uint8_t event_priorities[7];
} eng;

static const uint8_t default_priorities[7] = {
    [kEventHazard]       = 7,
    [kEventEmergency]    = 7,
    [kEventVoiceCommand] = 5,
    [kEventNavigation]   = 4,
    [kEventReminder]     = 3,
    [kEventMemoryHint]   = 2,
    [kEventContextUpdate]= 1,
};

static uint32_t now_us(void)
{
    return xTaskGetTickCount() * portTICK_PERIOD_MS * 1000;
}

static uint32_t now_ms(void)
{
    return xTaskGetTickCount() * portTICK_PERIOD_MS;
}

static bool event_is_stale(const event_entry_t *e)
{
    if (eng.config.event_timeout_ms == 0) return false;
    uint32_t age_ms = (now_us() - e->timestamp_us) / 1000;
    return age_ms > eng.config.event_timeout_ms;
}

static dec_eng_status_t push_event(event_type_t type, const void *data,
                                    size_t data_size, uint8_t priority)
{
    if (!eng.initialized) return DEC_ENG_ERR_NOT_INIT;
    if (data_size > MSG_MAX_PAYLOAD_BYTES) return DEC_ENG_ERR_QUEUE_FULL;

    xSemaphoreTake(eng.event_q.lock, portMAX_DELAY);

    dec_eng_status_t status = DEC_ENG_OK;
    event_entry_t *slot = NULL;
    int victim_idx = -1;
    uint8_t victim_prio = 255;

    for (int i = 0; i < DEC_ENG_EVENT_QUEUE_SIZE; i++) {
        if (!eng.event_q.events[i].used) {
            slot = &eng.event_q.events[i];
            break;
        }
        if (eng.event_q.events[i].priority < victim_prio) {
            victim_prio = eng.event_q.events[i].priority;
            victim_idx = i;
        }
    }

    if (slot == NULL) {
        if (victim_idx >= 0 && priority > victim_prio) {
            slot = &eng.event_q.events[victim_idx];
            log_warn("dec_eng", "Dropped priority %d event for %d",
                     victim_prio, priority);
        } else {
            log_warn("dec_eng", "Queue full, dropped event type %d", type);
            xSemaphoreGive(eng.event_q.lock);
            return DEC_ENG_OK;
        }
    }

    slot->e_type = type;
    slot->priority = priority;
    slot->timestamp_us = now_us();
    slot->payload_size = data_size;
    slot->used = true;
    if (data && data_size > 0) {
        memcpy(slot->payload, data, data_size);
    }
    eng.event_q.count++;

    xSemaphoreGive(eng.event_q.lock);
    return status;
}

static dec_eng_status_t pop_highest_priority(event_entry_t *out)
{
    xSemaphoreTake(eng.event_q.lock, portMAX_DELAY);

    int best = -1;
    uint8_t best_prio = 0;
    uint32_t best_age = 0;

    for (int i = 0; i < DEC_ENG_EVENT_QUEUE_SIZE; i++) {
        if (!eng.event_q.events[i].used) continue;
        if (event_is_stale(&eng.event_q.events[i])) {
            eng.event_q.events[i].used = false;
            eng.event_q.count--;
            log_debug("dec_eng", "Discarded stale event type %d",
                      eng.event_q.events[i].e_type);
            continue;
        }
        uint32_t age = now_us() - eng.event_q.events[i].timestamp_us;
        if (eng.event_q.events[i].priority > best_prio ||
            (eng.event_q.events[i].priority == best_prio && age > best_age)) {
            best = i;
            best_prio = eng.event_q.events[i].priority;
            best_age = age;
        }
    }

    if (best < 0) {
        xSemaphoreGive(eng.event_q.lock);
        return DEC_ENG_ERR_QUEUE_FULL;
    }

    *out = eng.event_q.events[best];
    eng.event_q.events[best].used = false;
    eng.event_q.count--;

    xSemaphoreGive(eng.event_q.lock);
    return DEC_ENG_OK;
}

static bool is_speech_throttled(void)
{
    uint32_t tick_now = now_ms();
    uint32_t minute_ms = 60000;

    if (tick_now - eng.minute_start_tick >= minute_ms) {
        eng.minute_start_tick = tick_now;
        eng.speech_this_minute = 0;
    }

    if (eng.speech_this_minute >= eng.config.max_speech_per_minute) {
        return true;
    }
    return false;
}

static void select_and_execute_action(event_entry_t *event)
{
    if (event == NULL) return;

    action_t action;
    memset(&action, 0, sizeof(action));

    switch (event->e_type) {
        case kEventHazard:
            action.action = kActionHazardAlert;
            action.priority = event->priority;
            break;
        case kEventEmergency:
            action.action = kActionEmergencyAlert;
            action.priority = event->priority;
            break;
        case kEventVoiceCommand:
            action.action = kActionSystemCommand;
            action.priority = event->priority;
            break;
        case kEventNavigation:
            action.action = kActionNavigate;
            action.priority = event->priority;
            break;
        case kEventReminder:
            action.action = kActionReminder;
            action.priority = event->priority;
            break;
        case kEventMemoryHint:
            action.action = kActionMemoryHint;
            action.priority = event->priority;
            break;
        case kEventContextUpdate:
        default:
            action.action = kActionSpeak;
            action.priority = event->priority;
            break;
    }

    if (eng.muted) {
        if (action.action == kActionSpeak || action.action == kActionHazardAlert ||
            action.action == kActionEmergencyAlert || action.action == kActionReminder) {
            log_debug("dec_eng", "Muted, suppressed action %d", action.action);
            return;
        }
    }

    if ((action.action == kActionSpeak || action.action == kActionHazardAlert ||
         action.action == kActionEmergencyAlert || action.action == kActionReminder ||
         action.action == kActionMemoryHint) && is_speech_throttled()) {
        log_debug("dec_eng", "Speech throttled, suppressed action %d", action.action);
        return;
    }

    bool emitted_speech = false;
    bool emitted_nav = false;

    switch (action.action) {
        case kActionHazardAlert:
        case kActionEmergencyAlert:
        case kActionReminder:
        case kActionMemoryHint:
        case kActionSpeak: {
            speech_req_t req;
            memset(&req, 0, sizeof(req));
            req.severity = action.priority;
            if (event->payload_size > 0 && event->payload_size <= sizeof(req.speech_text)) {
                memcpy(req.speech_text, event->payload, event->payload_size);
            }
            req.duration_ms = 3000;
            message_bus_publish(MSG_SPEECH_REQ, &req, sizeof(req), action.priority);
            emitted_speech = true;
            eng.speech_this_minute++;
            eng.last_speech_tick = now_ms();
            break;
        }
        case kActionNavigate: {
            nav_override_t nav;
            memset(&nav, 0, sizeof(nav));
            nav.bearing = 0;
            nav.distance = 0;
            nav.reason = 0;
            message_bus_publish(MSG_NAV_OVERRIDE, &nav, sizeof(nav), action.priority);
            emitted_nav = true;
            break;
        }
        case kActionSystemCommand:
            log_info("dec_eng", "System command action (voice)");
            break;
        case kActionMute:
            eng.muted = !eng.muted;
            log_info("dec_eng", "Mute toggled: %d", eng.muted);
            break;
        default:
            log_error("dec_eng", "Unknown action %d", action.action);
            break;
    }

    xSemaphoreTake(eng.action_lock, portMAX_DELAY);
    eng.last_action = action;
    eng.decision_count++;
    if (emitted_speech) eng.speech_count++;
    if (emitted_nav) eng.nav_override_count++;
    xSemaphoreGive(eng.action_lock);

    log_info("dec_eng", "Decision: event=%d action=%d prio=%d",
             event->e_type, action.action, action.priority);
}

static void msg_callback(const bus_message_t *msg, void *user_ctx)
{
    (void)user_ctx;
    event_type_t e_type;
    uint8_t priority;

    switch (msg->type) {
        case MSG_HAZARD_EVENT:
            e_type = kEventHazard;
            priority = eng.event_priorities[kEventHazard];
            break;
        case MSG_EMERGENCY:
            e_type = kEventEmergency;
            priority = eng.event_priorities[kEventEmergency];
            break;
        case MSG_VOICE_COMMAND:
            e_type = kEventVoiceCommand;
            priority = eng.event_priorities[kEventVoiceCommand];
            break;
        case MSG_CONTEXT:
            e_type = kEventContextUpdate;
            priority = eng.event_priorities[kEventContextUpdate];
            break;
        case MSG_REMINDER:
            e_type = kEventReminder;
            priority = eng.event_priorities[kEventReminder];
            break;
        case MSG_MEMORY_HINT:
            e_type = kEventMemoryHint;
            priority = eng.event_priorities[kEventMemoryHint];
            break;
        default:
            return;
    }

    push_event(e_type, msg->payload, msg->payload_size, priority);
}

dec_eng_status_t dec_eng_init(const dec_eng_config_t *config)
{
    if (config == NULL) return DEC_ENG_ERR_NOT_INIT;

    memset(&eng, 0, sizeof(eng));
    eng.config = *config;
    eng.event_q.lock = xSemaphoreCreateMutex();
    eng.action_lock = xSemaphoreCreateMutex();
    if (eng.event_q.lock == NULL || eng.action_lock == NULL) {
        return DEC_ENG_ERR_NOT_INIT;
    }
    memcpy(eng.event_priorities, default_priorities, sizeof(default_priorities));

    message_bus_subscribe(MSG_HAZARD_EVENT, msg_callback, NULL);
    message_bus_subscribe(MSG_EMERGENCY, msg_callback, NULL);
    message_bus_subscribe(MSG_VOICE_COMMAND, msg_callback, NULL);
    message_bus_subscribe(MSG_CONTEXT, msg_callback, NULL);
    message_bus_subscribe(MSG_REMINDER, msg_callback, NULL);
    message_bus_subscribe(MSG_MEMORY_HINT, msg_callback, NULL);

    eng.initialized = true;
    log_info("dec_eng", "Initialized, max_speech=%d/min", config->max_speech_per_minute);
    return DEC_ENG_OK;
}

dec_eng_status_t dec_eng_process(void)
{
    if (!eng.initialized) return DEC_ENG_ERR_NOT_INIT;

    event_entry_t event;
    dec_eng_status_t st = pop_highest_priority(&event);
    if (st != DEC_ENG_OK) return st;

    select_and_execute_action(&event);
    return DEC_ENG_OK;
}

dec_eng_status_t dec_eng_get_last_decision(action_t *out)
{
    if (!eng.initialized || out == NULL) return DEC_ENG_ERR_NOT_INIT;
    xSemaphoreTake(eng.action_lock, portMAX_DELAY);
    *out = eng.last_action;
    xSemaphoreGive(eng.action_lock);
    return DEC_ENG_OK;
}

dec_eng_status_t dec_eng_set_priority(event_type_t event, uint8_t priority)
{
    if (!eng.initialized) return DEC_ENG_ERR_NOT_INIT;
    if (event > kEventContextUpdate) return DEC_ENG_ERR_UNKNOWN_ACTION;
    eng.event_priorities[event] = priority;
    return DEC_ENG_OK;
}

dec_eng_status_t dec_eng_mute(bool mute)
{
    if (!eng.initialized) return DEC_ENG_ERR_NOT_INIT;
    eng.muted = mute;
    log_info("dec_eng", "Mute set to %d", mute);
    return DEC_ENG_OK;
}

dec_eng_status_t dec_eng_get_stats(uint32_t *decisions_made, uint32_t *speech_outputs)
{
    if (!eng.initialized) return DEC_ENG_ERR_NOT_INIT;
    xSemaphoreTake(eng.action_lock, portMAX_DELAY);
    if (decisions_made) *decisions_made = eng.decision_count;
    if (speech_outputs) *speech_outputs = eng.speech_count;
    xSemaphoreGive(eng.action_lock);
    return DEC_ENG_OK;
}

dec_eng_status_t dec_eng_deinit(void)
{
    eng.initialized = false;
    return DEC_ENG_OK;
}

void dec_eng_task_entry(void *params)
{
    (void)params;
    TickType_t last_wake = xTaskGetTickCount();

    for (;;) {
        dec_eng_process();
        task_manager_feed_watchdog(TASK_DECISION_ENGINE);
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(66));
    }
}
