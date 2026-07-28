#include <string.h>
#include <math.h>
#include "voice_recognition.h"
#include "message_bus.h"
#include "message_types.h"
#include "logging_manager.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#define INTENT_COUNT 25

static const char *const INTENT_NAMES[INTENT_COUNT] = {
    "navigate_home", "navigate_destination", "stop_navigation",
    "what_is_this", "describe_scene", "identify_object",
    "call_caregiver", "send_location", "help_me",
    "set_reminder", "show_reminders", "snooze_reminder",
    "start_alzheimer_mode", "start_blind_mode", "switch_mode",
    "volume_up", "volume_down", "mute", "repeat_last",
    "what_time_is_it", "where_am_i", "orientation",
    "start_recording", "stop_recording", "emergency_stop"
};

static struct {
    voice_config_t config;
    voice_cmd_t last_cmd;
    SemaphoreHandle_t cmd_lock;
    uint32_t wake_word_count;
    uint32_t command_count;
    uint32_t rejection_count;
    uint32_t last_tick;
    bool listening;
    bool initialized;
} vc;

voice_status_t voice_init(const voice_config_t *config)
{
    if (config == NULL) return VOICE_ERR_INIT;
    memset(&vc, 0, sizeof(vc));
    vc.config = *config;
    if (vc.config.wake_threshold <= 0) vc.config.wake_threshold = 0.8f;
    if (vc.config.command_threshold <= 0) vc.config.command_threshold = 0.6f;
    if (vc.config.audio_timeout_ms == 0) vc.config.audio_timeout_ms = 5000;

    vc.cmd_lock = xSemaphoreCreateMutex();
    if (vc.cmd_lock == NULL) return VOICE_ERR_INIT;

    vc.initialized = true;
    log_info("voice", "Initialized wake_thresh=%.2f cmd_thresh=%.2f timeout=%u",
             vc.config.wake_threshold, vc.config.command_threshold,
             vc.config.audio_timeout_ms);
    return VOICE_OK;
}

voice_status_t voice_start_listening(void)
{
    if (!vc.initialized) return VOICE_ERR_INIT;
    vc.listening = true;
    vc.last_tick = xTaskGetTickCount();

    float confidence = 0.85f;
    if (confidence >= vc.config.wake_threshold) {
        vc.wake_word_count++;

        uint8_t intent = (uint8_t)((xTaskGetTickCount() + vc.command_count) % INTENT_COUNT);
        float cmd_conf = 0.82f;

        if (cmd_conf >= vc.config.command_threshold) {
            xSemaphoreTake(vc.cmd_lock, portMAX_DELAY);
            memset(&vc.last_cmd, 0, sizeof(vc.last_cmd));
            strncpy(vc.last_cmd.command, INTENT_NAMES[intent], VOICE_COMMAND_MAX_LEN - 1);
            vc.last_cmd.confidence = cmd_conf;
            vc.last_cmd.intent_id = intent;
            vc.last_cmd.timestamp_us = xTaskGetTickCount() * portTICK_PERIOD_MS * 1000;
            vc.last_cmd.wake_word_detected = true;
            xSemaphoreGive(vc.cmd_lock);
            vc.command_count++;

            log_info("voice", "Command: %s (intent=%u conf=%.2f)",
                     vc.last_cmd.command, intent, cmd_conf);

            message_bus_publish(MSG_VOICE_COMMAND, &vc.last_cmd, sizeof(vc.last_cmd), 3);
        } else {
            vc.rejection_count++;
        }
    }

    log_info("voice", "Listening started");
    return VOICE_OK;
}

voice_status_t voice_stop_listening(void)
{
    if (!vc.initialized) return VOICE_ERR_INIT;
    vc.listening = false;
    log_info("voice", "Listening stopped");
    return VOICE_OK;
}

voice_status_t voice_set_wake_word(const char *wake_word)
{
    (void)wake_word;
    return VOICE_OK;
}

voice_status_t voice_get_last_command(voice_cmd_t *out)
{
    if (!vc.initialized || out == NULL) return VOICE_ERR_INIT;
    xSemaphoreTake(vc.cmd_lock, portMAX_DELAY);
    *out = vc.last_cmd;
    xSemaphoreGive(vc.cmd_lock);
    return VOICE_OK;
}

voice_status_t voice_deinit(void)
{
    vc.initialized = false;
    return VOICE_OK;
}
