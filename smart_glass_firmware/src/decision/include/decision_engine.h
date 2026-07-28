#ifndef DECISION_ENGINE_H
#define DECISION_ENGINE_H

#include <stdint.h>
#include <stdbool.h>

#define DEC_ENG_EVENT_QUEUE_SIZE 16
#define DEC_ENG_SPEECH_MAX_LEN 256

typedef enum {
    kEventHazard,
    kEventEmergency,
    kEventNavigation,
    kEventReminder,
    kEventMemoryHint,
    kEventVoiceCommand,
    kEventContextUpdate
} event_type_t;

typedef enum {
    kActionSpeak,
    kActionNavigate,
    kActionHazardAlert,
    kActionEmergencyAlert,
    kActionReminder,
    kActionMemoryHint,
    kActionMute,
    kActionSystemCommand
} action_type_t;

typedef struct {
    action_type_t action;
    uint8_t priority;
    char speech_text[DEC_ENG_SPEECH_MAX_LEN];
    float nav_bearing;
    float nav_distance;
    uint8_t nav_reason;
    uint32_t duration_ms;
} action_t;

typedef struct {
    uint8_t max_speech_per_minute;
    bool mute_all;
    uint8_t min_priority_for_speech;
    uint32_t event_timeout_ms;
} dec_eng_config_t;

typedef enum {
    DEC_ENG_OK = 0,
    DEC_ENG_ERR_NOT_INIT,
    DEC_ENG_ERR_QUEUE_FULL,
    DEC_ENG_ERR_UNKNOWN_ACTION
} dec_eng_status_t;

dec_eng_status_t dec_eng_init(const dec_eng_config_t *config);
dec_eng_status_t dec_eng_process(void);
dec_eng_status_t dec_eng_get_last_decision(action_t *out);
dec_eng_status_t dec_eng_set_priority(event_type_t event, uint8_t priority);
dec_eng_status_t dec_eng_mute(bool mute);
dec_eng_status_t dec_eng_get_stats(uint32_t *decisions_made,
                                    uint32_t *speech_outputs);
dec_eng_status_t dec_eng_deinit(void);

#endif /* DECISION_ENGINE_H */
